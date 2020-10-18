sources = [
    'bus_safe_fork.cc',
    'initialize.cc',
    'no_close_host_fd.cc',
    'process_override.cc',
    'spawn_request.cc',
    'supervisor.cc'
]

zypak_preload_host_spawn_strategy = shared_library(
    'zypak-preload-host-spawn-strategy',
    sources,
    include_directories: [src_dir, nickle_dir],
    dependencies: [dl_dep, threads_dep, dbus_dep],
    link_with: [zypak_base, zypak_dbus],
    install: true
)
