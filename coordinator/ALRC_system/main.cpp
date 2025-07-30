#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <vector>
#include <chrono>
#include <random>

#include "encoder.hpp"
#include "placement.hpp"
#include "memcached_client.hpp"
#include "repair.hpp"
#include "gf256_solver.hpp"
#include "decode.hpp"
#include "single_repair.hpp"

//const int BLOCK_SIZE = 64;              // 每个块大小64 KB
//const int BLOCK_SIZE = 256;              // 每个块大小256 KB
//const int BLOCK_SIZE = 1024;              // 每个块大小1 MB
//const int BLOCK_SIZE = 4096;              // 每个块大小4 MB
const std::string INPUT_FILE = "input_data.txt";


// Step 1: 生成指定大小的可读输入文件
void generate_input_file(const std::string& filename, int size) {
    std::ofstream out(filename);
    std::mt19937 rng(std::random_device{}());

    std::string charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::uniform_int_distribution<> dist(0, charset.size() - 1);

    for (int i = 0; i < size; ++i) {
        char c = charset[dist(rng)];
        out << c;
    }

    out.close();
}


int main() {
    init_tables();
    // 参数设定
    int k, l, g, strategy;
    int z;
    int BLOCK_SIZE;
    //int k = 4, l = 1, g = 1, z = 8;
    //int strategy = 2;
    std::cout << "please input k, l, g, z, and strategy: " << std::endl;
    std::cin >> k >> l >> g >> z >> strategy;
    std::cout << "please input BLOCK_SIZE(64 or 256 or 1024 or 4096): " << std::endl;
    std::cin >> BLOCK_SIZE;

    
    std::string unit;
    std::cout << "please input maintenance unit (rack or zone or single) :" << std::endl;
    std::cin >> unit;
    
    int total_blocks = k + l + g;
    int FILE_SIZE = BLOCK_SIZE * k;         // 输入文件总大小 

    std::cout << "========== A-LRC Simulation Start ==========\n";

    // Step 1: 生成输入文件
    generate_input_file(INPUT_FILE, FILE_SIZE);
    std::cout << "[INFO] Input file generated: " << INPUT_FILE << "\n";

    // Step 2: 编码
    std::ifstream in(INPUT_FILE);
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string input_data = buffer.str();
    //std::cout<<input_data<<std::endl;

    Encoder encoder;
    std::vector<std::vector<int>> lrc_matrix = encoder.build_lrc_matrix(k, l, g);
    std::unordered_map<int, std::string> encoded_blocks = encoder.encode(input_data, k, l, g, BLOCK_SIZE, lrc_matrix);
    std::cout << "[INFO] Encoding complete. Total blocks: " << encoded_blocks.size() << "\n";

    // Step 3: 块放置 & 写入 Memcached
    Placement placement(k, l, g, z, strategy);
    placement.init();
    placement.set_all_blocks_to_servers(encoded_blocks);
    std::cout << "[INFO] All blocks placed into Memcached.\n";

    // Step 4: 故障模拟 + 解码修复
    double total_repair_time = 0.0;
    int successful_repairs = 0;
    
    if(unit == "rack"){
    
    	Repair repair;
    	MemcachedClient client;

//    	std::cout << "========== Simulating Failures and Repair (Rack)==========\n";
    	for (int failed_id = 0; failed_id < k; ++failed_id) {

        	std::unordered_map<int, std::string> recovered;
        	double repair_time = 0.0;
        	bool ok = repair.repair_and_set_group_first(failed_id, placement, client, k, l, g, BLOCK_SIZE, recovered, lrc_matrix, repair_time);
        	if (ok) {
            		++successful_repairs;
            		total_repair_time += repair_time;
            		std::cout << "[OK] Block " << failed_id << " repaired in " << repair_time << " ms.\n";
        	} else {
            		auto entry = placement.get(failed_id);
            		std::cerr << "[FAIL] Block " << failed_id << " (rack=" << entry.rack_id
                      		<< ", zone=" << entry.zone_id << ") repair failed.\n";
        	}
    	}
    }
    
    if(unit == "zone"){
    
    	Decode decode;
    	MemcachedClient client;

//    	std::cout << "========== Simulating Failures and Repair (Zone)==========\n";
    	for (int failed_id = 0; failed_id < k; ++failed_id) {

        	std::unordered_map<int, std::string> recovered;
        	double repair_time = 0.0;
        	bool ok = decode.repair_and_set_group_first(failed_id, placement, client, k, l, g, BLOCK_SIZE, recovered, lrc_matrix, repair_time);
        	if (ok) {
            		++successful_repairs;
            		total_repair_time += repair_time;
            		std::cout << "[OK] Block " << failed_id << " repaired in " << repair_time << " ms.\n";
        	} else {
            		auto entry = placement.get(failed_id);
            		std::cerr << "[FAIL] Block " << failed_id << " (rack=" << entry.rack_id
                      		<< ", zone=" << entry.zone_id << ") repair failed.\n";
        	}
    	}
    }

    if(unit == "single"){
    
    	Single single;
    	MemcachedClient client;

//    	std::cout << "========== Simulating Failures and Repair (Single)==========\n";
    	for (int failed_id = 0; failed_id < k; ++failed_id) {

        	std::unordered_map<int, std::string> recovered;
        	double repair_time = 0.0;
        	bool ok = single.repair_and_set_group_first(failed_id, placement, client, k, l, g, BLOCK_SIZE, recovered, lrc_matrix, repair_time);
        	if (ok) {
            		++successful_repairs;
            		total_repair_time += repair_time;
            		std::cout << "[OK] Block " << failed_id << " repaired in " << repair_time << " ms.\n";
        	} else {
            		auto entry = placement.get(failed_id);
            		std::cerr << "[FAIL] Block " << failed_id << " (rack=" << entry.rack_id
                      		<< ", zone=" << entry.zone_id << ") repair failed.\n";
        	}
    	}
    }


    // Step 5: 输出统计信息
    std::cout << "========== Repair Summary ==========\n";
    std::cout << "Total blocks tested: " << k << "\n";
    std::cout << "Successful repairs:  " << successful_repairs << "\n";
    if (successful_repairs > 0) {
        std::cout << "Average repair time: "
                  << total_repair_time / successful_repairs << " ms\n";
    } else {
        std::cout << "No successful repairs.\n";
    }

    std::cout << "========== A-LRC Simulation Complete ==========\n";
    return 0;
}
