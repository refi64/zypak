.PHONY : compile_flags.txt

PKGS := dbus-1 libsystemd

CXX := clang++
CXXFLAGS := \
		-fstack-protector-all -Wall -Werror \
		-std=c++17 -g -pthread \
		-Inickle -Isrc \
		$(shell pkg-config --cflags $(PKGS))

LDLIBS := $(shell pkg-config --libs $(PKGS))

# CXXFLAGS += -Wno-sign-compare -Wno-unused-result
# CXX := g++

BUILD := build
OBJ := $(BUILD)/obj

include rules.mk

all :

base_NAME := base
base_CXXFLAGS := -fPIC  # for usage in the preload libs
base_SOURCES := \
	debug_internal/log_stream.cc \
	debug.cc \
	env.cc \
	evloop.cc \
	fd_map.cc \
	socket.cc \
	strace.cc \

$(call build_stlib,base)

dbus_NAME := dbus
dbus_PUBLIC_LIBS := $(LDLIBS)
dbus_CXXFLAGS := -fPIC  # for usage in the preload libs
dbus_SOURCES := \
	bus.cc \
	bus_error.cc \
	bus_readable_message.cc \
	bus_writable_message.cc \
	flatpak_portal_proxy.cc \
	internal/bus_thread.cc \

$(call build_stlib,dbus)

preload_PUBLIC_LIBS := -ldl

preload_host_SOURCE_DIR := preload/host
preload_host_NAME := zypak-preload-host
preload_host_DEPS := preload base
preload_host_SOURCES := \
	exec_zypak_sandbox.cc \
	sandbox_path.cc \
	sandbox_suid.cc \

$(call build_shlib,preload_host)

preload_host_spawn_strategy_SOURCE_DIR := preload/host/spawn_strategy
preload_host_spawn_strategy_NAME := zypak-preload-host-spawn-strategy
preload_host_spawn_strategy_DEPS := preload dbus base
preload_host_spawn_strategy_SOURCES := \
	bus_safe_fork.cc \
	initialize.cc \
	no_close_host_fd.cc \
	process_override.cc \
	supervisor.cc \

$(call build_shlib,preload_host_spawn_strategy)

preload_child_SOURCE_DIR := preload/child
preload_child_NAME := zypak-preload-child
preload_child_DEPS := preload base
preload_child_SOURCES := \
	bwrap_pid.cc \

$(call build_shlib,preload_child)

preload_child_spawn_strategy_SOURCE_DIR := preload/child/spawn_strategy
preload_child_spawn_strategy_NAME := zypak-preload-child-spawn-strategy
preload_child_spawn_strategy_DEPS := preload dbus base
preload_child_spawn_strategy_SOURCES := \
	chroot_fake.cc \

$(call build_shlib,preload_child_spawn_strategy)

sandbox_NAME := zypak-sandbox
sandbox_DEPS := dbus base
sandbox_SOURCES := \
	launcher.cc \
	main.cc \
	mimic_strategy/fork.cc \
	mimic_strategy/mimic_launcher_delegate.cc \
	mimic_strategy/reap.cc \
	mimic_strategy/status.cc \
	mimic_strategy/zygote.cc \
	spawn_strategy/run.cc \
	spawn_strategy/spawn_launcher_delegate.cc \

$(call build_exe,sandbox)

helper_NAME := zypak-helper
helper_DEPS := dbus base
helper_CXXFLAGS := $(shell pkg-config --cflags glib-2.0)
helper_LIBS := $(shell pkg-config --libs glib-2.0)
helper_SOURCES := \
	main.cc \
	determine_strategy.cc \

$(call build_exe,helper)

# XXX: Add glib-2.0 flags here so autocomplete works
compile_flags.txt :
	echo -xc++ $(CXXFLAGS) $(helper_CXXFLAGS) | tr ' ' '\n' > compile_flags.txt

clean :
	rm -rf $(BUILD)

install : all
	install -Dm 755 zypak-wrapper.sh $(FLATPAK_DEST)/bin/zypak-wrapper
	install -Dm 755 build/zypak-helper $(FLATPAK_DEST)/bin/zypak-helper
	install -Dm 755 build/zypak-sandbox $(FLATPAK_DEST)/bin/zypak-sandbox
	install -Dm 755 build/libzypak-preload.so $(FLATPAK_DEST)/lib/libzypak-preload.so
