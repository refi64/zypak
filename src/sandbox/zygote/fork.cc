// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base.h"
#include "base/debug.h"
#include "base/env.h"
#include "base/fd_map.h"
#include "base/str_util.h"
#include "base/unique_fd.h"

#include "../socket.h"
#include "command.h"
#include "fork.h"
#include "zygote.h"

#include <nickle.h>

#include <array>
#include <filesystem>
#include <signal.h>
#include <sys/wait.h>
#include <vector>

namespace fs = std::filesystem;

void ExecZygoteChild(unique_fd pid_oracle, std::vector<std::string> command) {
  close(kZygoteHostFd);

  if (chdir("/") == -1) {
    Errno() << "Failed to chdir to /";
  }

  constexpr std::string_view kChildPing = "CHILD_PING";
  if (!Socket::Write(pid_oracle.get(), kChildPing)) {
    Errno() << "Failed to send child ping message";
    ZYPAK_ASSERT(false);
  }

  std::vector<const char*> c_argv;

  for (const auto& arg : command) {
    c_argv.push_back(arg.c_str());
  }
  c_argv.push_back(nullptr);

  Debug() << "run as " << getpid() << ": " << Join(command.begin(), command.end());

  execvp(c_argv[0], const_cast<char* const*>(c_argv.data()));
  Errno() << "Failed to exec child process " << Join(command.begin(), command.end());
  ZYPAK_ASSERT(false);
}

bool TestChildPidFromHost() {
  std::array<std::byte, kZygoteMaxMessageLength> buffer;
  int child_pid_test = -1;

  ssize_t len = Socket::Read(kZygoteHostFd, &buffer, nullptr);
  if (len > 0) {
    nickle::buffers::ReadOnlyContainerBuffer nbuf(buffer);
    nickle::Reader reader(&nbuf);

    ZygoteCommand command;
    ZYPAK_ASSERT(reader.Read<ZygoteCommandCodec>(&command));
    ZYPAK_ASSERT(command == ZygoteCommand::kForkRealPID);
    ZYPAK_ASSERT(reader.Read<nickle::codecs::Int>(&child_pid_test));
  }

  return child_pid_test != -1;
}

std::optional<pid_t> SpawnZygoteChild(unique_fd pid_oracle, std::vector<std::string> args,
                                      FdMap fd_map) {
  namespace fs = std::filesystem;

  auto bindir = Env::Require("ZYPAK_BIN");
  auto libdir = Env::Require("ZYPAK_LIB");

  // Since /proc/self/exe now refers to zypak-sandbox rather than Chrome, rewrite it.
  if (args[0] == "/proc/self/exe") {
    auto caller_path = fs::read_symlink(fs::path("/proc") / std::to_string(getppid()) / "exe");
    args[0] = caller_path.string();
  }

  std::vector<std::string> spawn_args;
  spawn_args.push_back("flatpak-spawn");
  spawn_args.push_back("--watch-bus");

  constexpr std::string_view kAllowNetworkEnv = "ZYPAK_ALLOW_NETWORK";
  if (!Env::Test(kAllowNetworkEnv)) {
    spawn_args.push_back("--no-network");
  }

  constexpr std::string_view kDisableSandboxEnv = "ZYPAK_DISABLE_SANDBOX";
  if (!Env::Test(kDisableSandboxEnv)) {
    constexpr std::string_view kAllowGpuEnv = "ZYPAK_ALLOW_GPU";
    if (std::find(args.begin(), args.end(), "--type=gpu-process") == args.end()
        || Env::Test(kAllowGpuEnv)) {
      spawn_args.push_back("--sandbox");
    }
  }

  spawn_args.push_back("--env="s + Env::kZypakBin.data() + "=" + bindir.data());
  spawn_args.push_back("--env="s + Env::kZypakLib.data() + "=" + libdir.data());

  if (DebugContext::instance()->enabled()) {
    spawn_args.push_back("--env="s + DebugContext::kDebugEnv.data() + "=1");
  }

  for (const auto& assignment : fd_map) {
    spawn_args.push_back("--forward-fd="s + std::to_string(assignment.fd().get()));
  }

  auto helper_path = fs::path(bindir.data()) / "zypak-helper";
  spawn_args.push_back(helper_path.string());

  for (const auto& assignment : fd_map) {
    spawn_args.push_back(assignment.Serialize());
  }

  spawn_args.push_back("-");

  spawn_args.reserve(spawn_args.size() + args.size());
  std::copy(args.begin(), args.end(), std::back_inserter(spawn_args));

  pid_t child = fork();
  if (child == -1) {
    Errno() << "fork";
    return {};
  }

  if (child == 0) {
    ExecZygoteChild(std::move(pid_oracle), std::move(spawn_args));
    ZYPAK_ASSERT(false);
  } else {
    if (!TestChildPidFromHost()) {
      if (kill(child, SIGKILL) == -1) {
        Errno() << "Unexpected error reaping dead child";
      } else if (HANDLE_EINTR(waitpid(child, nullptr, 0)) == -1) {
        Errno() << "Unexpected error waiting for dead child";
      }

      child = -1;
    }

    std::vector<std::byte> buffer;
    nickle::buffers::ContainerBuffer nbuf(&buffer);
    nickle::Writer writer(&nbuf);

    writer.Write<nickle::codecs::Int>(child);
    // XXX: ignoring UMA args for now
    writer.Write<nickle::codecs::StringView>("");

    if (!Socket::Write(kZygoteHostFd, buffer, nullptr)) {
      Errno() << "Failed to send exec reply to zygote host";
    }

    return {child};
  }
}

std::optional<pid_t> HandleFork(nickle::Reader* reader, std::vector<unique_fd> fds) {
  std::string process_type;
  int argc;

  Debug() << "Handling fork request";

  std::vector<std::string> args;

  if (!reader->Read<nickle::codecs::String>(&process_type)
      || !reader->Read<nickle::codecs::Int>(&argc)) {
    Log() << "Failed to read fork process type and argc";
    return {};
  }

  args.reserve(argc);
  for (int i = 0; i < argc; i++) {
    std::string arg;
    if (!reader->Read<nickle::codecs::String>(&arg)) {
      Log() << "Failed to read argument #" << i;
      return {};
    }

    args.push_back(arg);
  }

  std::basic_string<std::uint16_t> timezone;
  if (!reader->Read<nickle::codecs::String16>(&timezone)) {
    Log() << "Failed to read timezone information";
    return {};
  }

  FdMap fd_map;

  int desired_fd_count = 0;
  if (!reader->Read<nickle::codecs::Int>(&desired_fd_count)) {
    Log() << "Failed to read fd count";
    return {};
  } else if (desired_fd_count != fds.size()) {
    Log() << "Given " << fds.size() << " fds, but pickle wants " << desired_fd_count;
    return {};
  }

  unique_fd pid_oracle(std::move(fds[0]));

  for (int i = 1; i < desired_fd_count; i++) {
    int key;
    if (!reader->Read<nickle::codecs::Int>(&key)) {
      Log() << "Failed to read fd # " << i;
      return {};
    }

    // The Zygote is given a Key rather than a file descriptor; it must be added to the base
    // descriptor to get a proper FD.
    constexpr int kBaseDescriptor = 3;
    int desired_fd = key + kBaseDescriptor;

    Debug() << "map fd " << fds[i].get() << " to " << desired_fd;
    fd_map.emplace_back(std::move(fds[i]), desired_fd);
  }

  // Sandbox service FD.
  fd_map.emplace_back(unique_fd(4), 4);

  return SpawnZygoteChild(std::move(pid_oracle), std::move(args), std::move(fd_map));
}
