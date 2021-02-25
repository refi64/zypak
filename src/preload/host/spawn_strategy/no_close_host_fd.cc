// Copyright 2020 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Avoids closing the supervisor communication fd after fork and before exec.

#include "no_close_host_fd.h"

#include <syscall.h>
#include <unistd.h>

#include "base/base.h"
#include "preload/declare_override.h"
#include "sandbox/spawn_strategy/supervisor_communication.h"

bool zypak::preload::block_supervisor_fd_close = false;

DECLARE_OVERRIDE_THROW(int, close, int fd) {
  if (fd == zypak::sandbox::kZypakSupervisorFd && zypak::preload::block_supervisor_fd_close) {
    return 0;
  }

  // Just use the syscall to avoid tons of latency from indirection.
  return syscall(__NR_close, fd);
}
