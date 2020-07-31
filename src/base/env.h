// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <optional>

#include "base/base.h"

namespace zypak {

class Env {
 public:
  // Get the requested environment variable, returning an empty optional on failure.
  static std::optional<std::string_view> Get(std::string_view name);
  // Get the requested environment variable, aborting on failure.
  static std::string_view Require(std::string_view name);
  // Sets the given environment variable.
  static void Set(std::string_view name, std::string_view value, bool overwrite = true);
  // Tests if the variable is set to a truthy value (i.e. not empty, 0, or false).
  static bool Test(std::string_view name);

  static constexpr std::string_view kZypakBin = "ZYPAK_BIN";
  static constexpr std::string_view kZypakLib = "ZYPAK_LIB";
  static constexpr std::string_view kZypakZygoteStrategySpawn = "ZYPAK_ZYGOTE_STRATEGY_SPAWN";

  static constexpr std::string_view kZypakSettingEnableDebug = "ZYPAK_DEBUG";
  static constexpr std::string_view kZypakSettingStrace = "ZYPAK_STRACE";
  static constexpr std::string_view kZypakSettingStraceFilter = "ZYPAK_STRACE_FILTER";
  static constexpr std::string_view kZypakSettingAllowNetwork = "ZYPAK_ALLOW_NETWORK";
  static constexpr std::string_view kZypakSettingDisableSandbox = "ZYPAK_DISABLE_SANDBOX";
  static constexpr std::string_view kZypakSettingAllowGpu = "ZYPAK_ALLOW_GPU";
};

}  // namespace zypak
