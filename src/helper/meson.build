sources = [
    'chroot_helper.cc',
    'main.cc',
    'spawn_latest.cc'
]

zypak_helper = executable(
    'zypak-helper',
    sources,
    include_directories: [src_dir],
    dependencies: [threads_dep, dbus_dep],
    link_with: [zypak_base, zypak_dbus],
    install: true
)
