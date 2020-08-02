// Copyright 2020 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Pretend that /proc/self/exe isn't accessible due to sandboxing.

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "base/base.h"
#include "preload/declare_override.h"

namespace {

constexpr std::string_view kSandboxTestPath = "/proc/self/exe";

}  // namespace

// The syscall *must* be used directly, as TCMalloc calls open very early on and therefore many
// memory allocations will cause it to crash.
DECLARE_OVERRIDE_THROW(int, open64, const char* path, int flags, ...) {
  int mode = 0;

  // Load the mode if needed.
  if (__OPEN_NEEDS_MODE(flags)) {
    va_list va;
    va_start(va, flags);
    mode = va_arg(va, int);
    va_end(va);
  }

  if (path == kSandboxTestPath) {
    errno = ENOENT;
    return -1;
  }

  // On x64 systems, off64_t and off_t are the same at the ABI level, so O_LARGEFILE
  // isn't needed.
  return syscall(__NR_openat, AT_FDCWD, path, flags, mode);
}
