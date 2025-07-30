// decode.cpp rack
#include "decode.hpp"
#include <iostream>
#include <chrono>
#include <cstring>
#include "gf256_solver.hpp"
#include <algorithm>  // for std::sort


bool Decode::try_local_repair_for_group(int group_id, int k, int l, int failed_rack_id, int block_size,
                                const std::unordered_set<int>& failed_blocks,
                                const Placement& placement,
                                MemcachedClient& client,
                                const std::vector<std::vector<int>>& encode_matrix,
                                std::unordered_map<int, std::string>& recovered_data,
                                std::unordered_set<int>& repaired_ids,
                                double& repair_time_ms) {
                                

    int group_size = k / l;
    int data_start = group_id * group_size;
    int data_end = data_start + group_size;
    int local_parity_id = k + group_id;  // 局部校验块编号
    // 找出组内丢失data块
    std::vector<int> lost_blocks;
    for (int i = data_start; i < data_end; ++i) {
        if (failed_blocks.count(i)) lost_blocks.push_back(i);
    }
    if (lost_blocks.empty()) return false; // 无丢失块无需恢复

    // 组内所有块（数据块 + 局部校验块）
    int total_local_blocks = group_size + 1;

    if (lost_blocks.size() > 1 || failed_blocks.count(local_parity_id)) return false;

    int lost_id = lost_blocks[0];
    
    auto start = std::chrono::steady_clock::now();		//time start
    
    const auto& parity_entry = placement.table.at(local_parity_id);
    std::string parity_val;
    if (!client.get(parity_entry.server_ip, parity_entry.port, "block_" + std::to_string(local_parity_id), parity_val)) {
        std::cerr << "[Local] Failed to get parity block_" << local_parity_id << std::endl;
        return false;
    }

    ////////////////////cross_rack/////////////////////////
    if(parity_entry.rack_id != failed_rack_id){
        std::string temp_key = "block_cross_rack" + std::to_string(local_parity_id);
        bool success = client.set("10.0.0.122", 11211, temp_key, parity_val);
        if (!success) {
            std::cerr << "Failed to set block " << " to server " << std::endl;
            return false;
        }
        std::string temp_val;
        if (!client.get("10.0.0.122", 11211, temp_key, temp_val)) {
            std::cerr << "Failed to get cross_rack block" << std::endl;
            return false;
        }
    /*  
        if(temp_val != parity_val) std::cout << "error!" << std::endl;   
        for (size_t i = 0; i < std::min<size_t>(temp_val.size(), 16); ++i) {
            std::cout << static_cast<int>(static_cast<uint8_t>(temp_val[i])) << " ";
        }
        std::cout << "(size=" << temp_val.size() << ")" << std::endl;
        for (size_t i = 0; i < std::min<size_t>(parity_val.size(), 16); ++i) {
            std::cout << static_cast<int>(static_cast<uint8_t>(parity_val[i])) << " ";
        }
        std::cout << "(size=" << parity_val.size() << ")" << std::endl;
    */    
    }
    ///////////////////////////////////////////////////////
    
    // 初始化 RHS = 局部校验块值
    std::vector<int> rhs(block_size, 0);
    for (int b = 0; b < block_size; ++b)
        rhs[b] = static_cast<int>(parity_val[b]);

    
    // 减去已知项 α_i·D_i
    for (int i = data_start; i < data_end; ++i) {
        if (i == lost_id) continue;
        if (failed_blocks.count(i)) return false; // 其他块也坏了，无法局部恢复

        const auto& entry = placement.table.at(i);
        std::string val;
        if (!client.get(entry.server_ip, entry.port, "block_" + std::to_string(i), val)) {
            std::cerr << "[Local] Failed to get data block_" << i << std::endl;
            return false;
        }
                
        ////////////////////cross_rack/////////////////////////
        if(entry.rack_id != failed_rack_id){
            std::string temp_key = "block_cross_rack_" + std::to_string(i);
            bool success = client.set("10.0.0.122", 11211, temp_key, val);
            if (!success) {
                std::cerr << "Failed to set block " << " to server " << std::endl;
                return false;
            }
            std::string temp_val;
            if (!client.get("10.0.0.122", 11211, temp_key, temp_val)) {
                std::cerr << "Failed to get cross_rack block" << std::endl;
                return false;
            }        
        }
        ///////////////////////////////////////////////////////

        int coef = encode_matrix[group_id][i]; // 系数来自局部校验块第 group_id 行
        for (int b = 0; b < block_size; ++b)
            rhs[b] ^= gf256_mul(coef, static_cast<int>(val[b]));
    }
/*
                // === 添加调试信息 ===
    std::cout << "!!!!repiar (" << l+1 << " x " << k << "):\n";
    for (int i = 0; i < l+1; ++i) {
        for (int j = 0; j < k; ++j) {
            std::cout << encode_matrix[i][j] << " ";
        }
        std::cout << "\n";
    }
    std::cout << "========================================\n";
*/                           
                                

    // 除以丢失块的系数（乘以逆元）
    int lost_coef = encode_matrix[group_id][lost_id];
    int inv_coef = gf256_inv(lost_coef);
    for (int b = 0; b < block_size; ++b)
        rhs[b] = gf256_mul(rhs[b], inv_coef);

    repair_time_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();     // time end
    // 写回丢失块
    const auto& lost_entry = placement.table.at(lost_id);
    std::string recovered(rhs.begin(), rhs.end());
    

/*    
    // 打印恢复后的 block 内容
    std::cout << "[Local Repair] Recovered block_" << lost_id << " : ";
    for (int i = 0; i < std::min(block_size, 16); ++i)
        std::cout << static_cast<int>(static_cast<int>(recovered[i])) << " ";
    std::cout << "(size=" << recovered.size() << ")" << std::endl;
*/    
    if (!client.set(lost_entry.server_ip, lost_entry.port, "block_" + std::to_string(lost_id), recovered)) {
        std::cerr << "[Local] Failed to set recovered block_" << lost_id << std::endl;
        return false;
    }

    recovered_data[lost_id] = recovered;
    repaired_ids.insert(lost_id);

//    std::cout << "[Local Repair] block_" << lost_id << " repaired successfully using local parity." << std::endl;
    return true;
}


