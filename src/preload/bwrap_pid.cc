// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base.h"
#include "preload/declare_override.h"

// If this process is sandboxed, pretend it's PID 1 to pass the sandbox client check.

DECLARE_OVERRIDE(pid_t, getpid) {
  auto original = LoadOriginal();

  pid_t res = original();
  if (res == 2 && getenv("SBX_CHROME_API_PRV")) {
    res = 1;
  }

  return res;
}
