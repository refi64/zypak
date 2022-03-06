// Copyright 2021 Endless OS Foundation, LLC.
// Copyright 2022 Ryan Gonzalez
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Ensure that, if this same binary is exec'd again, zypak-helper is added.

#include <sys/ptrace.h>

#include <cstdarg>
#include <vector>

#include "base/base.h"
#include "base/env.h"
#include "base/file_util.h"
#include "preload/declare_override.h"

using namespace zypak;

DECLARE_OVERRIDE(int, execv, const char* file, char* const* argv) {
  auto original = LoadOriginal();

  // Don't preload any libraries into whatever is getting exec'd.
  Env::Clear("LD_PRELOAD");

  if (PathsPointToSameFile(file, kCurrentExe)) {
    Env::Set(Env::kZypakWasTrampolined, "1");

    std::string helper(Env::Require(Env::kZypakBin));
    helper += "/zypak-helper";

    std::vector<const char*> c_argv;
    c_argv.push_back("zypak-helper");
    c_argv.push_back("host");

    for (; *argv != nullptr; argv++) {
      c_argv.push_back(*argv);
    }

    c_argv.push_back(nullptr);

    return original(helper.c_str(), const_cast<char* const*>(c_argv.data()));
  }

  return original(file, argv);
}