bool Decode::check_and_add_row(std::vector<std::vector<int>>& mat, const std::vector<int>& new_row) {
    int n = mat.size();
    if (n == 0) {
        mat.push_back(new_row);
        return true;
    }
    // 复制原矩阵+新增行到tmp进行消元（GF(256)乘法和异或）
    std::vector<std::vector<int>> tmp = mat;
    tmp.push_back(new_row);

    int row_count = tmp.size();
    int col_count = tmp[0].size();

    // 高斯消元过程，GF(256)版本
    int rank = 0;
    for (int col = 0; col < col_count; ++col) {
        int pivot = -1;
        for (int r = rank; r < row_count; ++r) {
            if (tmp[r][col] != 0) {
                pivot = r;
                break;
            }
        }
        if (pivot == -1) continue;

        if (pivot != rank) std::swap(tmp[pivot], tmp[rank]);
        uint8_t inv = gf256_inv(tmp[rank][col]);
        for (int c = col; c < col_count; ++c)
            tmp[rank][c] = gf256_mul(tmp[rank][c], inv);

        for (int r = 0; r < row_count; ++r) {
            if (r != rank && tmp[r][col] != 0) {
                uint8_t factor = tmp[r][col];
                for (int c = col; c < col_count; ++c) {
                    tmp[r][c] ^= gf256_mul(factor, tmp[rank][c]);
                }
            }
        }
        rank++;
    }
    // 如果rank增加，说明新行线性独立
    if (rank > (int)mat.size()) {
        mat.push_back(new_row);
        return true;
    }
    return false;
}

