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

// Chrome 92 introduces its own close override:
// https://chromium-review.googlesource.com/q/I918d79c343c0027ee1ce4353c7acbe7c0e79d1dd This will
// mean that an override of "close" here will not take effect anymore. In order to work around it,
// we can also override glibc's __close instead, which Chromium's close override calls into.

DECLARE_OVERRIDE_THROW(int, __close, int fd) {
  if (fd == zypak::sandbox::kZypakSupervisorFd && zypak::preload::block_supervisor_fd_close) {
    return 0;
  }

  // Just use the syscall to avoid tons of latency from indirection.
  return syscall(__NR_close, fd);
}

DECLARE_OVERRIDE_THROW(int, close, int fd) { return __close_override_detail::__close(fd); }
