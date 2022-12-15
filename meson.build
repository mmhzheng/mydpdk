project('flowbook', 'cpp', default_options: ['cpp_std=c++11'])

# dependencies
dpdk = dependency('libdpdk')
libcuckoo = dependency('libcuckoo')

# indlude and source
incdir = include_directories('include')
sources = files('src/main.cc', 'src/flowbook_hash.cc', 'src/flowbook_table.cc')

# cxx_flags
extra_args = ['-Wdeprecated-declarations']

# build
executable('flowbook', 
            sources, 
            include_directories: incdir, 
            cpp_args : extra_args,
            dependencies: [dpdk, libcuckoo])