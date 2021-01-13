// Copyright 2020 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "preload/child/mimic_strategy/fd_storage.h"
#include "preload/declare_override.h"

using namespace zypak;
using namespace zypak::preload;

// Chromium lazily opens a file descriptor to urandom the first time it needs a random number and
// re-uses it later on. This generally works well, but when using the mimic strategy, because a new
// subprocess is started for every child (i.e. the zygote is not used), this is not pre-initialized
// and then re-used across all forked processes. Therefore, when the ppapi process tries to load
// some random numbers, it can't open /dev/urandom now because the BPF sandbox has already been
// applied, and thus it fails.
// The workaround is to open /dev/urandom on process start, then use that saved file descriptor
// whenever Chromium later tries to open it. It's saved in initialize.cc, and here, it gets sent to
// Chromium when it tries to open it.

DECLARE_OVERRIDE_THROW(int, open64, const char* pathname, int flags, ...) {
  mode_t mode = 0;
  va_list va;
  va_start(va, flags);
  if (flags & (O_CREAT | O_TMPFILE)) {
    mode = va_arg(va, mode_t);
  }
  va_end(va);

  if (pathname == "/dev/urandom"sv) {
    // If fd == -1, the urandom fd hasn't been loaded yet (so this is probably during
    // initialization).
    if (const unique_fd& fd = FdStorage::instance()->urandom_fd(); !fd.invalid()) {
      return fd.get();
    }
  }

  // tcmalloc doesn't like if we use dlopen early on, so just call the syscall directly.
  return syscall(__NR_openat, AT_FDCWD, pathname, flags, mode);
}
