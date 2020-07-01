// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// zypak-helper is called by zypak-sandbox and is responsible for setting up the file descriptors
// and launching the target process.

#include <sys/prctl.h>
#include <sys/signal.h>

#include <filesystem>
#include <set>
#include <variant>
#include <vector>

#include "base/base.h"
#include "base/container_util.h"
#include "base/debug.h"
#include "base/env.h"
#include "base/fd_map.h"
#include "base/socket.h"
#include "base/str_util.h"
#include "base/strace.h"
#include "dbus/bus.h"
#include "helper/determine_strategy.h"
#include "helper/spawn_latest.h"

using namespace zypak;

namespace fs = std::filesystem;

using ArgsView = std::vector<std::string_view>;

constexpr std::string_view kSandboxHelperFdVar = "SBX_D";
constexpr std::string_view kSandboxHelperPidVar = "SBX_HELPER_PID";

bool ApplyFdMapFromArgs(ArgsView::iterator* it, ArgsView::iterator last) {
  FdMap fd_map;

  for (; *it < last && **it != "-"; (*it)++) {
    if (auto assignment = FdAssignment::Deserialize(**it)) {
      fd_map.push_back(std::move(*assignment));
      Debug() << "Assignment: " << **it;
    } else {
      Log() << "Invalid fd assignment: " << **it;
      return false;
    }
  }

  if (*it == last) {
    Log() << "FD map ended too soon";
    return false;
  }

  (*it)++;

  std::set<int> target_fds;
  for (auto& assignment : fd_map) {
    if (target_fds.find(assignment.fd().get()) != target_fds.end() ||
        target_fds.find(assignment.target()) != target_fds.end()) {
      Log() << "Duplicate/overwriting fd assignment detected! Aborting...";
      return false;
    }

    if (auto fd = assignment.Assign()) {
      target_fds.insert(fd->get());
      (void)fd->release();
    } else {
      return false;
    }
  }

  return true;
}

bool StubSandboxChrootHelper(unique_fd fd) {
  if (prctl(PR_SET_PDEATHSIG, SIGKILL, 0, 0, 0) == -1) {
    Errno() << "Warning: Failed to prctl(DEATHSIG)";
  }

  Debug() << "Waiting for chroot request";

  std::array<std::byte, 1> msg;
  if (ssize_t bytes_read = Socket::Read(fd.get(), &msg); bytes_read == -1) {
    Errno() << "Failed to read from chroot message pipe";
    return false;
  }

  if (msg[0] == static_cast<std::byte>(0)) {
    Log() << "Chroot pipe is empty";
    return false;
  } else if (msg[0] != static_cast<std::byte>('C')) {
    Log() << "Unexpected chroot pipe message: " << static_cast<int>(msg[0]);
    return false;
  }

  Debug() << "Sending chroot reply";

  std::array<std::byte, 1> reply{static_cast<std::byte>('O')};
  if (!Socket::Write(fd.get(), reply)) {
    Errno() << "Failed to send chroot reply";
    return false;
  }

  Debug() << "Sent chroot reply";
  return true;
}

std::string GetPreload(std::string_view mode, std::string_view libdir) {
  std::vector<std::string> preload_names;
  std::vector<std::string> preload_libs;

  preload_names.push_back(mode.data());
  if (Env::Test(Env::kZypakZygoteStrategySpawn)) {
    preload_names.push_back(std::string(mode) + "-spawn-strategy");
  }

  for (std::string_view name : preload_names) {
    fs::path path = fs::path(libdir) / ("libzypak-preload-"s + name.data() + ".so");
    preload_libs.push_back(path.string());
  }

  return Join(preload_libs.begin(), preload_libs.end(), ":");
}

int main(int argc, char** argv) {
  DebugContext::instance()->set_name("zypak-helper");
  DebugContext::instance()->LoadFromEnvironment();

  ArgsView args(argv + 1, argv + argc);
  auto it = args.begin();

  if (it == args.end()) {
    Log() << "usage: zypak-helper [spawn-strategy-test|host-latest|host|child] ....";
    return 1;
  }

  std::string_view mode = *it++;

  if (mode == "host-latest") {
    if (!SpawnLatest(ArgsView(it, args.end()))) {
      return 1;
    }

    return 0;
  }

  if (mode == "host" || mode == "spawn-strategy-test") {
    DetermineZygoteStrategy();

    if (mode == "spawn-strategy-test") {
      return !Env::Test(Env::kZypakZygoteStrategySpawn);
    }
  } else if (mode == "child") {
    if (!ApplyFdMapFromArgs(&it, args.end())) {
      return 1;
    }
  } else {
    Log() << "Invalid mode: " << mode;
    return 1;
  }

  if (it == args.end()) {
    Log() << "Expected a command";
    return 1;
  }

  auto bindir = Env::Require(Env::kZypakBin);
  auto libdir = Env::Require(Env::kZypakLib);

  auto path = std::string(bindir) + ":" + std::string(Env::Require("PATH"));
  Env::Set("PATH", path);

  std::string preload = GetPreload(mode, libdir);
  Debug() << "Preload is: " << preload;

  ArgsView command(it, args.end());

  if (mode == "host" && Strace::ShouldTraceTarget(Strace::Target::kHost)) {
    ArgsView new_command;
    new_command.push_back("strace");
    new_command.push_back("-f");

    new_command.push_back("-E");
    // XXX: Ugly hack to avoid copying when *not* using strace.
    new_command.push_back((new std::string("LD_PRELOAD="s + preload))->data());

    if (auto filter = Strace::GetSyscallFilter()) {
      new_command.push_back("-e");
      new_command.push_back(*filter);
    }

    ExtendContainerCopy(&new_command, command);
    command = std::move(new_command);
  } else {
    Env::Set("LD_PRELOAD", preload);
  }

  std::vector<const char*> c_argv;
  c_argv.reserve(command.size() + 1);

  for (const auto& arg : command) {
    c_argv.push_back(arg.data());
  }

  c_argv.push_back(nullptr);

  Debug() << Join(command.begin(), command.end());

  if (Env::Test(Env::kZypakZygoteStrategySpawn) && mode == "child") {
    auto pair = Socket::OpenSocketPair();
    if (!pair) {
      return 1;
    }

    auto [parent_end, helper_end] = std::move(*pair);

    pid_t helper = fork();
    if (helper == -1) {
      Errno() << "Helper fork failed";
      return 1;
    } else if (helper == 0) {
      if (!StubSandboxChrootHelper(std::move(helper_end))) {
        return 1;
      }

      return 0;
    } else {
      std::string fd_s = std::to_string(parent_end.release());
      std::string helper_s = std::to_string(helper);

      Env::Set(kSandboxHelperFdVar, fd_s);
      Env::Set(kSandboxHelperPidVar, helper_s);
    }
  }

  execvp(c_argv[0], const_cast<char* const*>(c_argv.data()));
  Errno() << "exec failed for " << Join(command.begin(), command.end());
  return 1;
}
