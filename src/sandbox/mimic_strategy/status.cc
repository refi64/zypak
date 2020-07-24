// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mimic_strategy/status.h"

#include <sys/wait.h>

#include <set>

#include <nickle.h>

#include "base/base.h"
#include "base/debug.h"
#include "base/socket.h"
#include "sandbox/mimic_strategy/command.h"
#include "sandbox/mimic_strategy/zygote.h"

namespace zypak::sandbox::mimic_strategy {

static ZygoteTerminationStatus GetTerminationStatusForSignal(int signal) {
  if (signal == SIGINT || signal == SIGKILL || signal == SIGTERM) {
    return ZygoteTerminationStatus::kKilled;
  } else {
    return ZygoteTerminationStatus::kCrashed;
  }
}

void HandleTerminationStatusRequest(std::set<pid_t>* children, nickle::Reader* reader) {
  bool known_dead;
  int child_pid;

  if (!reader->Read<nickle::codecs::Bool>(&known_dead) ||
      !reader->Read<nickle::codecs::Int>(&child_pid)) {
    Log() << "Failed to read termination status arguments";
    return;
  }

  Debug() << "Termination status request  " << known_dead << ' ' << child_pid;

  ZygoteTerminationStatus child_status = ZygoteTerminationStatus::kNormal;
  int wstatus = 0;

  if (auto it = children->find(child_pid); it != children->end()) {
    if (known_dead) {
      if (kill(child_pid, SIGKILL) == -1) {
        Errno() << "Failed to kill child process " << child_pid;
      }
    }

    int wstatus_backup;
    pid_t wret = HANDLE_EINTR(waitpid(child_pid, &wstatus_backup, known_dead ? 0 : WNOHANG));
    if (wret == -1) {
      Errno() << "Failed to wait for child process " << child_pid;
    } else if (wret == 0) {
      child_status = ZygoteTerminationStatus::kRunning;
    } else {
      wstatus = wstatus_backup;

      if (WIFSIGNALED(wstatus)) {
        child_status = GetTerminationStatusForSignal(WTERMSIG(wstatus));
      } else if (WIFEXITED(wstatus)) {
        int ret_code = WEXITSTATUS(wstatus);
        if (ret_code != 0) {
          // If the exit code is above 128, it's probably a signal:
          // https://blogs.gnome.org/wjjt/2018/06/08/when-is-an-exit-code-not-an-exit-code/
          if (ret_code > 128) {
            int sig = ret_code - 128;
            wstatus = W_STOPCODE(sig);
            child_status = GetTerminationStatusForSignal(sig);
          } else {
            child_status = ZygoteTerminationStatus::kAbnormal;
          }
        }
      }
    }

    if (child_status != ZygoteTerminationStatus::kRunning) {
      children->erase(it);
    }
  }

  std::vector<std::byte> buffer;
  nickle::buffers::ContainerBuffer nbuf(&buffer);
  nickle::Writer writer(&nbuf);

  Debug() << "Child status: " << static_cast<int>(child_status)
          << ", Zygote exit code (wstatus): " << wstatus;

  ZYPAK_ASSERT(writer.Write<ZygoteTerminationStatusCodec>(child_status));
  ZYPAK_ASSERT(writer.Write<nickle::codecs::Int32>(wstatus));

  if (!Socket::Write(kZygoteHostFd, buffer, nullptr)) {
    Errno() << "Failed to send termination status reply to zygote host";
  }
}

void HandleSandboxStatusRequest() {
  Debug() << "Sandbox status request";

  constexpr int kFlagSUID = 1 << 0, kFlagPidNS = 1 << 1, kFlagNetNS = 1 << 2, kBPF = 1 << 3,
                /* kYama      = 1 << 4, */
      kBPFTsync = 1 << 5;

  // XXX
  int flags = kFlagSUID | kFlagPidNS | kFlagNetNS | kBPF | kBPFTsync;
  if (!Socket::Write(kZygoteHostFd, reinterpret_cast<std::byte*>(&flags), sizeof(flags), nullptr)) {
    Errno() << "Failed to write sandbox status";
  }
}

}  // namespace zypak::sandbox::mimic_strategy
