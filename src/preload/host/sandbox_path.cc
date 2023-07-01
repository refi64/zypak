// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "preload/host/sandbox_path.h"

#include "base/cstring_view.h"
#include "base/env.h"
#include "base/str_util.h"

namespace zypak {

namespace {

constexpr cstring_view kDefaultSandboxFilename = "chrome-sandbox";

}

cstring_view SandboxPath::sandbox_path() const { return sandbox_path_; }

void SandboxPath::set_sandbox_path(cstring_view path) { sandbox_path_ = path; }

bool SandboxPath::LooksLikeSandboxPath(cstring_view path) {
  if (sandbox_path_.empty()) {
    cstring_view sandbox =
        Env::Get(Env::kZypakSettingSandboxFilename).value_or(kDefaultSandboxFilename);
    std::string suffix = "/"s + sandbox.ToOwned();
    return path.ends_with(suffix);
  }

  return sandbox_path_ == path;
}

// static
SandboxPath SandboxPath::instance_;

}  // namespace zypak
