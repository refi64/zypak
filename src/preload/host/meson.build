sources = [
    'exec_zypak_sandbox.cc',
    'sandbox_path.cc',
    'sandbox_suid.cc'
]

zypak_preload_host = shared_library(
    'zypak-preload-host',
    sources,
    include_directories: [src_dir],
    dependencies: [dl_dep],
    link_with: [zypak_base],
    install: true
)

subdir('spawn_strategy')
