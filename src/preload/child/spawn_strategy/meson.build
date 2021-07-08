sources = [
    'chroot_fake.cc'
]

zypak_preload_child_spawn_strategy = shared_library(
    'zypak-preload-child-spawn-strategy',
    sources,
    include_directories: [src_dir],
    dependencies: [dl_dep, dbus_dep],
    link_with: [zypak_base, zypak_dbus],
    install: true
)
