#! /bin/bash

set -x

dpdk_path='deps/dpdk-stable-22.11.1/'

## Clean Existing 
rm -rf $dpdk_path/app/dcbook
cp -r src $dpdk_path/app/dcbook
sed -i 's/dcbook/test-acl/' app/meson.build
rm ./dpdk-dcbook

cd $dpdk_path
sed -i 's/test-acl/dcbook/' app/meson.build
meson build
ninja -C build
cd -

cp $dpdk_path/build/app/dpdk-dcbook ./

echo "Done."