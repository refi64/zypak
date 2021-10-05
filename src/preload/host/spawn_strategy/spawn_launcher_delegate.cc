// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "preload/host/spawn_strategy/spawn_launcher_delegate.h"

#include <nickle.h>

#include "base/container_util.h"
#include "base/debug.h"
#include "base/fd_map.h"
#include "base/socket.h"
#include "dbus/flatpak_portal_proxy.h"
#include "sandbox/spawn_strategy/supervisor_communication.h"

namespace zypak::preload {

bool SpawnLauncherDelegate::Spawn(const Launcher::Helper& helper, std::vector<std::string> command,
                                  const FdMap& fd_map, EnvMap env,
                                  std::vector<std::string> exposed_paths,
                                  Launcher::Flags flags) /*override*/ {
  ZYPAK_ASSERT(!was_called_);

  constexpr cstring_view kSpawnDirectory = "/";

  dbus::FlatpakPortalProxy::SpawnCall spawn;
  spawn.cwd = kSpawnDirectory;

  // Since we map the descriptors ourselves via the portal API, there's no need for zypak-helper to
  // adjust them as well.
  ExtendContainerMove(&spawn.argv, helper.BuildCommandWrapper(FdMap()));
  ExtendContainerMove(&spawn.argv, std::move(command));

  spawn.fds = &fd_map;

  for (const auto& [var, value] : env) {
    spawn.env.emplace(var, value);
  }

  for (const auto& path : exposed_paths) {
    spawn.options.ExposePathRo(path);
  }

  spawn.flags = static_cast<dbus::FlatpakPortalProxy::SpawnFlags>(
      dbus::FlatpakPortalProxy::SpawnFlags::kExposePids |
      dbus::FlatpakPortalProxy::SpawnFlags::kEmitSpawnStarted |
      dbus::FlatpakPortalProxy::SpawnFlags::kNoNetwork);
  spawn.options.sandbox_flags = dbus::FlatpakPortalProxy::SpawnOptions::kNoSandboxFlags;

  if (flags & Launcher::Flags::kAllowGpu) {
    spawn.options.sandbox_flags |= dbus::FlatpakPortalProxy::SpawnOptions::SandboxFlags::kShareGpu;
  }

  if (flags & Launcher::Flags::kSandbox) {
    spawn.flags |= dbus::FlatpakPortalProxy::SpawnFlags::kSandbox;
  }

  if (flags & Launcher::Flags::kWatchBus) {
    spawn.flags |= dbus::FlatpakPortalProxy::SpawnFlags::kWatchBus;
  }

  portal_->SpawnAsync(std::move(spawn), std::move(handler_));
  was_called_ = true;

  return true;
}

}  // namespace zypak::preload
