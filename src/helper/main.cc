// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// zypak-helper is called by zypak-sandbox and is responsible for setting up the file descriptors
// and launching the target process.

#include <filesystem>
#include <set>
#include <vector>

#include "base/base.h"
#include "base/container_util.h"
#include "base/debug.h"
#include "base/env.h"
#include "base/fd_map.h"
#include "base/str_util.h"
#include "base/strace.h"
#include "dbus/bus.h"
#include "dbus/flatpak_portal_proxy.h"
#include "helper/chroot_helper.h"
#include "helper/spawn_latest.h"

using namespace zypak;

namespace fs = std::filesystem;

using ArgsView = std::vector<std::string_view>;

void DetermineZygoteStrategy() {
  Debug() << "Determining sandbox strategy...";

  if (auto spawn_strategy = Env::Get(Env::kZypakZygoteStrategySpawn)) {
    Log() << "Using spawn strategy test " << *spawn_strategy << " as set by environment";
    return;
  }

  dbus::Bus* bus = dbus::Bus::Acquire();
  ZYPAK_ASSERT(bus);

  dbus::FlatpakPortalProxy portal(bus);

  constexpr std::uint32_t kMinPortalSupportingSpawnStarted = 4;

  auto version = portal.GetVersionBlocking();
  auto supports = portal.GetSupportsBlocking();

  bus->Shutdown();

  if (!version) {
    Log() << "WARNING: Unknown portal version";
    return;
  } else if (*version < kMinPortalSupportingSpawnStarted) {
    Log() << "Portal v4 is not available";
    return;
  }

  if (!supports) {
    Log() << "WARNING: Unknown portal supports";
    return;
  } else if (!(*supports & dbus::FlatpakPortalProxy::kSupports_ExposePids)) {
    Log() << "Portal does not support expose-pids";
    return;
  }

  Debug() << "Spawn strategy is enabled";
  Env::Set(Env::kZypakZygoteStrategySpawn, "1");
}

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

std::string GetPreload(std::string_view mode, std::string_view libdir) {
  std::vector<std::string> preload_names;
  std::vector<std::string> preload_libs;

  preload_names.push_back(mode.data());
  if (Env::Test(Env::kZypakZygoteStrategySpawn)) {
    preload_names.push_back(std::string(mode) + "-spawn-strategy");
  } else if (mode == "child") {
    preload_names.push_back(std::string(mode) + "-mimic-strategy");
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
    Log() << "usage: zypak-helper [spawn-strategy-test|host-latest|exec-latest|host|child] ....";
    return 1;
  }

  std::string_view mode = *it++;

  if (mode == "host-latest" || mode == "exec-latest") {
    bool wrap_with_zypak = mode == "host-latest";
    if (!SpawnLatest(ArgsView(it, args.end()), wrap_with_zypak)) {
      return 1;
    }

    return 0;
  } else if (mode == "host" || mode == "spawn-strategy-test") {
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

  if (mode == "host" && Strace::ShouldTraceHost()) {
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

    if (!Strace::HasLineLimit()) {
      new_command.push_back("-v");
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
    auto helper = ChrootHelper::Spawn();
    if (!helper) {
      Log() << "Helper start failed, cannot continue";
      return 1;
    }

    std::string fd_s = std::to_string(helper->fd.release());
    std::string helper_s = std::to_string(helper->pid);

    Env::Set(kSandboxHelperFdVar, fd_s);
    Env::Set(kSandboxHelperPidVar, helper_s);
  }

  execvp(c_argv[0], const_cast<char* const*>(c_argv.data()));
  Errno() << "exec failed for " << Join(command.begin(), command.end());
  return 1;
}
