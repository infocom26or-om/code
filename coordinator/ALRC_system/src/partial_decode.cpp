#include "partial_decode.hpp"
#include <unordered_set>
#include <unordered_map>
#include <chrono>
#include <iostream>
#include <reed_sol.h>
#include <galois.h>
#include <jerasure.h> 

bool PartialDecoder::recover_and_set(int failed_block_id,
                                     const Placement& placement,
                                     MemcachedClient& client,
                                     int k, int l, int g, int block_size,
                                     std::unordered_map<int, std::string>& recovered_data,
                                     double& repair_time_ms) {
    auto start = std::chrono::steady_clock::now();

    auto failed_entry = placement.table.at(failed_block_id);
    int failed_zone = failed_entry.zone_id;
    int failed_group = failed_entry.group_id;

    // Step 1: mark all blocks in the same zone as failed
    std::unordered_set<int> failed_blocks;
    for (const auto& [id, entry] : placement.table) {
        if (entry.zone_id == failed_zone) {
            failed_blocks.insert(id);
        }
    }

    // Step 2: try local repair with rack-level grouping
    std::vector<int> group_blocks;
    int missing_count = 0;
    for (const auto& [id, entry] : placement.table) {
        if (entry.group_id == failed_group) {
            if (failed_blocks.count(id)) {
                ++missing_count;
            } else {
                group_blocks.push_back(id);
            }
        }
    }

    bool can_local_repair = (missing_count == 1);

    if (can_local_repair) {
        // Step 2.1: group remaining blocks by rack and XOR them per rack
        std::unordered_map<int, std::vector<int>> rack_to_blocks;
        for (int id : group_blocks) {
            int rack_id = placement.table.at(id).rack_id;
            rack_to_blocks[rack_id].push_back(id);
        }

        std::vector<char> repaired(block_size, 0);
        for (const auto& [rack_id, block_ids] : rack_to_blocks) {
            std::vector<char> rack_combined(block_size, 0);
            for (int id : block_ids) {
                auto& entry = placement.table.at(id);
                std::string val;
                if (client.get(entry.server_ip, entry.port, "block_" + std::to_string(id), val)) {
                    for (int i = 0; i < block_size; ++i) rack_combined[i] ^= val[i];
                }
            }
            //以上生成完rack_combined,可以transfer模拟跨rack了
            for (int i = 0; i < block_size; ++i) repaired[i] ^= rack_combined[i];
        }

        std::string recovered(repaired.begin(), repaired.end());
        client.set(failed_entry.server_ip, failed_entry.port,
                   "block_" + std::to_string(failed_block_id), recovered);
        recovered_data[failed_block_id] = recovered;

        auto end = std::chrono::steady_clock::now();
        repair_time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        return true;
    }

    // Step 3: fallback to global repair
    int total_blocks = k + l + g;
    int* matrix = reed_sol_vandermonde_coding_matrix(k, l + g, 8);

    char** data_ptrs = new char*[k];
    char** coding_ptrs = new char*[l + g];
    for (int i = 0; i < k; ++i) data_ptrs[i] = new char[block_size];
    for (int i = 0; i < l + g; ++i) coding_ptrs[i] = new char[block_size];

    int* erasures = new int[failed_blocks.size() + 1];
    int idx = 0;
    for (int bid : failed_blocks) erasures[idx++] = bid;
    erasures[idx] = -1;

    std::unordered_set<int> required_blocks;
    for (int i = 0; i < total_blocks; ++i) {
        if (failed_blocks.count(i)) continue;
        for (int j = 0; j < idx; ++j) {
            if (matrix[erasures[j] * total_blocks + i] != 0) {
                required_blocks.insert(i);
                break;
            }
        }
    }

    // Step 4: rack-level combination of required blocks
    std::unordered_map<int, std::vector<int>> rack_to_blocks;
    for (int id : required_blocks) {
        int rack_id = placement.table.at(id).rack_id;
        rack_to_blocks[rack_id].push_back(id);
    }

    std::unordered_map<int, std::string> available_data;
    for (const auto& [rack_id, block_ids] : rack_to_blocks) {
        std::vector<char> rack_combined(block_size, 0);
        for (int id : block_ids) {
            auto& entry = placement.table.at(id);
            std::string val;
            if (client.get(entry.server_ip, entry.port, "block_" + std::to_string(id), val)) {
                for (int i = 0; i < block_size; ++i) rack_combined[i] ^= val[i];
            } else {
                std::cerr << "[ERROR] Missing block: " << id << "\n";
                return false;
            }
        }
        // Use synthetic block ID to avoid conflict with real IDs
        int synthetic_id = 10000 + rack_id;
        available_data[synthetic_id] = std::string(rack_combined.begin(), rack_combined.end());
    }

    for (int i = 0; i < total_blocks; ++i) {
        char* buf = (i < k) ? data_ptrs[i] : coding_ptrs[i - k];
        if (failed_blocks.count(i)) {
            memset(buf, 0, block_size);
        } else if (available_data.count(i)) {
            memcpy(buf, available_data[i].data(), block_size);
        } else {
            memset(buf, 0, block_size);
        }
    }

    int success = jerasure_matrix_decode(k, l + g, 8, matrix, 0,
                                         erasures, data_ptrs, coding_ptrs, block_size);
    if (success < 0) {
        std::cerr << "Decoding failed." << std::endl;
        return false;
    }

    for (int bid : failed_blocks) {
        std::string recovered;
        if (bid < k) recovered.assign(data_ptrs[bid], block_size);
        else recovered.assign(coding_ptrs[bid - k], block_size);

        auto entry = placement.table.at(bid);
        client.set(entry.server_ip, entry.port, "block_" + std::to_string(bid), recovered);
        recovered_data[bid] = recovered;
    }

    for (int i = 0; i < k; ++i) delete[] data_ptrs[i];
    for (int i = 0; i < l + g; ++i) delete[] coding_ptrs[i];
    delete[] data_ptrs;
    delete[] coding_ptrs;
    delete[] erasures;
    delete[] matrix;

    auto end = std::chrono::steady_clock::now();
    repair_time_ms = std::chrono::duration<double, std::milli>(end - start).count();
    return true;
}
