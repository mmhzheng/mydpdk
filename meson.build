project('flowbook', 'cpp', default_options: ['cpp_std=c++17'])

# dependencies
dpdk = dependency('libdpdk')
libcuckoo = dependency('libcuckoo')
libpqxx = dependency('libpqxx')
libpq = dependency('libpq')

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
            dependencies: [dpdk, libcuckoo, libpqxx, libpq])