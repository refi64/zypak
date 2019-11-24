CXX := clang++
CXXFLAGS := \
  	-pthread -Isrc -std=c++2a -Wall -Werror \
	-Inickle

BUILD := build
OBJ := $(BUILD)/obj

include rules.mk

all :

base_NAME := base
base_SOURCES := \
	debug.cc \
	env.cc \
	fd_map.cc \

$(call build_stlib,base)

preload_NAME := zypak-preload
preload_DEPS := base
preload_LIBS := -ldl
preload_SOURCES := \
	bwrap_pid.cc \
	exec_zypak_sandbox.cc \
	sandbox_suid.cc \

$(call build_shlib,preload)

sandbox_NAME := zypak-sandbox
sandbox_DEPS := base
sandbox_SOURCES := \
	epoll.cc \
	main.cc \
	socket.cc \
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
	install -Dm 755 zypak-wrapper.sh $(FLATPAK_DEST)/bin/zypak-wrapper
	install -Dm 755 build/zypak-helper $(FLATPAK_DEST)/bin/zypak-helper
	install -Dm 755 build/zypak-sandbox $(FLATPAK_DEST)/bin/zypak-sandbox
	install -Dm 755 build/libzypak-preload.so $(FLATPAK_DEST)/lib/libzypak-preload.so