bool Decode::construct_recovery_system(
    int k, int l, int g, int failed_rack_id, int block_size,
    const Placement& placement,
    MemcachedClient& client,
    const std::unordered_set<int>& available_block_ids,
    const std::unordered_set<int>& failed_blocks,
    const std::vector<std::vector<int>>& encode_matrix,
    std::vector<std::vector<int>>& matrix,
    std::vector<std::vector<uint8_t>>& rhs) {

    int m = l + g;        // 校验块数
    int total = k + m;    // 总块数

    // 收集所有丢失的数据块 id
    std::vector<int> all_failed;
    for (int bid : failed_blocks) {
        if (bid < k) all_failed.push_back(bid);
    }
    
    std::sort(all_failed.begin(), all_failed.end());
    int f = all_failed.size();
    if (f == 0) return true;

    matrix.clear();
    rhs.clear();

    std::vector<int> selected_equations;		// // 记录选中的全局校验方程下标（行号 eq）
    std::vector<std::vector<int>> candidate_rows;	// 记录构造出的 A 矩阵行，用于线性无关性判断
/*
    // 1. 优先选全局校验行 [l, l+g)
    for (int eq = l; eq < m; ++eq) {
        int parity_id = k + eq;
    	if (!available_block_ids.count(parity_id)) continue;
    	
        bool useful = false;
        std::vector<int> row_coef;
        for (int missing_idx = 0; missing_idx < f; ++missing_idx) {
            int missing_id = all_failed[missing_idx];
            //std::cout << "missing_id : " << missing_id << std::endl;
            row_coef.push_back(encode_matrix[eq][missing_id]);
            if (encode_matrix[eq][missing_id] != 0)
                useful = true;
        }
        if (!useful) continue;
        if (check_and_add_row(candidate_rows, row_coef)) {
            selected_equations.push_back(eq);
            if ((int)selected_equations.size() == f) break;
        }
    }

    // 2. 不足时用局部校验行补足 [0, l)
    if ((int)selected_equations.size() < f) {
        for (int eq = 0; eq < l; ++eq) {
            int parity_id = k + eq;  // 局部校验块编号
            if (!available_block_ids.count(parity_id)) continue;

            bool useful = false;
            std::vector<int> row_coef;
            for (int missing_idx = 0; missing_idx < f; ++missing_idx) {
                int missing_id = all_failed[missing_idx];   
                row_coef.push_back(encode_matrix[eq][missing_id]);
                if (encode_matrix[eq][missing_id] != 0)
                    useful = true;
            }
            if (!useful) continue;
            if (check_and_add_row(candidate_rows, row_coef)) {
                selected_equations.push_back(eq);
                if ((int)selected_equations.size() == f) break;
            }
        }
    }
*/

    // 顺序选择可用校验行 [0, l+g)，不区分局部/全局
    for (int eq = 0; eq < l + g; ++eq) {
        int parity_id = k + eq;
        if (available_block_ids.count(parity_id) == 0) continue;  // parity 块必须可用

        bool useful = false;
        std::vector<int> row_coef;
        for (int missing_idx = 0; missing_idx < f; ++missing_idx) {
            int missing_id = all_failed[missing_idx];
            row_coef.push_back(encode_matrix[eq][missing_id]);
            if (encode_matrix[eq][missing_id] != 0)
                useful = true;
        }
        if (!useful) continue;  // 若当前校验行对任何丢失块无贡献，跳过

        if (check_and_add_row(candidate_rows, row_coef)) {
            selected_equations.push_back(eq);
            if ((int)selected_equations.size() == f) break;
        }
    }

    if ((int)selected_equations.size() < f) {
        std::cerr << "[Global] Cannot find enough linearly independent equations to recover all failed blocks." << std::endl;
        return false;
    }
/*    
    for (int eq : selected_equations) {
    	std::cout << "selected_equation: " << eq << std::endl;
    }
*/
    std::unordered_map<int, std::vector<uint8_t>> available_blocks;

    for (const auto& [id, entry] : placement.table) {
        int bid = entry.block_index;
        //std::cout << "bid: " << bid << std::endl;
        //std::cout << "bid - k: " << bid - k << std::endl;

        if (bid >= 0 && bid < k) {
            if (std::find(all_failed.begin(), all_failed.end(), bid) != all_failed.end())
                continue;
        } else if (bid >= k) {
            if (std::find(selected_equations.begin(), selected_equations.end(), bid - k) == selected_equations.end())
                continue;
        } else {
            continue;  // 排除负值或非法编号（保险）
        }

        std::string val;
        if (!client.get(entry.server_ip, entry.port, "block_" + std::to_string(bid), val)) {
            std::cerr << "[Global] Missing block_" << bid << std::endl;
            return false;
         }

        ////////////////////cross_rack/////////////////////////
        if(entry.rack_id != failed_rack_id){
            std::string temp_key = "block_cross_rack_" + std::to_string(bid);
            bool success = client.set("10.0.0.122", 11211, temp_key, val);
            if (!success) {
                std::cerr << "Failed to set block " << " to server " << std::endl;
                return false;
            }
            std::string temp_val;
            if (!client.get("10.0.0.122", 11211, temp_key, temp_val)) {
                std::cerr << "Failed to get cross_rack block" << std::endl;
                return false;
            }     
        }
        ///////////////////////////////////////////////////////
                  
/*
        std::cout << "[Available] block_" << bid << ": ";
        for (size_t i = 0; i < std::min<size_t>(val.size(), 16); ++i) {
            std::cout << static_cast<int>(static_cast<uint8_t>(val[i])) << " ";
        }
        std::cout << "(size=" << val.size() << ")" << std::endl;
*/
       available_blocks[bid] = std::vector<uint8_t>(val.begin(), val.end());
    }


    // 3. 构造最终矩阵和 RHS
    matrix.assign(f, std::vector<int>(f, 0));
    rhs.assign(f, std::vector<uint8_t>(block_size, 0));


    for (int row = 0; row < f; ++row) {
        int eq = selected_equations[row];
        //std::cout << "eq : " << eq << std::endl;//global first
        const std::vector<int>& coef = encode_matrix[eq];

        int parity_id = k + eq;        
        auto parity_it = available_blocks.find(parity_id);
        if (available_block_ids.find(parity_id) == available_block_ids.end()) {
            std::cerr << "[Global] Missing parity block_" << parity_id << " when building RHS." << std::endl;
            return false;
        }


        // 正确方式：RHS = parity ⊕ sum(ci * known_data_i)
        const std::vector<uint8_t>& parity_val = parity_it->second;
        for (int b = 0; b < block_size; ++b) {
            rhs[row][b] = parity_val[b];  // 初始化为 parity 块
        }

        for (int j = 0; j < k; ++j) {
            if (failed_blocks.count(j)) continue; // skip failed
            int cj = coef[j];
            if (cj == 0) continue;

            auto it = available_blocks.find(j);
            if (it == available_blocks.end()) {
                std::cerr << "[Global] Missing data block_" << j << " when building RHS." << std::endl;
                return false;
            }
            const std::vector<uint8_t>& val = it->second;
            for (int b = 0; b < block_size; ++b) {
                rhs[row][b] ^= gf256_mul(static_cast<uint8_t>(cj), val[b]);
            }
        }


        for (int col = 0; col < f; ++col) {
            int unknown_id = all_failed[col];
            matrix[row][col] = coef[unknown_id];
           // std::cout << "unknown_id : " << unknown_id << std::endl;
           // std::cout << "col : " << col << std::endl;
        }
    }
/*
    // Debug print
    std::cout << "[Debug] A matrix:\n";
    for (const auto& row : matrix) {
        for (auto v : row) std::cout << int(v) << " ";
        std::cout << "\n";
    }

    for (int i = 0; i < rhs.size(); ++i) {
        std::cout << "[Debug] RHS[" << i << "]: ";
        for (auto b : rhs[i]) std::cout << int(b) << " ";
        std::cout << std::endl;
    }
*/

    return true;
}




