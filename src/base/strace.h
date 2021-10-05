// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <optional>

#include "base/base.h"
#include "base/cstring_view.h"

namespace zypak {

// A set of functions to determine if the user requested strace to be run on the host or children.
class Strace {
 public:
  // Returns true if the user requested that the given type of process be traced.
  static bool ShouldTraceHost();
  static bool ShouldTraceChild(std::string_view child_type);
  // Returns a filter to pass to 'strace -e' if given by the user; if none is given, returns an
  // empty optional.
  static std::optional<cstring_view> GetSyscallFilter();
  static bool HasLineLimit();
};

}  // namespace zypak
