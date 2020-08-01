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

// If this process is sandboxed, pretend it's PID 1 to pass the sandbox client check.

namespace {

enum class State { kUnknown, kFake, kNoFake };

std::atomic<State> state(State::kUnknown);

}  // namespace

DECLARE_OVERRIDE(pid_t, getpid) {
  auto original = LoadOriginal();

  pid_t res = original();

  // NOTE: This is technically racy, but the worst case scenario is just that this is run twice.
  if (getenv("SBX_CHROME_API_PRV")) {
    if (state.load() == State::kUnknown) {
      if (res == 2) {
        state = State::kFake;
      } else {
        // Check if the parent is strace, if so still override the value.
        std::string path("/proc/"s + std::to_string(getppid()) + "/comm");
        std::ifstream is(path);
        std::string line;
        if (is && std::getline(is, line) && line == "strace") {
          state = State::kFake;
        } else {
          state = State::kNoFake;
        }
      }
    }

    if (state.load() == State::kFake) {
      return 1;
    }
  }

  return res;
}
