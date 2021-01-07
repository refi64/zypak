// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include <atomic>
#include <fstream>
#include <string>

#include "base/base.h"
#include "preload/declare_override.h"

// If this process is sandboxed, pretend it's PID 1 to pass the sandbox client check. We know when
// the PID is about to be tested because getenv(SBX_CHROME_API_PRV) is called right before. Note
// that other places of the code in debug builds expect the pid to equal the tid, so the PID is
// only faked that one time.

namespace {

std::atomic<bool> fake_next(false);

constexpr std::string_view kSandboxApiEnv = "SBX_CHROME_API_PRV";

}  // namespace

// getenv(SBX_CHROME_API_PRV) always
DECLARE_OVERRIDE(char*, getenv, const char* name) {
  auto original = LoadOriginal();

  if (name == kSandboxApiEnv) {
    fake_next.store(true);
  }

  return original(name);
}

DECLARE_OVERRIDE(pid_t, getpid) {
  auto original = LoadOriginal();

  pid_t res = original();

  bool test = true;
  if (fake_next.compare_exchange_strong(test, false, std::memory_order_relaxed)) {
    return 1;
  }

  return res;
}
