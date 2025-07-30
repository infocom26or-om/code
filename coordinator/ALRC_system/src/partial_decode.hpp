// partial_decoder.hpp
#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "placement.hpp"

class PartialDecoder {
public:
    bool recover_and_set(int failed_block_id,
                               const Placement& placement,
                               MemcachedClient& client,
                               int k, int l, int g, int block_size,
                               std::unordered_map<int, std::string>& recovered_data,
                               double& repair_time_ms);
};