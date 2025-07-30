// decode.hpp
#pragma once

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include "placement.hpp"
#include "memcached_client.hpp"

class Decode {


public:
	bool repair_and_set_group_first(int failed_block_id,
                                const Placement& placement,
                                MemcachedClient& client,
                                int k, int l, int g, int block_size,
                                std::unordered_map<int, std::string>& recovered_data,
                                const std::vector<std::vector<int>>& encode_matrix,
                                double& repair_time_ms);
        bool try_local_repair_for_group(int group_id, int k, int l, int failed_rack_id, int block_size,
                                const std::unordered_set<int>& failed_blocks,
                                const Placement& placement,
                                MemcachedClient& client,
                                const std::vector<std::vector<int>>& encode_matrix,
                                std::unordered_map<int, std::string>& recovered_data,
                                std::unordered_set<int>& repaired_ids,
                                double& repair_time_ms);
       bool construct_recovery_system(int k, int l, int g, int failed_rack_id, int block_size,
       			       const Placement& placement,
       			       MemcachedClient& client,
                               const std::unordered_set<int>& available_block_ids,
                               const std::unordered_set<int>& failed_blocks,
                               const std::vector<std::vector<int>>& encode_matrix, // 传入编码矩阵
                               std::vector<std::vector<int>>& matrix,
                               std::vector<std::vector<uint8_t>>& rhs);
       bool gf256_solve(const std::vector<std::vector<int>>& A,
                 const std::vector<std::vector<uint8_t>>& B,
                 std::vector<std::vector<uint8_t>>& X);
       bool repair_with_lrc_global_solver(const std::unordered_set<int>& failed_block_ids,
                                   const Placement& placement,
                                   MemcachedClient& client,
                                   int k, int l, int g, int failed_rack_id, int block_size,
                                   const std::vector<std::vector<int>>& encode_matrix,
                                   std::unordered_map<int, std::string>& recovered_data,
                                   double& repair_time_ms); 
       bool check_and_add_row(std::vector<std::vector<int>>& mat, 
       				const std::vector<int>&new_row);                                                                                                           

}; 
