// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/stat.h>

#include <cstring>
#include <string_view>
#include <vector>

#include "base/base.h"
#include "base/env.h"
#include "preload/declare_override.h"
#include "preload/host/sandbox_path.h"

// If exec is run, make sure it runs the zypak-provided sandbox binary instead of the normal
// Chrome one.

namespace {

using namespace zypak;

bool IsCurrentExe(cstring_view exec) {
  struct stat self_st, exec_st;
  return stat("/proc/self/exe", &self_st) != -1 && stat(exec.c_str(), &exec_st) != -1 &&
         self_st.st_ino == exec_st.st_ino;
}

bool HasTypeArg(char* const* argv) {
  constexpr cstring_view kTypeArgPrefix = "--type=";

  for (; *argv != nullptr; argv++) {
    if (strncmp(*argv, kTypeArgPrefix.c_str(), kTypeArgPrefix.size()) == 0) {
      return true;
    }
  }

  return false;
}

}  // namespace

DECLARE_OVERRIDE(int, execvp, const char* file, char* const* argv) {
  auto original = LoadOriginal();

  if (*argv == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (file == SandboxPath::instance()->sandbox_path()) {
    file = "zypak-sandbox";
  } else if (!HasTypeArg(argv)) {
    Env::Clear("LD_PRELOAD");

    if (IsCurrentExe(file)) {
      // exec on the host exe, so pass it through the sandbox.
      // "Leaking" calls to 'new' doesn't matter here since we're about to exec anyway.
      std::vector<const char*> c_argv;
      c_argv.push_back("zypak-helper");

      // Swap out the main binary to the wrapper if one was used, and assume the wrapper will use
      // zypak-wrapper.sh itself (i.e. we don't need to handle it here).
      if (auto wrapper = Env::Get("CHROME_WRAPPER")) {
        c_argv.push_back("exec-latest");
        c_argv.push_back(wrapper->data());
        argv++;
      } else {
        c_argv.push_back("host-latest");
      }

      for (; *argv != nullptr; argv++) {
        c_argv.push_back(*argv);
      }

      c_argv.push_back(nullptr);

      return original("zypak-helper", const_cast<char* const*>(c_argv.data()));
    }
  }

  return original(file, argv);
}
