cmake_minimum_required(VERSION 3.10)
project(ALRC_System)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === Manually set Jerasure ===
set(JERASURE_INCLUDE_DIR "/usr/local/include")
set(JERASURE_LIBRARY "/usr/local/lib/libJerasure.so")

# === Manually set GF-Complete ===
set(GALOIS_LIBRARY "/usr/local/lib/libgf_complete.so")

# === Manually set libmemcached ===
set(MEMCACHED_INCLUDE_DIR "/usr/include")  # 或 /usr/local/include 看你系统的情况
set(MEMCACHED_LIBRARY "/usr/lib/x86_64-linux-gnu/libmemcached.so")


# Add include directories
include_directories(
    ${JERASURE_INCLUDE_DIR}
    ${MEMCACHED_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)

# Source files
add_executable(ALRC_System
    main.cpp
    src/placement.cpp
    src/encoder.cpp
    src/repair.cpp
    src/memcached_client.cpp
    src/decode.cpp
    #src/partial_decode.cpp
    src/gf256_solver.cpp
    src/single_repair.cpp
)

# Link libraries
target_link_libraries(ALRC_System
    ${JERASURE_LIBRARY}
    ${GALOIS_LIBRARY}
    ${MEMCACHED_LIBRARY}
)
