#!/bin/bash

ZONES=12
BASE_PORT=11211

mkdir -p logs

for ((zone=0; zone<$ZONES; zone++)); do
    port=$((BASE_PORT + zone))
    if lsof -i:$port >/dev/null; then
        echo "Port $port is already in use. Skipping."
    else
        echo "Starting memcached on port $port"
        memcached -p $port -d -vv -u root > "logs/memcached_${port}.log" 2>&1
    fi
done

