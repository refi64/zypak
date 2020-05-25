// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/base.h"
#include "base/unique_fd.h"
#include "sandbox/launcher.h"

namespace zypak::sandbox::mimic_strategy {

class MimicLauncherDelegate : public Launcher::Delegate {
 public:
  MimicLauncherDelegate(unique_fd pid_oracle, pid_t* pid_out)
      : pid_oracle_(std::move(pid_oracle)), pid_out_(pid_out) {}

  bool Spawn(std::vector<std::string> command, const FdMap& fd_map, EnvMap env,
             Launcher::Flags flags) override;

 private:
  void ExecZygoteChild(std::vector<std::string> command);

  unique_fd pid_oracle_;
  pid_t* pid_out_;
};

}  // namespace zypak::sandbox::mimic_strategy
