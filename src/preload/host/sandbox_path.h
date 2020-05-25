// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/base.h"

// Keeps track of the known sandbox path.
class SandboxPath {
 public:
  std::string_view sandbox_path() const;
  void set_sandbox_path(std::string_view path);

  // Does the given path "look" like a sandbox path?
  static bool LooksLikeSandboxPath(std::string_view path);

  static SandboxPath* instance() { return &instance_; }

 private:
  static SandboxPath instance_;

  std::string sandbox_path_;
};