bool Decode::gf256_solve(const std::vector<std::vector<int>>& A,
                 const std::vector<std::vector<uint8_t>>& B,
                 std::vector<std::vector<uint8_t>>& X) {
    return gf256_gaussian_elimination(A, B, X);
}

bool Decode::repair_with_lrc_global_solver(const std::unordered_set<int>& failed_block_ids,
                                   const Placement& placement,
                                   MemcachedClient& client,
                                   int k, int l, int g, int failed_rack_id, int block_size,
                                   const std::vector<std::vector<int>>& encode_matrix,
                                   std::unordered_map<int, std::string>& recovered_data,
                                   double& repair_time_ms) {
 //   auto start = std::chrono::steady_clock::now();
/*
    std::unordered_map<int, std::vector<uint8_t>> available_blocks;
    for (const auto& [id, entry] : placement.table) {
        int bid = entry.block_index;
        if (failed_block_ids.count(bid)) continue;

        std::string val;
        if (!client.get(entry.server_ip, entry.port, "block_" + std::to_string(bid), val)) {
            std::cerr << "[Global] Missing block_" << bid << std::endl;
            return false;
        }

        std::cout << "[Available] block_" << bid << ": ";
        for (size_t i = 0; i < std::min<size_t>(val.size(), 16); ++i) {
            std::cout << static_cast<int>(static_cast<uint8_t>(val[i])) << " ";
        }
        std::cout << "(size=" << val.size() << ")" << std::endl;

        available_blocks[bid] = std::vector<uint8_t>(val.begin(), val.end());
    }
*/
    // 仅记录可用块的编号（不从 Memcached 获取数据）
    std::unordered_set<int> available_block_ids;
    for (const auto& [id, entry] : placement.table) {
        int bid = entry.block_index;
        if (failed_block_ids.count(bid)) continue;
        available_block_ids.insert(bid);
//        std::cout << "[Available] block_id: " << bid << std::endl;
    }
    

    auto start = std::chrono::steady_clock::now();		//time start
    
    std::vector<std::vector<int>> matrix;
    std::vector<std::vector<uint8_t>> rhs;
    if (!construct_recovery_system(k, l, g, failed_rack_id, block_size, placement, client, available_block_ids, failed_block_ids, encode_matrix, matrix, rhs)) {
        std::cerr << "[Global] Failed to construct recovery system." << std::endl;
        return false;
    }

    std::vector<std::vector<uint8_t>> decoded;
    if (!gf256_solve(matrix, rhs, decoded)) {
        std::cerr << "[Global] GF(256) decode failed." << std::endl;
        return false;
    }
    
    repair_time_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();   //time end
    
    // 正确顺序写回，必须与 all_failed 顺序一致
    std::vector<int> failed_data_ids;
    for (int bid : failed_block_ids) {
        if (bid < k) failed_data_ids.push_back(bid);
    }
    std::sort(failed_data_ids.begin(), failed_data_ids.end());


    for (int idx = 0; idx < failed_data_ids.size(); ++idx) {
        int bid = failed_data_ids[idx];
        //std::cout<< " bid =" << bid << std::endl;
        const auto& entry = placement.table.at(bid);
        std::string recovered(decoded[idx].begin(), decoded[idx].end());
        if (!client.set(entry.server_ip, entry.port, "block_" + std::to_string(bid), recovered)) {
            std::cerr << "[Global] Failed to write recovered block_" << bid << std::endl;
            return false;
        }
        recovered_data[bid] = recovered;
//        std::cout << "[Global Repair] block_" << bid << " repaired." << std::endl;
    }

    //repair_time_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return true;
}


