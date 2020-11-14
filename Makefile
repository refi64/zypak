.PHONY : compile_flags.txt

LIBSYSTEMD_CFLAGS := $(shell pkg-config --cflags libsystemd)
LIBSYSTEMD_LDLIBS := $(shell pkg-config --libs libsystemd)

DBUS_CFLAGS := $(shell pkg-config --cflags dbus-1)
DBUS_LDLIBS := $(shell pkg-config --libs dbus-1)

GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0)

CXX := clang++
CXXFLAGS := \
		-fstack-protector-all -Wall -Werror \
		-std=c++17 -g -pthread \
		-Inickle -Isrc \
		$(LIBSYSTEMD_CFLAGS) $(DBUS_CFLAGS) $(GTK_CFLAGS)

BUILD := build
OBJ := $(BUILD)/obj

include rules.mk

all :

# Anything that the preload libs depend on needs to be build with -fPIC, since the preload libs
# are so files can can only have objects built with -fPIC.
pic_NAME := pic
pic_PUBLIC_CXXFLAGS := -fPIC

base_NAME := base
base_DEPS := pic
base_PUBLIC_LIBS := $(LIBSYSTEMD_LDLIBS)
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
dbus_DEPS := pic
dbus_PUBLIC_LIBS := $(DBUS_LDLIBS)
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

preload_host_file_portal_SOURCE_DIR := preload/host/file_portal
preload_host_file_portal_NAME := zypak-preload-host-file-portal
preload_host_file_portal_DEPS := preload base
preload_host_file_portal_SOURCES := \
	casts.cc \
	native_dialog_actions.cc \
	use_native_file_chooser.cc \

$(call build_shlib,preload_host_file_portal)

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

preload_child_mimic_strategy_SOURCE_DIR := preload/child/mimic_strategy
preload_child_mimic_strategy_NAME := zypak-preload-child-mimic-strategy
preload_child_mimic_strategy_DEPS := preload base
preload_child_mimic_strategy_SOURCES := \
	initialize.cc \
	open_urandom.cc \
	urandom_fd.cc

$(call build_shlib,preload_child_mimic_strategy)

preload_child_spawn_strategy_SOURCE_DIR := preload/child/spawn_strategy
preload_child_spawn_strategy_NAME := zypak-preload-child-spawn-strategy
preload_child_spawn_strategy_DEPS := preload dbus base
preload_child_spawn_strategy_SOURCES := \
	chroot_fake.cc \

$(call build_shlib,preload_child_spawn_strategy)

sandbox_NAME := zypak-sandbox
sandbox_DEPS := base
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
helper_SOURCES := \
	chroot_helper.cc \
	main.cc \
	spawn_latest.cc \

$(call build_exe,helper)

compile_flags.txt :
	echo -xc++ $(CXXFLAGS) | tr ' ' '\n' > compile_flags.txt

clean :
	rm -rf $(BUILD)

install : all
	install -Dm 755 -t $(FLATPAK_DEST)/bin zypak-wrapper.sh
	ln -sf $(FLATPAK_DEST)/bin/zypak-wrapper{.sh,}
	install -Dm 755 -t $(FLATPAK_DEST)/bin build/zypak-helper
	install -Dm 755 -t $(FLATPAK_DEST)/bin build/zypak-sandbox
	install -Dm 755 -t $(FLATPAK_DEST)/lib build/libzypak-preload-host.so
	install -Dm 755 -t $(FLATPAK_DEST)/lib build/libzypak-preload-host-file-portal.so
	install -Dm 755 -t $(FLATPAK_DEST)/lib build/libzypak-preload-host-spawn-strategy.so
	install -Dm 755 -t $(FLATPAK_DEST)/lib build/libzypak-preload-child.so
	install -Dm 755 -t $(FLATPAK_DEST)/lib build/libzypak-preload-child-spawn-strategy.so
