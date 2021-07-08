sources = [
    'fd_storage.cc',
    'initialize.cc',
    'localtime.cc',
    'open_urandom.cc',
]

zypak_preload_child_spawn_strategy = shared_library(
    'zypak-preload-child-mimic-strategy',
    sources,
    include_directories: [src_dir, nickle_dir],
    dependencies: [dl_dep],
    link_with: [zypak_base],
    install: true
)
