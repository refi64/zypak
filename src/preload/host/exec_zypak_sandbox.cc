// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>

#include <cstring>
#include <vector>

#include "base/base.h"
#include "base/env.h"
#include "preload/declare_override.h"
#include "preload/host/sandbox_path.h"

// If exec is run, make sure it runs the zypak-provided sandbox binary instead of the normal
// Chrome one.

namespace {

bool HasTypeArg(char* const* argv) {
  constexpr std::string_view kTypeArgPrefix = "--type=";

  for (; *argv != nullptr; argv++) {
    if (strncmp(*argv, kTypeArgPrefix.data(), kTypeArgPrefix.size()) == 0) {
      return true;
    }
  }

  return false;
}

}  // namespace

DECLARE_OVERRIDE(int, execvp, const char* file, char* const* argv) {
  auto original = LoadOriginal();

  if (file == SandboxPath::instance()->sandbox_path()) {
    file = "zypak-sandbox";
  } else if (!HasTypeArg(argv)) {
    // Check if the file is identical to the current exe, from a re-exec.
    struct stat self_st, exec_st;

    if (stat("/proc/self/exe", &self_st) == -1 || stat(file, &exec_st) == -1) {
      // Pretend the exec just failed.
      errno = -ENOENT;
      return -1;
    }

    if (self_st.st_ino == exec_st.st_ino) {
      // exec on the host exe, so pass it through the sandbox.
      // "Leaking" calls to 'new' doesn't matter here since we're about to exec anyway.
      std::vector<const char*> c_argv;
      c_argv.push_back("zypak-helper");
      c_argv.push_back("host-latest");

      for (; *argv != nullptr; argv++) {
        c_argv.push_back(*argv);
      }

      c_argv.push_back(nullptr);

      return original("zypak-helper", const_cast<char* const*>(c_argv.data()));
    }
  }

  return original(file, argv);
}
