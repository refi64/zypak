// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <unordered_map>

#include "base/base.h"
#include "base/fd_map.h"

namespace zypak::sandbox {

class Launcher {
 public:
  // XXX: This needs better type safety.
  enum Flags {
    kAllowGpu = 1 << 0,
    kAllowNetwork = 1 << 1,
    kSandbox = 1 << 2,
    kWatchBus = 1 << 3,
  };

  class Delegate {
   public:
    using EnvMap = std::unordered_map<std::string_view, std::string_view>;

    virtual ~Delegate() {}
    virtual bool Spawn(std::vector<std::string> command, const FdMap& fd_map, EnvMap env,
                       Flags flags) = 0;
  };

  Launcher(Delegate* delegate) : delegate_(delegate) {}

  bool Run(std::vector<std::string> command, const FdMap& fd_map);

 private:
  Delegate* delegate_;
};

}  // namespace zypak::sandbox
