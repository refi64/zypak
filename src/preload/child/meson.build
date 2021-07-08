sources = [
    'bwrap_pid.cc'
]

zypak_preload_host = shared_library(
    'zypak-preload-child',
    sources,
    include_directories: [src_dir],
    dependencies: [dl_dep],
    link_with: [zypak_base],
    install: true
)

subdir('spawn_strategy')
subdir('mimic_strategy')
