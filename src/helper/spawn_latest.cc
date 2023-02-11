// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "spawn_latest.h"

#include <filesystem>

#include "base/container_util.h"
#include "base/env.h"
#include "base/str_util.h"
#include "dbus/bus.h"
#include "dbus/flatpak_portal_proxy.h"

namespace zypak {

bool SpawnLatest(std::vector<cstring_view> args, bool wrap_with_zypak) {
  dbus::Bus* bus = dbus::Bus::Acquire();
  ZYPAK_ASSERT(bus);

  dbus::FlatpakPortalProxy portal(bus);

  std::unique_ptr<char> cwd(getcwd(nullptr, 0));

  dbus::FlatpakPortalProxy::SpawnCall spawn;
  spawn.cwd = cwd.get();

  if (Env::Test(Env::kZypakSettingDisableSandbox)) {
    spawn.env[Env::kZypakSettingDisableSandbox.ToOwned()] = "1";
  }

  if (wrap_with_zypak) {
    // XXX: This is similar to sandbox/launcher.cc
    spawn.env[Env::kZypakBin.ToOwned()] = Env::Require(Env::kZypakBin);
    spawn.env[Env::kZypakLib.ToOwned()] = Env::Require(Env::kZypakLib);

    if (auto widevine_path = Env::Get(Env::kZypakSettingExposeWidevinePath)) {
      spawn.env[Env::kZypakSettingExposeWidevinePath.ToOwned()] = *widevine_path;
    }

    if (auto sandbox_filename = Env::Get(Env::kZypakSettingSandboxFilename)) {
      spawn.env[Env::kZypakSettingSandboxFilename.ToOwned()] = *sandbox_filename;
    }

    if (auto ld_preload = Env::Get(Env::kZypakSettingLdPreload)) {
      spawn.env[Env::kZypakSettingLdPreload.ToOwned()] = *ld_preload;
    }

    auto helper = std::filesystem::path(Env::Require(Env::kZypakBin).ToOwned()) / "zypak-helper";
    spawn.argv.push_back(helper.string());
    spawn.argv.push_back("host");
    spawn.argv.push_back("-");
  }

  for (const cstring_view& arg : args) {
    spawn.argv.emplace_back(arg);
  }

  spawn.flags = dbus::FlatpakPortalProxy::SpawnFlags::kSpawnLatest;

  // Forward stdio.
  FdMap fds;
  for (int fd = 0; fd < 3; fd++) {
    unique_fd copy(dup(fd));
    if (copy.invalid()) {
      Errno() << "WARNING: Failed to dup " << fd;
      continue;
    }

    fds.push_back(FdAssignment(std::move(copy), fd));
  }
  spawn.fds = &fds;

  Debug() << "Spawn latest of " << Join(spawn.argv.begin(), spawn.argv.end());

  auto reply = portal.SpawnBlocking(std::move(spawn));
  if (!reply) {
    Log() << "SpawnBlocking failed with unknown result";
    return false;
  } else if (dbus::InvocationError* error = std::get_if<dbus::InvocationError>(&*reply)) {
    Log() << "SpawnBlocking failed with: " << *error;
    return false;
  }

  return true;
}

}  // namespace zypak
