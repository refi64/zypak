project('zypak', 'cpp',
  version : '2021.06',
  default_options : ['warning_level=3', 'cpp_std=c++17'])

libsystemd_dep = dependency('libsystemd')
dbus_dep = dependency('dbus-1')
threads_dep = dependency('threads')

nickle_dir = include_directories('nickle')

subdir('src')
