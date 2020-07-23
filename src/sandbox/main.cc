// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iostream>
#include <vector>

#include "base/base.h"
#include "base/debug.h"
#include "base/env.h"
#include "base/str_util.h"
#include "sandbox/zygote/zygote.h"

using namespace zypak;
using namespace zypak::sandbox;

int main(int argc, char** argv) {
  DebugContext::instance()->set_name("zypak-sandbox");
  DebugContext::instance()->LoadFromEnvironment();

  if (argc < 2) {
    Log() << "zypak-sandbox: wrong arguments";
    return 1;
  }

  std::vector<std::string> args(argv, argv + argc);
  if (args[1] == "--get-api") {
    // Mimic sandbox API version 1.
    std::cout << 1 << std::endl;
    return 0;
  } else if (args[1] == "--adjust-oom-score") {
    Debug() << "XXX ignoring --adjust-oom-score " << args[2] << ' ' << args[3];
    return 0;
  } else {
    if (std::find(args.begin() + 1, args.end(), "--type=zygote") == args.end()) {
      auto cmdline = Join(args.begin() + 1, args.end());
      Log() << "Ignoring non-Zygote command: " << cmdline;
      return 1;
    }

    if (!RunZygote()) {
      return 1;
    }
  }

  return 0;
}
