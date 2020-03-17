// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox_path.h"

static bool EndsWith(std::string_view str, std::string_view suffix) {
  return str.size() >= suffix.size() && str.substr(str.size() - suffix.size()) == suffix;
}

std::string_view SandboxPath::sandbox_path() const { return sandbox_path_; }

void SandboxPath::set_sandbox_path(std::string_view path) { sandbox_path_ = path; }

bool SandboxPath::LooksLikeSandboxPath(std::string_view path) {
  return EndsWith(path, "/chrome-sandbox");
}

/*static*/
SandboxPath SandboxPath::instance_;
