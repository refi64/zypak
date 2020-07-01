// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <optional>

#include "base/base.h"

namespace zypak {

class Strace {
 public:
  enum class Target { kHost, kChild };

  static bool ShouldTraceTarget(Target target);
  static std::optional<std::string_view> GetSyscallFilter();
};

}  // namespace zypak
