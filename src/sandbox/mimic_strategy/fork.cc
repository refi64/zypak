// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mimic_strategy/fork.h"

#include <bits/c++config.h>
#include <sys/signal.h>
#include <sys/wait.h>

#include <filesystem>
#include <vector>

#include <nickle.h>

#include "base/base.h"
#include "base/debug.h"
#include "base/fd_map.h"
#include "base/socket.h"
#include "base/unique_fd.h"
#include "sandbox/launcher.h"
#include "sandbox/mimic_strategy/command.h"
#include "sandbox/mimic_strategy/mimic_launcher_delegate.h"
#include "sandbox/mimic_strategy/zygote.h"

namespace fs = std::filesystem;

namespace zypak::sandbox::mimic_strategy {

bool TestChildPidFromHost() {
  std::array<std::byte, kZygoteMaxMessageLength> buffer;
  int child_pid_test = -1;

  ssize_t len = Socket::Read(kZygoteHostFd, &buffer);
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

void SendChildInfoToHost(pid_t child) {
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

  if (!Socket::Write(kZygoteHostFd, buffer)) {
    Errno() << "Failed to send exec reply to zygote host";
  }
}

std::optional<pid_t> HandleFork(nickle::Reader* reader, std::vector<unique_fd> fds) {
  std::string process_type;
  int argc;

  Debug() << "Handling fork request";

  std::vector<std::string> args;

  if (!reader->Read<nickle::codecs::String>(&process_type) ||
      !reader->Read<nickle::codecs::Int>(&argc)) {
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

  // Since /proc/self/exe now refers to zypak-sandbox rather than Chrome,
  // rewrite it.
  if (args[0] == "/proc/self/exe") {
    auto caller_path = fs::read_symlink(fs::path("/proc") / std::to_string(getppid()) / "exe");
    args[0] = caller_path.string();
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
  } else if (desired_fd_count < 1) {
    Log() << "Too few FDs " << fds.size();
    return {};
  }

  unique_fd pid_oracle(std::move(fds[0]));

  for (int i = 1; i < desired_fd_count; i++) {
    int key;
    if (!reader->Read<nickle::codecs::Int>(&key)) {
      Log() << "Failed to read fd # " << i;
      return {};
    }

    // The Zygote is given a Key rather than a file descriptor; it must be added
    // to the base descriptor to get a proper FD.
    constexpr int kBaseDescriptor = 3;
    int desired_fd = key + kBaseDescriptor;

    Debug() << "map fd " << fds[i].get() << " to " << desired_fd;
    fd_map.emplace_back(std::move(fds[i]), desired_fd);
  }

  // Sandbox service FD.
  fd_map.emplace_back(unique_fd(4), 4);

  pid_t child;
  MimicLauncherDelegate launcher_delegate(std::move(pid_oracle), &child);
  Launcher launcher(&launcher_delegate);

  if (!launcher.Run(std::move(args), fd_map)) {
    return {};
  }

  SendChildInfoToHost(child);
  return child;
}

}  // namespace zypak::sandbox::mimic_strategy
