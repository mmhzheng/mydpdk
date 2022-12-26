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
ninja -C build
sudo ninja -C build install   # install dpdk lib to /usr/local
sudo ldconfig        

mkdir -p /dev/hugepages
mountpoint -q /dev/hugepages || mount -t hugetlbfs nodev /dev/hugepages
echo 64 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
grep Huge /proc/meminfo
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

--- Install Mysql
sudo apt-get install mysql-server mysql-client  # version 8.0

# https://dev.mysql.com/downloads/connector/cpp/
# https://pkgs.org/download/mysql-community-client-plugins
sudo dpkg -u *.deb

sudo mysql -h 127.0.0.1 -uroot -proot  <test/create_db.sql
```

## Implementation Progress

1. [OK] Setup the project skeleton from l2fwd main.
2. [OK] Add l3 and l4 parsing.
3. [OK] Add test usecases.
4. [OK] Setup EM flow table to holding table entries.
    * [OK] Use meson to reference thread-safe hash table libraries.
    * [OK] Change project to c++ style.
    * [OK] Construct self-defined flow tables (Use high-performance concurrent hash table libcuckoo).
    * [OK] Test flow tables.
5. [OK] Define self-defined protocol (i.e., packet header structure from NP devices.)
6. [OK] Add aging strategy for tables and reporting module (maybe simple store as files).
    * [OK] Thread-safe table switching (active table and idle table).
    * [OK] Store idle table entires into a file.

---- Project works.

7. [OK] Performance optimizing.
    * [OK] Replace the STL default hash functions with a better one.
    * [OK] Multithread reporting.
    * [OK] Enable multi-queue feature of NIC and scale the number of concurrent lcores.
        * [OK] need to check&test RSS mode (use ip+udp hash mode supported by hardware).
8. [TODO] Report to redis or other database (currently store as files).

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
# 1 port with 2 queues.
sudo ./build/flowbook -l 1,2 -n 4 --vdev=net_pcap0,iface=enp130s0f0 -- -p 0x1 --config="(0,0,1),(0,1,2)" 
                     core_num  mem_channel_num                             port_mask  

# 2 port, each with 4 queues. total 4 queues and 4 cores.
sudo ./build/flowbook -l 1-4 -n 4 --vdev=net_pcap0,iface=enp130s0f0 -- -p 0x3 --config="(0,0,1),(0,1,2),(1,0,3),(1,1,4)" 
              core_num  mem_channel_num                             port_mask  
```

Send packets.

```
sudo ./test/sendpkt.py -p enp130s0f0 -s 10.0.0.0/24 -d 1.1.1.0/24 -n 5 -l 64
```

## Links

http://doc.dpdk.org/guides/linux_gsg/build_dpdk.html