// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mimic_strategy/mimic_launcher_delegate.h"

#include "base/container_util.h"
#include "base/debug.h"
#include "base/socket.h"
#include "base/str_util.h"
#include "sandbox/mimic_strategy/zygote.h"

namespace zypak::sandbox::mimic_strategy {

bool MimicLauncherDelegate::Spawn(const Launcher::Helper& helper, std::vector<std::string> command,
                                  const FdMap& fd_map, EnvMap env,
                                  Launcher::Flags flags) /*override*/ {
  std::vector<std::string> spawn_command;
  spawn_command.push_back("flatpak-spawn");

  if (flags & Launcher::kWatchBus) {
    spawn_command.push_back("--watch-bus");
  }
  if (!(flags & Launcher::kAllowNetwork)) {
    spawn_command.push_back("--no-network");
  }
  if (!(flags & Launcher::kAllowGpu) && (flags & Launcher::kSandbox)) {
    spawn_command.push_back("--sandbox");
  }

  for (const auto& [var, value] : env) {
    spawn_command.push_back("--env="s + var.data() + "=" + value.data());
  }

  for (const auto& assignment : fd_map) {
    spawn_command.push_back("--forward-fd="s + std::to_string(assignment.fd().get()));
  }

  ExtendContainerMove(&spawn_command, helper.BuildCommandWrapper(fd_map));
  ExtendContainerMove(&spawn_command, std::move(command));

  pid_t child = fork();
  if (child == -1) {
    Errno() << "fork";
    return false;
  }

  if (child == 0) {
    ExecZygoteChild(std::move(spawn_command));
    ZYPAK_ASSERT(false);
  }

  *pid_out_ = child;
  return true;
}

void MimicLauncherDelegate::ExecZygoteChild(std::vector<std::string> command) {
  if (close(kZygoteHostFd) == -1) {
    Errno() << "Failed to close Zygote host FD";
  }

  if (chdir("/") == -1) {
    Errno() << "Failed to chdir to /";
  }

  constexpr std::string_view kChildPing = "CHILD_PING";
  if (!Socket::Write(pid_oracle_.get(), kChildPing)) {
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

}  // namespace zypak::sandbox::mimic_strategy
