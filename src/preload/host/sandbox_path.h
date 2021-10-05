// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/base.h"
#include "base/cstring_view.h"

namespace zypak {

// Keeps track of the known sandbox path.
class SandboxPath {
 public:
  cstring_view sandbox_path() const;
  void set_sandbox_path(cstring_view path);

  // If the given path the sandbox path, or if the sandbox path is unset, does the given path "look"
  // like a sandbox path?
  bool LooksLikeSandboxPath(cstring_view path);

  static SandboxPath* instance() { return &instance_; }

 private:
  static SandboxPath instance_;

  std::string sandbox_path_;
};

}  // namespace zypak
