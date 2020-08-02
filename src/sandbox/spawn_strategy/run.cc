// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "run.h"

#include <dirent.h>
#include <sys/prctl.h>
#include <sys/signal.h>

#include <memory>
#include <string>

#include "base/debug.h"
#include "base/fd_map.h"
#include "sandbox/launcher.h"
#include "sandbox/spawn_strategy/spawn_launcher_delegate.h"

namespace zypak::sandbox::spawn_strategy {

namespace {

struct DirDeleter {
  void operator()(DIR* dir) {
    if (dir != nullptr) {
      closedir(dir);
    }
  }
};

}  // namespace

std::optional<FdMap> FindAllFds() {
  std::unique_ptr<DIR, DirDeleter> fd_dir(opendir("/proc/self/fd"));
  if (!fd_dir) {
    Errno() << "Failed to open /proc/self/fd";
    return {};
  }

  std::vector<int> fds;

  struct dirent* dp;
  while ((dp = readdir(fd_dir.get())) != nullptr) {
    int fd = -1;
    try {
      fd = std::stoi(dp->d_name);
    } catch (std::exception& ex) {
      Debug() << "Ignoring /proc/self/fd/" << dp->d_name << ": " << ex.what();
    }

    // Do nothing if the FD was invalid or is our dirfd.
    if (fd != -1 && fd != dirfd(fd_dir.get())) {
      fds.push_back(fd);
    }
  }

  FdMap fd_map;
  for (int fd : fds) {
    unique_fd copy(dup(fd));
    if (copy.invalid()) {
      Errno() << "Failed to dup " << fd;
      return {};
    }

    fd_map.push_back(FdAssignment(std::move(copy), fd));
  }

  return std::move(fd_map);
}

bool RunSpawnStrategy(std::vector<std::string> args) {
  if (prctl(PR_SET_PDEATHSIG, SIGKILL, 0, 0, 0) == -1) {
    Errno() << "Warning: prctl(PDEATHSIG) failed";
  }

  std::optional<FdMap> fd_map = FindAllFds();
  if (!fd_map) {
    return false;
  }

  SpawnLauncherDelegate delegate;
  Launcher launcher(&delegate);

  return launcher.Run(std::move(args), *fd_map);
}

}  // namespace zypak::sandbox::spawn_strategy
