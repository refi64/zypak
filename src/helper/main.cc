// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// zypak-helper is called by zypak-sandbox and is responsible for setting up the file descriptors
// and launching the target process.

#include "base/base.h"
#include "base/debug.h"
#include "base/env.h"
#include "base/fd_map.h"
#include "base/str_util.h"

#include <filesystem>
#include <set>
#include <vector>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
  DebugContext::instance()->set_name("zypak-helper");
  DebugContext::instance()->LoadFromEnvironment();

  std::vector<std::string_view> args(argv, argv + argc);
  auto it = args.begin() + 1;

  {
    FdMap fd_map;

    for (; it < args.end() && *it != "-"; it++) {
      if (auto assignment = FdAssignment::Deserialize(*it)) {
        fd_map.push_back(std::move(*assignment));
        Debug() << "Assignment: " << *it;
      } else {
        Log() << "Invalid fd assignment: " << *it;
        return 1;
      }
    }

    std::set<int> target_fds;
    for (auto& assignment : fd_map) {
      if (target_fds.find(assignment.fd().get()) != target_fds.end()
          || target_fds.find(assignment.target()) != target_fds.end()) {
        Log() << "Duplicate/overwriting fd assignment detected! Aborting...";
        return 1;
      }

      if (auto fd = assignment.Assign()) {
        target_fds.insert(fd->get());
        (void) fd->release();
      } else {
        return 1;
      }
    }
  }

  if (it == args.end()) {
    Log() << "too few arguments";
    return 1;
  }

  it++;

  std::vector<std::string_view> command(it, args.end());

  // Uncomment to debug via strace.
  /* auto i = command.insert(command.begin(), "strace"); */
  /* command.insert(++i, "-f"); */

  auto bindir = Env::Require("ZYPAK_BIN");
  auto libdir = Env::Require("ZYPAK_LIB");

  auto path = std::string(Env::Require("PATH")) + ":" + bindir.data();
  Env::Set("PATH", path);

  auto preload = (fs::path(libdir) / "libzypak-preload.so").string();
  Env::Set("LD_PRELOAD", preload);

  Env::Set("SBX_USER_NS", "1");
  Env::Set("SBX_PID_NS", "1");
  Env::Set("SBX_NET_NS", "1");

  std::vector<const char*> c_argv;
  c_argv.reserve(command.size() + 1);

  for (const auto& arg : command) {
    c_argv.push_back(arg.data());
  }

  c_argv.push_back(nullptr);

  Debug() << Join(command.begin(), command.end());

  execvp(c_argv[0], const_cast<char* const*>(c_argv.data()));
  Errno() << "exec failed for " << Join(command.begin(), command.end());
  return 1;
}
