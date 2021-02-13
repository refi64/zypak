// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <unordered_map>

#include "base/base.h"
#include "base/fd_map.h"

namespace zypak::sandbox {

// A Launcher allows execution of a sandboxed process, wrapped using zypak-helper. It will
// automatically forward relevant environment variables.
class Launcher {
 public:
  // The flags that can be passed to control the sandbox options.
  // XXX: This needs better type safety.
  enum Flags {
    kAllowGpu = 1 << 0,
    kAllowNetwork = 1 << 1,
    kSandbox = 1 << 2,
    kWatchBus = 1 << 3,
  };

  // A holder for the zypak-helper process that will generate the command line from an FD map.
  class Helper {
   public:
    Helper(std::string helper_path, std::string child_type)
        : helper_path_(std::move(helper_path)), child_type_(std::move(child_type)) {}

    // Builds a zypak-helper command line that will wrap the command to run.
    std::vector<std::string> BuildCommandWrapper(const FdMap& fd_map) const;

   private:
    std::string helper_path_;
    std::string child_type_;
  };

  // A delegate is responsible for actually executing the command, given various requirements for
  // its launch. It should use the helper argument to build a command line that will be prepended
  // the the command itself.
  class Delegate {
   public:
    using EnvMap = std::unordered_map<std::string_view, std::string_view>;

    virtual ~Delegate() {}
    virtual bool Spawn(const Helper& helper, std::vector<std::string> command, const FdMap& fd_map,
                       EnvMap env, std::vector<std::string> exposed_paths, Flags flags) = 0;
  };

  Launcher(Delegate* delegate) : delegate_(delegate) {}

  // Runs the given command using the stored delegate.
  bool Run(std::vector<std::string> command, const FdMap& fd_map);

 private:
  Delegate* delegate_;
};

}  // namespace zypak::sandbox
