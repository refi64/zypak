// Copyright 2020 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "preload/main_override.h"

using namespace zypak::preload;

int MAIN_OVERRIDE(int argc, char** argv, char** envp) {
  InitUrandomFd();
  return true_main(argc, argv, envp);
}

INSTALL_MAIN_OVERRIDE()
