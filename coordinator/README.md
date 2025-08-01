# A-LRC Simulation Environment Setup

This README provides the operation steps on the coordinator:

---

## 1. Install Dependencies on the Control Node

The control node coordinates encoding, placement, and repair. It requires the following libraries:

### Step 1: Install `libmemcached`

```bash
wget https://launchpad.net/libmemcached/1.0/1.0.18/+download/libmemcached-1.0.18.tar.gz
tar -zvxf libmemcached-1.0.18.tar.gz
cd libmemcached-1.0.18
./configure --prefix=$(pwd)
make
sudo make install
```

> This provides the client interface for interacting with Memcached servers from the control node.

---

## 2. Install Jerasure

If not yet installed, the **Jerasure** library is required for erasure coding and decoding. Follow standard installation instructions from its [GitHub repository](https://github.com/tsuraan/Jerasure) or your internal documentation.

---

## 2. Run
```bash
cd ALRC_System
mkdir -p build && cd build
cmake ..
make -j
./ALRC_system
```


