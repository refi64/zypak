// Copyright 2023 Ryan Gonzalez
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/env.h"
#include "preload/main_override.h"

using namespace zypak;

int MAIN_OVERRIDE(int argc, char** argv, char** envp) {
  Env::Clear("LD_PRELOAD");
  return true_main(argc, argv, envp);
}

INSTALL_MAIN_OVERRIDE()
