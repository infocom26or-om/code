// placement.hpp
#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "memcached_client.hpp"

enum BlockType {
    DATA,
    LOCAL_PARITY,
    GLOBAL_PARITY
};

struct PlacementEntry {
    std::string server_ip;
    int port;
    int rack_id;
    int zone_id;
    int group_id;
    BlockType type;
    int block_index; // block_id in encoder
//    std::string payload;  // 存放块的数据内容
};

class Placement {
private:
    int k, l, g, z, strategy;
    std::vector<std::vector<int>> rack_zone_load; // [rack][zone] = num blocks
    std::vector<std::string> rack_ips;
    int get_port(int rack, int zone, int zones_per_rack) const;
public:
    std::unordered_map<int, PlacementEntry> table;

    Placement(int _k, int _l, int _g, int _z, int _strategy);
    void init();
    PlacementEntry get(int block_id);
    void print_summary();
    //Placement 类内部调用 MemcachedClient
    void set_all_blocks_to_servers(const std::unordered_map<int, std::string>& block_data);

};