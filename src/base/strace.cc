// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "strace.h"

#include <string_view>

#include "base/env.h"

namespace zypak {

// static
bool Strace::ShouldTraceTarget(Strace::Target target) {
  auto target_env = Env::Get(Env::kZypakSettingStrace);
  if (!target_env) {
    return false;
  } else if (*target_env == "all") {
    return true;
  }

  switch (target) {
  case Target::kHost:
    return *target_env == "host";
  case Target::kChild:
    return *target_env == "child";
  }
}

// static
std::optional<std::string_view> Strace::GetSyscallFilter() {
  return Env::Get(Env::kZypakSettingStraceFilter);
}

}  // namespace zypak
