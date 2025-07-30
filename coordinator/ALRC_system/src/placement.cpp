// placement.cpp
#include "placement.hpp"
#include "memcached_client.hpp"
#include <iostream>
#include <algorithm>

Placement::Placement(int _k, int _l, int _g, int _z, int _strategy)
    : k(_k), l(_l), g(_g), z(_z), strategy(_strategy) {
    rack_zone_load.resize(200, std::vector<int>(z, 0));

    rack_ips = {
        "10.0.0.112", "10.0.0.113", "10.0.0.114", "10.0.0.115",
        "10.0.0.116", "10.0.0.117", "10.0.0.118", "10.0.0.119",
        "10.0.0.120", "10.0.0.121", "10.0.0.122"
    };
}


PlacementEntry Placement::get(int block_id) {
    return table.at(block_id);
}

void Placement::print_summary() {
    for (const auto& [id, entry] : table) {
        std::cout << "Block " << id << ": type=" << entry.type
                  << ", group=" << entry.group_id
                  << ", idx=" << entry.block_index
                  << ", rack=" << entry.rack_id
                  << ", zone=" << entry.zone_id << std::endl;
    }
}

int Placement::get_port(int rack, int zone, int zones_per_rack) const {
    return 11211 + zone;
    // return 11211 + zone;
}


