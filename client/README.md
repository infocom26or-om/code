# A-LRC Simulation Environment Setup

This README provides the operation steps on the client:

---

## 1. Install Memcached on Rack Servers


### Step 1: Install Required Dependencies

```bash
sudo apt-get update
sudo apt-get install build-essential cmake automake autoconf libevent-dev
```

### Step 2: Download and Install Memcached (v1.6.38)

```bash
wget https://memcached.org/files/memcached-1.6.38.tar.gz
tar -zxvf memcached-1.6.38.tar.gz
cd memcached-1.6.38
./configure && make && make test && sudo make install
```

### Step 3: Launch Memcached Server with Large Object Support

Run Memcached with increased maximum item size to support large block data:

```bash
./memcached -m 128 -p 8888 --max-item-size=5242880 -u root -vv
```

> 📌 Note: You may need to launch multiple instances on different ports to simulate multiple zones within a rack.

---

## 2. Start memcached instances

```bash
chmod +x start_memcached.sh
./start_memcached.sh
```

---

## 3. (Optional) Limit network bandwidth 

```bash
sudo ./limit_bandwidth.sh eth0 100 100 start
```

---

