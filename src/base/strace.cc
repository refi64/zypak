// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "strace.h"

#include <iterator>
#include <set>

#include "base/env.h"
#include "base/str_util.h"

namespace zypak {

namespace {

constexpr std::string_view kStraceAll = "all";
constexpr std::string_view kStraceHost = "host";
constexpr std::string_view kStraceChild = "child";
constexpr std::string_view kStraceChildTypes = "child:";

}  // namespace

// static
bool Strace::ShouldTraceHost() {
  // If only tracing the trampolined process was requested, don't trace the parent
  if ((Env::Test(Env::kZypakSettingStraceNoTrampoline) && !Env::Test(Env::kZypakWasTrampolined)) ||
      // ...but otherwise, the parent is already being traced, so don't run strace again.
      (!Env::Test(Env::kZypakSettingStraceNoTrampoline) && Env::Test(Env::kZypakWasTrampolined))) {
    return false;
  }

  auto target_env = Env::Get(Env::kZypakSettingStrace);
  return target_env && (*target_env == kStraceAll || *target_env == kStraceHost);
}

// static
bool Strace::ShouldTraceChild(std::string_view child_type) {
  auto target_env = Env::Get(Env::kZypakSettingStrace);
  if (!target_env) {
    return false;
  }

  if (*target_env == kStraceAll || *target_env == kStraceChild) {
    return true;
  }

  if (StartsWith(*target_env, kStraceChildTypes)) {
    target_env->remove_prefix(kStraceChildTypes.length());

    std::set<std::string_view> types;
    SplitInto(*target_env, ',', std::inserter(types, types.end()));
    return types.find(child_type) != types.end();
  }

  return false;
}

// static
std::optional<cstring_view> Strace::GetSyscallFilter() {
  return Env::Get(Env::kZypakSettingStraceFilter);
}

// static
bool Strace::HasLineLimit() { return !Env::Test(Env::kZypakSettingStraceNoLineLimit); }

}  // namespace zypak
