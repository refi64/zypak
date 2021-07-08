sources = [
    'debug_internal/log_stream.cc',
    'debug.cc',
    'env.cc',
    'evloop.cc',
    'fd_map.cc',
    'socket.cc',
    'strace.cc'
]

zypak_base = static_library(
    'zypak-base',
    sources,
    include_directories: [src_dir],
    dependencies: [libsystemd_dep]
)
