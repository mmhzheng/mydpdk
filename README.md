# DCBook


## Install Deps.

```bash
sudo apt-get install python3 python3-pip python3-setuptools \
                       python3-wheel ninja-build
pip3 install meson

--- Build DPDK
meson build
ninja -C build

mkdir -p /dev/hugepages
mountpoint -q /dev/hugepages || mount -t hugetlbfs nodev /dev/hugepages
echo 64 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
---

--- Bind port and run test
sudo build/app/dpdk-testpmd -c7 --vdev=net_pcap0,iface=enp130s0f0 --vdev=net_pcap1,iface=enp130s0f1 -- -i --nb-cores=2 --nb-ports=2 --total-num-mbufs=2048
```


## Implementation Thoughts

1. [OK] Setup the project skeleton from l2fwd main.
2. [OK] Add l3 and l4 parsing.
3. [TODO] Add test usecases.
4. [TODO] Setup EM flow table to holding table entries.
5. [TODO] Add aging strategy for tables.
6. [TODO] Add reporting threads (maybe simple store as files).
7. [TODO] Performance optimizing.


## Quick Start

> This will build dpdk first and then build flowbook.

```bash
./build.sh

sudo ./dpdk-flowbook -l 0-1 -n 2 --vdev=net_pcap0,iface=enp130s0f0 --vdev=net_pcap1,iface=enp130s0f0 -- -q 1 -p 3 --portmap="(0,1)"
#             cores, core_num                                          queue_num_per_lcore port_mask(1111) portmap 0 <-> 2, 1 <-> 3 
#                                                                      note that each port may have multiple queue.                 
```