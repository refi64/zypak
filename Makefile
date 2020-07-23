CXX := clang++
CXXFLAGS := \
  	-pthread -Inickle -Isrc -std=c++17 -Wall -Werror

BUILD := build
OBJ := $(BUILD)/obj

include rules.mk

all :

base_NAME := base
base_SOURCES := \
	debug_internal/log_stream.cc \
	debug.cc \
	env.cc \
	fd_map.cc \
	socket.cc \

$(call build_stlib,base)

preload_PUBLIC_LIBS := -ldl

preload_host_SOURCE_DIR := preload/host
preload_host_NAME := zypak-preload-host
preload_host_DEPS := preload base
preload_host_SOURCES := \
	exec_zypak_sandbox.cc \
	sandbox_path.cc \
	sandbox_suid.cc \

$(call build_shlib,preload_host)

preload_child_SOURCE_DIR := preload/child
preload_child_NAME := zypak-preload-child
preload_child_DEPS := preload base
preload_child_SOURCES := \
	bwrap_pid.cc \

$(call build_shlib,preload_child)

sandbox_NAME := zypak-sandbox
sandbox_DEPS := base
sandbox_SOURCES := \
	epoll.cc \
	main.cc \
	zygote/fork.cc \
	zygote/reap.cc \
	zygote/status.cc \
	zygote/zygote.cc \

$(call build_exe,sandbox)

helper_NAME := zypak-helper
helper_DEPS := base
helper_SOURCES := \
	main.cc \

$(call build_exe,helper)

clean :
	rm -rf $(BUILD)

install : all
	install -Dm 755 -t $(FLATPAK_DEST)/bin zypak-wrapper.sh
	install -Dm 755 -t $(FLATPAK_DEST)/bin build/zypak-helper
	install -Dm 755 -t $(FLATPAK_DEST)/bin build/zypak-sandbox
	install -Dm 755 -t $(FLATPAK_DEST)/lib build/libzypak-preload-host.so
	install -Dm 755 -t $(FLATPAK_DEST)/lib build/libzypak-preload-child.so
