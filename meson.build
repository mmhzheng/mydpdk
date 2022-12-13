project('flowbook', 'cpp', default_options: ['cpp_std=c++11'])

dpdk = dependency('libdpdk')
libcuckoo = dependency('libcuckoo')

incdir = include_directories('include')
sources = files('src/main.cc', 'src/flowbook_hash.cc')

executable('flowbook', sources, include_directories: incdir, dependencies: [dpdk, libcuckoo])