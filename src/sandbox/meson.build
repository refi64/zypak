sources = [
    'launcher.cc',
    'main.cc',
    'mimic_strategy/fork.cc',
    'mimic_strategy/mimic_launcher_delegate.cc',
    'mimic_strategy/reap.cc',
    'mimic_strategy/status.cc',
    'mimic_strategy/zygote.cc',
    'spawn_strategy/run.cc',
    'spawn_strategy/spawn_launcher_delegate.cc'
]

zypak_sandbox = executable(
    'zypak-sandbox',
    sources,
    include_directories: [src_dir, nickle_dir],
    dependencies: [dbus_dep],
    link_with: [zypak_base],
    install: true
)
