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

// Overriding close is...tricky.
//
// Chrome 92 introduces its own close override:
// https://chromium-review.googlesource.com/q/I918d79c343c0027ee1ce4353c7acbe7c0e79d1dd
// This will mean that an override of "close" here will not take effect anymore. In order to work
// around it, we can also override glibc's __close instead, which Chromium's close override calls
// into.
//
// Later versions change it to call the close from dlsym(RTLD_NEXT) instead of hardcoding glibc's
// close:
// https://chromium-review.googlesource.com/c/chromium/src/+/4418528
// But, if close() is being called from a shared library like libcef, then RTLD_NEXT is *not*
// going to point to Zypak, because the resolution order will look like:
//
// LD_PRELOAD(zypak is here) -> libcef.so -> ... -> libc.so
//
// That's why this override is in its own shared library, so it can be added to the LD_PRELOAD list
// after the main overrides & with libcef in-between, resulting in a resolution order like:

// LD_PRELOAD(zypak main preload -> libcef.so -> this preload library w/ close) -> ... -> libc.so

DECLARE_OVERRIDE_THROW(int, __close, int fd) {
  if (fd == zypak::sandbox::kZypakSupervisorFd && zypak::preload::block_supervisor_fd_close) {
    return 0;
  }

  // Just use the syscall to avoid tons of latency from indirection.
  return syscall(__NR_close, fd);
}

DECLARE_OVERRIDE_THROW(int, close, int fd) { return __close_override_detail::__close(fd); }
