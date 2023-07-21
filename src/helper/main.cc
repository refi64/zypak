// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// zypak-helper is called by zypak-sandbox and is responsible for setting up the file descriptors
// and launching the target process.

#include <fcntl.h>

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

using ArgsView = std::vector<cstring_view>;

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

bool SanityCheckWidevinePath(cstring_view widevine_path) {
  if (!widevine_path.ends_with("WidevineCdm") && !widevine_path.ends_with("WidevineCdm/")) {
    Log() << "Rejecting potentially incorrect Widevine CDM path: " << widevine_path;
    return false;
  }

  return true;
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

std::string GetZypakLib(std::string_view libdir, std::string_view name) {
  return (fs::path(libdir) / ("libzypak-preload-"s + std::string(name) + ".so")).string();
}

std::string GetPreload(std::string_view mode, std::string_view libdir) {
  std::vector<std::string> preload_libs;

  // LD_PRELOAD is loaded in left-to-right order, and the last reference to a symbol is used to
  // resolve it. We want Zypak's symbols to have the highest priority, so we put any extra libraries
  // *first*, that way in the event of a conflict, Zypak's symbols will override all others.
  if (auto preload = Env::Get(Env::kZypakSettingLdPreload); preload && !preload->empty()) {
    preload_libs.push_back(std::string(*preload));
  }

  preload_libs.push_back(GetZypakLib(libdir, mode));
  if (Env::Test(Env::kZypakZygoteStrategySpawn)) {
    preload_libs.push_back(GetZypakLib(libdir, std::string(mode) + "-spawn-strategy"));
    if (mode == "host") {
      if (auto crlib = Env::Get(Env::kZypakSettingCefLibraryPath)) {
        preload_libs.emplace_back(*crlib);
      }

      preload_libs.push_back(GetZypakLib(libdir, "host-spawn-strategy-close"));
    }
  } else {
    preload_libs.push_back(GetZypakLib(libdir, std::string(mode) + "-mimic-strategy"));
  }

  return Join(preload_libs.begin(), preload_libs.end(), ":");
}

bool LooksLikeElfFile(cstring_view target) {
  unique_fd target_fd;

  if (target.starts_with("/")) {
    target_fd = open(target.c_str(), O_RDONLY);
    if (target_fd.invalid()) {
      return false;
    }
  } else {
    cstring_view path = Env::Require("PATH");
    std::vector<std::string_view> path_entries;
    SplitInto(path, ':', std::back_inserter(path_entries));

    for (auto entry : path_entries) {
      fs::path target_path = fs::path(entry) / target.c_str();
      Debug() << "Check " << target_path;
      if (access(target_path.c_str(), X_OK) == -1) {
        continue;
      }

      target_fd = open(target_path.c_str(), O_RDONLY);
      if (!target_fd.invalid()) {
        break;
      }
    }

    if (target_fd.invalid()) {
      Log() << "Failed to find full path for target executable '" << target << "'";
      return false;
    }
  }

  static constexpr std::array<std::byte, 4> kElfMagic = {std::byte(0x7F), std::byte('E'),
                                                         std::byte('L'), std::byte('F')};
  std::array<std::byte, kElfMagic.size()> magic;

  if (HANDLE_EINTR(read(target_fd.get(), magic.data(), magic.size())) == -1) {
    Errno() << "read() failed";
    return false;
  }

  if (magic != kElfMagic) {
    Log() << target << " is not an ELF file";
    if (magic[0] == std::byte('#') && magic[1] == std::byte('!')) {
      Log() << "(it appears to be a shell script?)";
    }
    return false;
  }

  return true;
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

  cstring_view mode = *it++;

  if (mode == "version") {
    printf("Zypak %s\n", ZYPAK_RELEASE);
    return 0;
  } else if (mode == "host-latest" || mode == "exec-latest") {
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

    if (auto widevine_path = Env::Get(Env::kZypakSettingExposeWidevinePath);
        widevine_path && !widevine_path->empty()) {
      if (!SanityCheckWidevinePath(*widevine_path)) {
        return 1;
      }
    }
  } else if (mode != "child") {
    Log() << "Invalid mode: " << mode;
    return 1;
  }

  if (!ApplyFdMapFromArgs(&it, args.end())) {
    return 1;
  }

  if (it == args.end()) {
    Log() << "Expected a command";
    return 1;
  }

  if (!LooksLikeElfFile(*it)) {
    Log() << "WARNING: supplied target " << *it << " does not look like a valid Chromium binary!";
    Log() << "Zypak needs to be called directly on the executable *binary*, not any wrappers.";
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
    new_command.push_back((new std::string("LD_PRELOAD="s + preload))->c_str());

    if (auto filter = Strace::GetSyscallFilter()) {
      new_command.push_back("-e");
      new_command.push_back(*filter);
    }

    if (!Strace::HasLineLimit()) {
      new_command.push_back("-v");
      new_command.push_back("-s1024");
      new_command.push_back("-k");
    }

    ExtendContainerCopy(&new_command, command);
    command = std::move(new_command);
  } else {
    Env::Set("LD_PRELOAD", preload);
  }

  std::vector<const char*> c_argv;
  c_argv.reserve(command.size() + 1);

  for (const auto& arg : command) {
    c_argv.push_back(arg.c_str());
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
