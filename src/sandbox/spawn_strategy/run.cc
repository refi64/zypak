// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "run.h"

#include <dirent.h>
#include <sys/prctl.h>
#include <sys/signal.h>

#include <memory>
#include <string>
#include <vector>

#include "base/debug.h"
#include "base/fd_map.h"
#include "base/launcher.h"
#include "base/socket.h"
#include "base/unique_fd.h"
#include "nickle.h"
#include "sandbox/spawn_strategy/supervisor_communication.h"

namespace zypak::sandbox::spawn_strategy {

namespace {

struct DirDeleter {
  void operator()(DIR* dir) {
    if (dir != nullptr) {
      closedir(dir);
    }
  }
};

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

    // Do nothing if the FD was invalid or one we don't want to forward.
    if (fd != -1 && fd != dirfd(fd_dir.get()) && fd != kZypakSupervisorFd) {
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

std::optional<unique_fd> OpenSpawnRequest() {
  auto sockets = Socket::OpenSocketPair();
  if (!sockets) {
    Log() << "Socket pair open failed, aborting spawn";
    return false;
  }

  auto [our_end, supervisor_end] = std::move(*sockets);

  std::vector<int> spawn_request_fds;
  spawn_request_fds.push_back(supervisor_end.get());

  Socket::WriteOptions options;
  options.fds = &spawn_request_fds;
  if (!Socket::Write(kZypakSupervisorFd, kZypakSupervisorSpawnRequest, options)) {
    Errno() << "Failed to send spawn request to supervisor";
    return {};
  }

  return std::move(our_end);
}

bool SendSpawnRequest(int request_pipe, const std::vector<std::string>& args, const FdMap& fd_map) {
  std::vector<std::byte> target;
  nickle::buffers::ContainerBuffer buffer(&target);
  nickle::Writer writer(&buffer);

  ZYPAK_ASSERT(writer.Write<nickle::codecs::UInt64>(args.size()));
  for (const std::string& arg : args) {
    ZYPAK_ASSERT(writer.Write<nickle::codecs::String>(arg));
  }

  std::vector<int> fds;
  for (const FdAssignment& assignment : fd_map) {
    fds.push_back(assignment.fd().get());
    ZYPAK_ASSERT(writer.Write<nickle::codecs::UInt32>(assignment.target()));
  }

  Socket::WriteOptions options;
  options.fds = &fds;
  if (!Socket::Write(request_pipe, target, options)) {
    Errno() << "Failed to write spawn request data";
    return false;
  }

  return true;
}

bool ReadExitReply(int request_pipe) {
  // +1 to includes null terminator.
  std::array<std::byte, kZypakSupervisorExitReply.size() + 1> reply;
  ssize_t bytes_read = Socket::Read(request_pipe, &reply);
  if (bytes_read == -1) {
    Errno() << "Failed to wait for supervisor exit reply";
    return false;
  }

  Debug() << "Got supervisor exit message";

  bool force_closed = bytes_read == 0;
  ZYPAK_ASSERT(force_closed || kZypakSupervisorExitReply == reinterpret_cast<char*>(reply.data()));

  return true;
}

}  // namespace

bool RunSpawnStrategy(std::vector<std::string> args) {
  if (prctl(PR_SET_PDEATHSIG, SIGKILL, 0, 0, 0) == -1) {
    Errno() << "Warning: prctl(PDEATHSIG) failed";
  }

  std::optional<FdMap> fd_map = FindAllFds();
  if (!fd_map) {
    return false;
  }

  std::optional<unique_fd> request_pipe = OpenSpawnRequest();
  if (!request_pipe) {
    return false;
  }

  return SendSpawnRequest(request_pipe->get(), args, *fd_map) && ReadExitReply(request_pipe->get());
}

}  // namespace zypak::sandbox::spawn_strategy
