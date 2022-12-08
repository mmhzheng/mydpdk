project('flowbook', 'c')

dpdk = dependency('libdpdk')

incdir = include_directories('include')
sources = files('src/main.c')

executable('dpdk-flowbook', sources, include_directories: incdir, dependencies: dpdk)