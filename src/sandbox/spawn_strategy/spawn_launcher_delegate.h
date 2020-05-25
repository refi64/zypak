// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/base.h"
#include "sandbox/launcher.h"

namespace zypak::sandbox::spawn_strategy {

class SpawnLauncherDelegate : public Launcher::Delegate {
 public:
  SpawnLauncherDelegate() {}

  bool Spawn(std::vector<std::string> command, const FdMap& fd_map, EnvMap env,
             Launcher::Flags flags) override;

 private:
  std::optional<unique_fd> OpenSpawnRequest();
};

}  // namespace zypak::sandbox::spawn_strategy
