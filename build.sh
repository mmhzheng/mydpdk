#! /bin/bash

set -x
set -E

dpdk_path='deps/dpdk-stable-22.11.1/'

## Clean Existing 
rm -rf $dpdk_path/app/flowbook
cp -r src $dpdk_path/app/flowbook
sed -i 's/flowbook/test-acl/' $dpdk_path/app/meson.build
rm ./dpdk-flowbook

## Build.
cd $dpdk_path
sed -i 's/test-acl/flowbook/' app/meson.build
meson build
ninja -C build
cd -
cp $dpdk_path/build/app/dpdk-flowbook ./

echo "Done."