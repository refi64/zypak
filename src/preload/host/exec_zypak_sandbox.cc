// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base.h"
#include "base/env.h"
#include "preload/declare_override.h"
#include "preload/host/sandbox_path.h"

// If exec is run, make sure it runs the zypak-provided sandbox binary instead of the normal
// Chrome one.

DECLARE_OVERRIDE(int, execvp, const char* file, char* const* argv) {
  auto original = LoadOriginal();

  if (file == SandboxPath::instance()->sandbox_path()) {
    file = "zypak-sandbox";
  }

  return original(file, argv);
}