void Placement::init() {
    int block_id = 0;
    int data_id = 0;
    int local_id = k;
    int global_id = k + l;
    int blocks_per_group = k / l;
    int max_per_rack = g + 1;
    int current_rack = 0;
    bool l_separate_rack = (blocks_per_group % max_per_rack == 0);
    // basic
    if (strategy == 1) {
        for (int group = 0; group < l; ++group) {
            int placed = 0;
            int zone_idx = 0;
	
            while (placed < blocks_per_group) {
                int to_place = std::min(blocks_per_group - placed, max_per_rack);

                for (int p = 0; p < to_place; ++p) {
                    int zone = zone_idx % z;
                    while (rack_zone_load[current_rack][zone] >= 1) {
                        zone = (zone + 1) % z;
                    }
                    table[data_id] = {
                        rack_ips[current_rack],
                        get_port(current_rack, zone, z),
                        current_rack, zone, group, DATA, data_id
                    };
                    data_id++;
                    rack_zone_load[current_rack][zone]++;
                    placed++;
                    zone_idx++;
                }
                current_rack++;
            }

            if (!l_separate_rack) {
                int zone = zone_idx % z;
                while (rack_zone_load[current_rack - 1][zone] >= 1) {
                    zone = (zone + 1) % z;
                }
                table[local_id] = {
                    rack_ips[current_rack - 1],
                    get_port(current_rack - 1, zone, z),
                    current_rack - 1, zone, group, LOCAL_PARITY, local_id
                };
                local_id++;
                rack_zone_load[current_rack - 1][zone]++;
                zone_idx++;
            }
        }

        if (l_separate_rack) {
            int rack_lg = current_rack;
            int zone_idx = 0;

            for (int group = 0; group < l; ++group) {
                int zone = zone_idx % z;
                while (rack_zone_load[rack_lg][zone] >= 1) {
                    zone = (zone + 1) % z;
                }
                table[local_id] = {
                    rack_ips[rack_lg],
                    get_port(rack_lg, zone, z),
                    rack_lg, zone, group, LOCAL_PARITY, local_id
                };
                local_id++;
                rack_zone_load[rack_lg][zone]++;
                zone_idx++;
            }

            for (int gid = 0; gid < g; ++gid) {
                int zone = zone_idx % z;
                while (rack_zone_load[rack_lg][zone] >= 1) {
                    zone = (zone + 1) % z;
                }
                table[global_id] = {
                    rack_ips[rack_lg],
                    get_port(rack_lg, zone, z),
                    rack_lg, zone, -1, GLOBAL_PARITY, global_id
                };
                global_id++;
                rack_zone_load[rack_lg][zone]++;
                zone_idx++;
            }
        } else {
            int rack_g = current_rack;
            int zone_idx = 0;

            for (int gid = 0; gid < g; ++gid) {
                int zone = zone_idx % z;
                while (rack_zone_load[rack_g][zone] >= 1) {
                    zone = (zone + 1) % z;
                }
                table[global_id] = {
                    rack_ips[rack_g],
                    get_port(rack_g, zone, z),
                    rack_g, zone, -1, GLOBAL_PARITY, global_id
                };
                global_id++;
                rack_zone_load[rack_g][zone]++;
                zone_idx++;
            }
        }
    }
    // our1
    else if (strategy == 2) {
        std::vector<std::pair<int, int>> zone_load(z);
        for (int i = 0; i < z; ++i) zone_load[i] = {0, i};

        for (int grp = 0; grp < l; ++grp) {
            int placed = 0;
            while (placed < blocks_per_group) {
                int to_place = std::min(blocks_per_group - placed, max_per_rack);
                std::sort(zone_load.begin(), zone_load.end());

                for (int j = 0; j < z && to_place > 0; ++j) {
                    int zid = zone_load[j].second;
                    if (rack_zone_load[current_rack][zid] >= 1) continue;
                    table[data_id] = {
                        rack_ips[current_rack], get_port(current_rack, zid, z),
                        current_rack, zid, grp, DATA, data_id
                    };
                    data_id++;
                    rack_zone_load[current_rack][zid]++;
                    zone_load[j].first++;
                    placed++;
                    to_place--;
                }
                current_rack++;
            }

            if (!l_separate_rack) {
                std::sort(zone_load.begin(), zone_load.end());
                for (int j = 0; j < z; ++j) {
                    int zid = zone_load[j].second;
                    if (rack_zone_load[current_rack - 1][zid] >= 1) continue;
                    table[local_id] = {
                        rack_ips[current_rack - 1], get_port(current_rack - 1, zid, z),
                        current_rack - 1, zid, grp, LOCAL_PARITY, local_id
                    };
                    local_id++;
                    rack_zone_load[current_rack - 1][zid]++;
                    zone_load[j].first++;
                    break;
                }
            }
        }

        if (l_separate_rack) {
            for (int grp = 0; grp < l; ++grp) {
                std::sort(zone_load.begin(), zone_load.end());
                for (int j = 0; j < z; ++j) {
                    int zid = zone_load[j].second;
                    if (rack_zone_load[current_rack][zid] >= 1) continue;
                    table[local_id] = {
                        rack_ips[current_rack], get_port(current_rack, zid, z),
                        current_rack, zid, grp, LOCAL_PARITY, local_id
                    };
                    local_id++;
                    rack_zone_load[current_rack][zid]++;
                    zone_load[j].first++;
                    break;
                }
            }
            for (int gid = 0; gid < g; ++gid) {
                std::sort(zone_load.begin(), zone_load.end());
                for (int j = 0; j < z; ++j) {
                    int zid = zone_load[j].second;
                    if (rack_zone_load[current_rack][zid] >= 1) continue;
                    table[global_id] = {
                        rack_ips[current_rack], get_port(current_rack, zid, z),
                        current_rack, zid, -1, GLOBAL_PARITY, global_id
                    };
                    global_id++;
                    rack_zone_load[current_rack][zid]++;
                    zone_load[j].first++;
                    break;
                }
            }
        } else {
            for (int gid = 0; gid < g; ++gid) {
                std::sort(zone_load.begin(), zone_load.end());
                for (int j = 0; j < z; ++j) {
                    int zid = zone_load[j].second;
                    if (rack_zone_load[current_rack][zid] >= 1) continue;
                    table[global_id] = {
                        rack_ips[current_rack], get_port(current_rack, zid, z),
                        current_rack, zid, -1, GLOBAL_PARITY, global_id
                    };
                    global_id++;
                    rack_zone_load[current_rack][zid]++;
                    zone_load[j].first++;
                    break;
                }
            }
        }
    }
    
    //our2
    else if (strategy == 3) {
        std::vector<std::pair<int, int>> zone_load(z);
        std::unordered_map<int, std::unordered_map<int, int>> zone_group_block_count; //zone_group_block_count[i][group]

        for (int i = 0; i < z; ++i) zone_load[i] = {0, i};

        for (int group = 0; group < l; ++group) {
            int placed = 0;
            while (placed < blocks_per_group) {
                int to_place = std::min(blocks_per_group - placed, max_per_rack);
                std::sort(zone_load.begin(), zone_load.end());

                for (int j = 0; j < z && to_place > 0; ++j) {
                    int zid = zone_load[j].second;
                    if (rack_zone_load[current_rack][zid] >= 1) continue;

                    table[data_id] = {
                        rack_ips[current_rack], get_port(current_rack, zid, z),
                        current_rack, zid, group, DATA, data_id
                    };
                    data_id++;
                    rack_zone_load[current_rack][zid]++;
                    zone_group_block_count[zid][group]++;
                    zone_load[j].first++;
                    placed++;
                    to_place--;
                }
                current_rack++;
            }

            if (!l_separate_rack) {
                std::vector<std::pair<int, int>> group_zone_load;
                for (int zid = 0; zid < z; ++zid) {
                    int cnt = zone_group_block_count[zid][group];
                    group_zone_load.push_back({cnt, zid});
                }

                if (blocks_per_group <= z) {
                    std::sort(group_zone_load.begin(), group_zone_load.end());
                } else {
                    std::sort(group_zone_load.begin(), group_zone_load.end(),
                            [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                                if (a.first != b.first) return a.first > b.first;
                                return a.second < b.second;
                            });
                }

                for (auto [_, zid] : group_zone_load) {
                    if (rack_zone_load[current_rack - 1][zid] >= 1) continue;

                    table[local_id] = {
                        rack_ips[current_rack - 1], get_port(current_rack -1, zid, z),
                        current_rack - 1, zid, group, LOCAL_PARITY, local_id
                    };
                    local_id++;
                    rack_zone_load[current_rack - 1][zid]++;
                    for (auto& p : zone_load) {
                        if (p.second == zid) {
                            p.first++;
                            break;
                        }
                    }
                    break;
                }
            }
        }

        int rack_lg = current_rack;

        if (l_separate_rack) {
            for (int group = 0; group < l; ++group) {
                std::vector<std::pair<int, int>> group_zone_load;
                for (int zid = 0; zid < z; ++zid) {
                    int cnt = zone_group_block_count[zid][group];
                    group_zone_load.push_back({cnt, zid});
                }

                if (blocks_per_group <= z) {
                    std::sort(group_zone_load.begin(), group_zone_load.end());
                } else {
                    std::sort(group_zone_load.begin(), group_zone_load.end(),
                            [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                                if (a.first != b.first) return a.first > b.first;
                                return a.second < b.second;
                            });
                }

                for (auto [_, zid] : group_zone_load) {
                    if (rack_zone_load[rack_lg][zid] >= 1) continue;

                    table[local_id] = {
                        rack_ips[rack_lg], get_port(rack_lg, zid, z),
                        rack_lg, zid, group, LOCAL_PARITY, local_id
                    };
                    local_id++;
                    rack_zone_load[rack_lg][zid]++;
                    for (auto& p : zone_load) {
                        if (p.second == zid) {
                            p.first++;
                            break;
                        }
                    }
                    break;
                }
            }

            for (int gid = 0; gid < g; ++gid) {
                std::sort(zone_load.begin(), zone_load.end());
                for (int j = 0; j < z; ++j) {
                    int zid = zone_load[j].second;
                    if (rack_zone_load[rack_lg][zid] >= 1) continue;

                    table[global_id] = {
                        rack_ips[rack_lg], get_port(rack_lg, zid, z),
                        rack_lg, zid, -1, GLOBAL_PARITY, global_id
                    };
                    global_id++;
                    rack_zone_load[rack_lg][zid]++;
                    zone_load[j].first++;
                    break;
                }
            }
        } else {
            for (int gid = 0; gid < g; ++gid) {
                std::sort(zone_load.begin(), zone_load.end());
                for (int j = 0; j < z; ++j) {
                    int zid = zone_load[j].second;
                    if (rack_zone_load[rack_lg][zid] >= 1) continue;

                    table[global_id] = {
                        rack_ips[rack_lg], get_port(rack_lg, zid, z),
                        rack_lg, zid, -1, GLOBAL_PARITY, global_id
                    };
                    global_id++;
                    rack_zone_load[rack_lg][zid]++;
                    zone_load[j].first++;
                    break;
                }
            }
        }
    }

    // our1&our2
    else if (strategy == 4) {
        std::vector<std::pair<int, int>> zone_load(z);
        std::unordered_map<int, std::unordered_map<int, int>> zone_group_block_count;
        std::vector<int> group_last_rack(l, -1);
        for (int i = 0; i < z; ++i) zone_load[i] = {0, i};

        for (int grp = 0; grp < l; ++grp) {
            int placed = 0;
            while (placed < blocks_per_group) {
                int to_place = std::min(blocks_per_group - placed, max_per_rack);
                std::sort(zone_load.begin(), zone_load.end());
                for (int j = 0; j < z && to_place > 0; ++j) {
                    int zid = zone_load[j].second;
                    if (rack_zone_load[current_rack][zid] >= 1) continue;
                    table[data_id] = {
                        rack_ips[current_rack], get_port(current_rack, zid, z),
                        current_rack, zid, grp, DATA, data_id
                    };
                    data_id++;
                    rack_zone_load[current_rack][zid]++;
                    zone_group_block_count[zid][grp]++;
                    zone_load[j].first++;
                    placed++;
                    to_place--;
                    group_last_rack[grp] = current_rack;
                }
                current_rack++;
            }
        }

        for (int grp = 0; grp < l; ++grp) {
            int l_rack = l_separate_rack ? current_rack : group_last_rack[grp];
            std::vector<std::tuple<int, int, int>> group_zone_load;
            for (int zid = 0; zid < z; ++zid) {
                int group_cnt = zone_group_block_count[zid][grp];
                int total_cnt = 0;
                for (const auto &kv : rack_zone_load) total_cnt += kv[zid];
                group_zone_load.emplace_back(group_cnt, total_cnt, zid);
            }

            int target_zid = -1;
            if (blocks_per_group <= z) {
                std::sort(group_zone_load.begin(), group_zone_load.end());
            } else {
                std::sort(group_zone_load.begin(), group_zone_load.end(), [](const auto &a, const auto &b) {
                    if (std::get<0>(a) != std::get<0>(b)) return std::get<0>(a) > std::get<0>(b);
                    if (std::get<1>(a) != std::get<1>(b)) return std::get<1>(a) < std::get<1>(b);
                    return std::get<2>(a) < std::get<2>(b);
                });
            }

            for (const auto &[gcnt, tcnt, zid] : group_zone_load) {
                if (rack_zone_load[l_rack][zid] < 1 && tcnt + 1 <= g + l) {
                    target_zid = zid;
                    break;
                }
            }

            if (target_zid != -1) {
                table[local_id] = {
                    rack_ips[l_rack], get_port(l_rack, target_zid, z),
                    l_rack, target_zid, grp, LOCAL_PARITY, local_id
                };
                local_id++;
                rack_zone_load[l_rack][target_zid]++;
                for (auto &p : zone_load) {
                    if (p.second == target_zid) {
                        p.first++;
                        break;
                    }
                }
            }
        }

        int g_rack = current_rack;
        for (int gid = 0; gid < g; ++gid) {
            std::sort(zone_load.begin(), zone_load.end());
            for (int j = 0; j < z; ++j) {
                int zid = zone_load[j].second;
                if (rack_zone_load[g_rack][zid] >= 1) continue;
                table[global_id] = {
                    rack_ips[g_rack], get_port(g_rack, zid, z),
                    g_rack, zid, -1, GLOBAL_PARITY, global_id
                };
                global_id++;
                rack_zone_load[g_rack][zid]++;
                zone_load[j].first++;
                break;
            }
        }
    }
    // baseline-2
    else if (strategy == 5) {
        //int block_id = 0;
        //int blocks_per_group = k / l;

        // Step 1: Place data blocks such that each rack holds the i-th data block of every group
        for (int i = 0; i < blocks_per_group; ++i) {
            for (int group = 0; group < l; ++group) {
                int rack = i; // rack i holds the i-th block of each group
                int zone = group % z;
                int temp_id = data_id + group * blocks_per_group;
                table[temp_id] = {
                    rack_ips[rack],
                    get_port(rack, zone, z),
                    rack, zone, group, DATA, temp_id
                };
               
                rack_zone_load[rack][zone]++;
            }
            data_id++;
        }

        // Step 2: Place all local parity blocks in the same rack
        int local_parity_rack = blocks_per_group; // next rack
        for (int group = 0; group < l; ++group) {
            int zone = group % z;
            table[local_id] = {
                rack_ips[local_parity_rack],
                get_port(local_parity_rack, zone, z),
                local_parity_rack, zone, group, LOCAL_PARITY, local_id
            };
            local_id++;
            rack_zone_load[local_parity_rack][zone]++;
        }

        // Step 3: Place all global parity blocks in the same rack
        int global_parity_rack = blocks_per_group + 1; // next rack
        for (int gid = 0; gid < g; ++gid) {
            int zone = gid % z;
            table[global_id] = {
                rack_ips[global_parity_rack],
                get_port(global_parity_rack, zone, z),
                global_parity_rack, zone, -1, GLOBAL_PARITY, global_id
            };
            global_id++;
            rack_zone_load[global_parity_rack][zone]++;
        }
    }


}

void Placement::set_all_blocks_to_servers(const std::unordered_map<int, std::string>& block_data) {
    MemcachedClient client;
    for (const auto& [id, entry] : table) {
        auto it = block_data.find(entry.block_index);
        if (it != block_data.end()) {
            std::string key = "block_" + std::to_string(entry.block_index);
            const std::string& value = it->second;
            bool success = client.set(entry.server_ip, entry.port, key, value);
            if (!success) {
                std::cerr << "Failed to set block " << entry.block_index << " to server "
                          << entry.server_ip << ":" << entry.port << std::endl;
            }
            //std::cout << "entry.server_ip:" << entry.server_ip << std::endl;
            //std::cout <<"  entry.port:" << entry.port << std::endl;
            //std::cout << "  key:" << key << std::endl;
            //std::cout << "value:" << value << std::endl;
        } else {
            std::cerr << "Warning: No data found for block " << entry.block_index << std::endl;
        }
    }
}