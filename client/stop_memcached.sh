#!/bin/bash

ZONES=12
BASE_PORT=11211

for ((zone=0; zone<$ZONES; zone++)); do
    port=$((BASE_PORT + zone))
    pid=$(lsof -ti tcp:$port)
    if [ -n "$pid" ]; then
        echo "Stopping memcached on port $port (PID: $pid)"
        kill "$pid"
    else
        echo "No memcached process found on port $port"
    fi
done
