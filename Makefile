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

preload_NAME := zypak-preload
preload_DEPS := base
preload_LIBS := -ldl
preload_SOURCES := \
	bwrap_pid.cc \
	exec_zypak_sandbox.cc \
	sandbox_path.cc \
	sandbox_suid.cc \

$(call build_shlib,preload)

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
	install -Dm 755 -t $(FLATPAK_DEST)/lib build/libzypak-preload.so
