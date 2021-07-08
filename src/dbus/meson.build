sources = [
    'bus.cc',
    'bus_error.cc',
    'bus_readable_message.cc',
    'bus_writable_message.cc',
    'flatpak_portal_proxy.cc',
    'internal/bus_thread.cc'
]

zypak_dbus = static_library(
    'zypak-dbus',
    sources,
    include_directories: [src_dir],
    dependencies: [dbus_dep]
)
