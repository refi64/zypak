// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/base.h"
#include "base/launcher.h"
#include "dbus/flatpak_portal_proxy.h"

namespace zypak::preload {

class SpawnLauncherDelegate : public Launcher::Delegate {
 public:
  SpawnLauncherDelegate(dbus::FlatpakPortalProxy* portal,
                        dbus::FlatpakPortalProxy::SpawnReplyHandler handler)
      : portal_(portal), handler_(handler) {}

  bool Spawn(const Launcher::Helper& helper, std::vector<std::string> command, const FdMap& fd_map,
             EnvMap env, std::vector<std::string> exposed_paths, Launcher::Flags flags) override;

 private:
  bool was_called_ = false;
  dbus::FlatpakPortalProxy* portal_;
  dbus::FlatpakPortalProxy::SpawnReplyHandler handler_;
};

}  // namespace zypak::preload
