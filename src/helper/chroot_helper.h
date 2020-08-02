// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <optional>
#include <string_view>

#include "base/base.h"
#include "base/unique_fd.h"

namespace zypak {

namespace {

ATTR_NO_WARN_UNUSED constexpr std::string_view kSandboxHelperFdVar = "SBX_D";
ATTR_NO_WARN_UNUSED constexpr std::string_view kSandboxHelperPidVar = "SBX_HELPER_PID";

}  // namespace

struct ChrootHelper {
  unique_fd fd;
  int pid;

  static std::optional<ChrootHelper> Spawn();
};

}  // namespace zypak
