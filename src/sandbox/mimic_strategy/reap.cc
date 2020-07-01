// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mimic_strategy/reap.h"

#include <sys/wait.h>

#include "base/debug.h"

namespace zypak::sandbox::mimic_strategy {

namespace {

class ReapTimerHandler {
 public:
  ReapTimerHandler(Epoll* epoll, pid_t child_pid) : epoll_(epoll), child_pid_(child_pid) {}

  void AddToLoop() {
    constexpr int kSecondsToWaitForDeath = 2;
    ZYPAK_ASSERT(epoll_->AddTimerSec(kSecondsToWaitForDeath, *this));
  }

  void operator()(Epoll::SourceRef source) {
    pid_t wret = HANDLE_EINTR(waitpid(child_pid_, nullptr, WNOHANG));
    if (wret > 0) {
      // XXX: in original code this was a logged error, but can this really occur?
      ZYPAK_ASSERT(wret == child_pid_);
      // Return before the timer is re-added.
      return;
    }

    Debug() << "Wait for " << child_pid_ << " timed out, trying to kill then reap again";

    if (!sent_sigkill_ && kill(child_pid_, SIGKILL) == -1) {
      Errno() << "Kill of child " << child_pid_ << " failed";
    }

    sent_sigkill_ = true;
    AddToLoop();
  }

 private:
  Epoll* epoll_;
  pid_t child_pid_;
  bool sent_sigkill_ = false;
};

}  // namespace

void HandleReap(Epoll* ep, std::set<pid_t>* children, nickle::Reader* reader) {
  int child_pid;
  if (!reader->Read<nickle::codecs::Int>(&child_pid)) {
    Log() << "Failed to read reap arguments";
    return;
  }

  Debug() << "Reap of " << child_pid;

  if (auto it = children->find(child_pid); it != children->end()) {
    ReapTimerHandler reaper(ep, child_pid);
    reaper.AddToLoop();
  } else {
    Log() << "Failed to find child to reap: " << child_pid;
  }
}

}  // namespace zypak::sandbox::mimic_strategy
