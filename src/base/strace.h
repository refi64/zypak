// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <optional>

#include "base/base.h"

namespace zypak {

// A set of functions to determine if the user requested strace to be run on the host or children.
class Strace {
 public:
  // An identifier for the type of process that is about to be run, to determine if it should be
  // traced.
  enum class Target { kHost, kChild };

  // Returns true if the user requested that the given type of process be traced.
  static bool ShouldTraceTarget(Target target);
  // Returns a filter to pass to 'strace -e' if given by the user; if none is given, returns an
  // empty optional.
  static std::optional<std::string_view> GetSyscallFilter();
};

}  // namespace zypak
