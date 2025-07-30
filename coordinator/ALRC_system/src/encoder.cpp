#include "encoder.hpp"
#include <iostream>
#include <cstring>
#include <cmath>
#include <jerasure.h>
#include <jerasure/reed_sol.h>
#include "gf256_solver.hpp"
#include <iomanip>


std::vector<std::vector<int>> Encoder::build_lrc_matrix(int k, int l, int g) {
    int group_size = k / l;
    int m = l + g;
    std::vector<std::vector<int>> matrix(m, std::vector<int>(k, 0));

    // 1. 生成局部校验矩阵，使用 Vandermonde 行代替全1行，保证线性无关
    for (int i = 0; i < l; ++i) {
        int base = i * group_size;
        for (int j = 0; j < group_size; ++j) {
            matrix[i][base + j] = gf256_pow(2, j); // 2^j in GF(256)
        }
    }

    // 2. 生成全局校验矩阵，k x g Vandermonde矩阵
    int* vandermonde = reed_sol_vandermonde_coding_matrix(k, g + 1, 8);
    for (int i = 0; i < g; ++i) {
        for (int j = 0; j < k; ++j) {
            int idx = (i + 1) * k + j;  // 使用第 1 ~ g 行
            matrix[l + i][j] = static_cast<uint8_t>(vandermonde[idx] & 0xFF);  // 显式截断为 GF(256) 元素
        }
    }
    delete[] vandermonde;
/*
        // === 添加调试信息 ===
    std::cout << "[Debug] Encode Matrix (" << m << " x " << k << "):\n";
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < k; ++j) {
            std::cout << matrix[i][j] << " ";
        }
        std::cout << "\n";
    }
    std::cout << "========================================\n";
*/
    return matrix;
}


std::unordered_map<int, std::string> Encoder::encode(const std::string& data, int k, int l, int g, int block_size, std::vector<std::vector<int>> matrix) {


    int m = l + g;
    int n = k + m;
//    int block_size = (data.size() + k - 1) / k;



    std::vector<std::vector<char>> data_blocks(k, std::vector<char>(block_size, 0));
    std::vector<std::vector<char>> parity_blocks(m, std::vector<char>(block_size, 0));  // l + g 校验块

    // 1. 切分原始数据
    split_data(data, data_blocks, block_size, k);

    // 3. 构造 Jerasure 编码输入
    char** data_ptrs = new char*[k];
    char** coding_ptrs = new char*[m];

    for (int i = 0; i < k; ++i) data_ptrs[i] = data_blocks[i].data();
    for (int i = 0; i < m; ++i) coding_ptrs[i] = parity_blocks[i].data();
    
    // Flatten matrix
    int* flat_matrix = new int[(l + g) * k];
    for (int i = 0; i < l + g; ++i)
    	for (int j = 0; j < k; ++j)
        	flat_matrix[i * k + j] = matrix.at(i).at(j);

    // 4. 编码
    jerasure_matrix_encode(k, m, 8, flat_matrix, data_ptrs, coding_ptrs, block_size);
    


    // 5. 封装结果
    std::unordered_map<int, std::string> result;
    int block_id = 0;
    for (auto& b : data_blocks)
        result[block_id++] = std::string(b.begin(), b.end());
    for (auto& b : parity_blocks)
        result[block_id++] = std::string(b.begin(), b.end());
/*
    std::cout << "[Encode Result] Total Blocks: " << result.size() << std::endl;
    for (const auto& [block_id, content] : result) {
        std::cout << "block_" << block_id << " : ";
        for (size_t i = 0; i < std::min<size_t>(16, content.size()); ++i) {
            std::cout << static_cast<int>(static_cast<uint8_t>(content[i])) << " ";
        }
        std::cout << "(size=" << content.size() << ")" << std::endl;
    }
*/

    delete[] flat_matrix;
    delete[] data_ptrs;
    delete[] coding_ptrs;
    return result;
}


void Encoder::split_data(const std::string& data,
                         std::vector<std::vector<char>>& data_blocks,
                         int block_size, int k) {
    for (int i = 0; i < k; ++i) {
        int start = i * block_size;
        int len = std::min((int)data.size() - start, block_size);
        if (len > 0) {
            memcpy(data_blocks[i].data(), data.data() + start, len);
        }

/*
        std::cout << "[Debug] block_" << i << ": ";
        for (int j = 0; j < std::min(len, 16); ++j) {
            std::cout << static_cast<int>(static_cast<uint8_t>(data_blocks[i][j])) << " ";
        }
        std::cout << std::endl;
*/

    }
}
