# DCBook


## Install Deps.

```bash
sudo apt-get install python3 python3-pip python3-setuptools \
                       python3-wheel ninja-build
pip3 install meson
sudo pip3 install meson

--- Build DPDK
# download dpdk-stable-20.11.1 at deps/
meson setup build
cd build
ninja
sudo ninja install   # install dpdk lib to /usr/local
sudo ldconfig        

mkdir -p /dev/hugepages
mountpoint -q /dev/hugepages || mount -t hugetlbfs nodev /dev/hugepages
echo 64 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
---

--- Bind port and run test
sudo build/app/dpdk-testpmd -c7 --vdev=net_pcap0,iface=enp130s0f0 --vdev=net_pcap1,iface=enp130s0f1 -- -i --nb-cores=2 --nb-ports=2 --total-num-mbufs=2048
---

--- Build Libcuckoo
# download Libcuckoo at deps/
git clone https://github.com/efficient/libcuckoo.git
cd libcuckoo/
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_EXAMPLES=1 -DBUILD_TESTS=1 ..
make all
sudo make install
```

## Implementation Progress

1. [OK] Setup the project skeleton from l2fwd main.
2. [OK] Add l3 and l4 parsing.
3. [OK] Add test usecases.
4. [20%] Setup EM flow table to holding table entries.
    * [OK] Use meson to reference thread-safe hash table libraries.
    * [Working] Change project to c++ style.
    * [Working] Construct self-defined flow tables.
    * [TODO] Test flow tables.
5. [TODO] Define self-defined protocol (i.e., packet header structure from NP devices.)
6. [TODO] Add aging strategy for tables and reporting module (maybe simple store as files).

---- Project works.

7. [TODO] Performance optimizing.

## BUGs

1. [FIX] Wrong dst_ip.
2. [FIX] Need to rebuild dpdk project => change to use meson build.


## Quick Start

> This will build dpdk first and then build flowbook.

```bash
meson build
ninja -C build    
```

Run flowbook daemon:

```
# 1 port
sudo ./build/flowbook -l 0 -n 1 --vdev=net_pcap0,iface=enp130s0f0 -- -q 1 -p 1

# 2 ports
sudo  ./build/flowbook -l 0-1 -n 2 --vdev=net_pcap0,iface=enp130s0f0 --vdev=net_pcap1,iface=enp130s0f1 -- -q 1 -p 3 
#             cores, core_num                                          queue_num_per_lcore port_mask(1111)     
```

Send packets.

```
sudo ./sendpkt.py -p enp130s0f0 -s 10.0.0.0/24 -d 1.1.1.1 -n 5 -l 64
```

## Links

http://doc.dpdk.org/guides/linux_gsg/build_dpdk.html