bool Decode::repair_and_set_group_first(int failed_block_id,
                                const Placement& placement,
                                MemcachedClient& client,
                                int k, int l, int g, int block_size,
                                std::unordered_map<int, std::string>& recovered_data,
                                const std::vector<std::vector<int>>& encode_matrix,
                                double& repair_time_ms) {


 
    auto failed_entry = placement.table.at(failed_block_id);
    int group_id = failed_entry.group_id;
    
    int failed_rack = failed_entry.rack_id;
    int failed_zone = failed_entry.zone_id;   
    // 修改处：以 zone 为单位标记不可用块
    std::unordered_set<int> failed_blocks;
  
    for (const auto& [id, entry] : placement.table) {
        if (entry.zone_id == failed_zone) {
            failed_blocks.insert(id);
        }
    }  
                    
                               
    std::unordered_set<int> remaining = failed_blocks;
    std::unordered_set<int> repaired;
    
    if(try_local_repair_for_group(group_id, k, l, failed_rack, block_size, remaining, placement, client, encode_matrix, recovered_data, repaired, repair_time_ms))
    	return true;
/*
    for (int gid = 0; gid < l; ++gid) {
        try_local_repair_for_group(gid, k, l, block_size, remaining, placement, client, encode_matrix, recovered_data, repaired);
    }
*/ /*
    for (int id : repaired) {remaining.erase(id);
    std::cout << " id = " << id << std::endl;}

    if (remaining.empty()) {
        repair_time_ms = 0.0;
        return true;
    }
*/
    return repair_with_lrc_global_solver(remaining, placement, client, k, l, g, failed_rack, block_size, encode_matrix, recovered_data, repair_time_ms);
}


