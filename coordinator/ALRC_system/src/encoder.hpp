// encoder.hpp
#pragma once

#include <string>
#include <vector>
#include <unordered_map>

class Encoder {
public:
    // 返回 block_id -> 块内容 映射
    std::unordered_map<int, std::string> encode(const std::string& data, int k, int l, int g, int block_size, std::vector<std::vector<int>> matrix);
    std::vector<std::vector<int>> build_lrc_matrix(int k, int l, int g);

private:
    void split_data(const std::string& data, std::vector<std::vector<char>>& data_blocks, int block_size, int k);
    
//    void generate_local_parity(const std::vector<std::vector<char>>& data_blocks,
//                               std::vector<std::vector<char>>& local_parities,
//                               int k, int l, int block_size);
//    void generate_global_parity(const std::vector<std::vector<char>>& data_blocks,
//                                std::vector<std::vector<char>>& global_parities,
//                                int k, int g, int block_size);
};
