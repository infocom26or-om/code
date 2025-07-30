// single_repair.hpp
#pragma once

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include "placement.hpp"
#include "memcached_client.hpp"

class Single {

public:
	bool repair_and_set_group_first(int failed_block_id,
                                const Placement& placement,
                                MemcachedClient& client,
                                int k, int l, int g, int block_size,
                                std::unordered_map<int, std::string>& recovered_data,
                                const std::vector<std::vector<int>>& encode_matrix,
                                double& repair_time_ms);
        bool try_local_repair_for_group(int group_id, int k, int l, int failed_rackId, int block_size,
                                int failed_block_id,
                                const Placement& placement,
                                MemcachedClient& client,
                                const std::vector<std::vector<int>>& encode_matrix,
                                std::unordered_map<int, std::string>& recovered_data,
                                double& repair_time_ms);

}; 
