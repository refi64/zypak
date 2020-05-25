// Copyright 2020 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Overrides waitpid / kill so they talk to the portal when needed.

#include <sys/signal.h>
#include <sys/wait.h>

#include "preload/declare_override.h"
#include "preload/host/spawn_strategy/supervisor.h"

using namespace zypak;
using namespace zypak::preload;

DECLARE_OVERRIDE(int, kill, pid_t pid, int sig) {
  auto original = LoadOriginal();
  Supervisor* supervisor = Supervisor::Acquire();

  if (pid <= 0) {
    Log() << "Warning: kill override ignores groups";
    return original(pid, sig);
  }

  Supervisor::Result result = supervisor->SendSignal(pid, sig);
  if (result == Supervisor::Result::kNotFound) {
    return original(pid, sig);
  } else if (result != Supervisor::Result::kOk) {
    errno = EIO;
    return -1;
  } else {
    return 0;
  }
}

DECLARE_OVERRIDE_THROW(pid_t, waitpid, pid_t pid, int* status, int options) {
  auto original = LoadOriginal();
  Supervisor* supervisor = Supervisor::Acquire();

  if (pid <= 0) {
    Log() << "Warning: waitpid override ignores groups";
    return original(pid, status, options);
  }

  if (options & (WUNTRACED | WCONTINUED)) {
    Log() << "Warning: waitpid override ignores WUNTRACED/WCONTINUED";
  }

  Supervisor::Result result = options & WNOHANG ? supervisor->GetExitStatus(pid, status)
                                                : supervisor->WaitForExitStatus(pid, status);
  if (result == Supervisor::Result::kNotFound) {
    return original(pid, status, options);
  } else if (result == Supervisor::Result::kTryLater) {
    ZYPAK_ASSERT(options & WNOHANG);
    return 0;
  } else if (result != Supervisor::Result::kOk) {
    errno = EIO;
    return -1;
  } else {
    return pid;
  }
}
