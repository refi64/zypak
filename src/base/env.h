// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <optional>

#include "base/base.h"
#include "base/cstring_view.h"

namespace zypak {

class Env {
 public:
  // Get the requested environment variable, returning an empty optional on failure.
  static std::optional<cstring_view> Get(cstring_view name);
  // Get the requested environment variable, aborting on failure.
  static cstring_view Require(cstring_view name);
  // Sets the given environment variable.
  static void Set(cstring_view name, cstring_view value, bool overwrite = true);
  // Clears the given environment variable.
  static void Clear(cstring_view name);
  // Tests if the variable is set to a truthy value (i.e. not empty, 0, or false).
  static bool Test(cstring_view name, bool default_value = false);

  static constexpr cstring_view kZypakBin = "ZYPAK_BIN";
  static constexpr cstring_view kZypakLib = "ZYPAK_LIB";
  static constexpr cstring_view kZypakZygoteStrategySpawn = "ZYPAK_ZYGOTE_STRATEGY_SPAWN";

  static constexpr cstring_view kZypakSettingEnableDebug = "ZYPAK_DEBUG";
  static constexpr cstring_view kZypakSettingStrace = "ZYPAK_STRACE";
  static constexpr cstring_view kZypakSettingStraceFilter = "ZYPAK_STRACE_FILTER";
  static constexpr cstring_view kZypakSettingStraceNoLineLimit = "ZYPAK_STRACE_NO_LINE_LIMIT";
  static constexpr cstring_view kZypakSettingDisableSandbox = "ZYPAK_DISABLE_SANDBOX";
  static constexpr cstring_view kZypakSettingAllowGpu = "ZYPAK_ALLOW_GPU";
  static constexpr cstring_view kZypakSettingSandboxFilename = "ZYPAK_SANDBOX_FILENAME";
  static constexpr cstring_view kZypakSettingExposeWidevinePath = "ZYPAK_EXPOSE_WIDEVINE_PATH";
  static constexpr cstring_view kZypakSettingLdPreload = "ZYPAK_LD_PRELOAD";
  static constexpr cstring_view kZypakSettingSpawnLatestOnReexec = "ZYPAK_SPAWN_LATEST_ON_REEXEC";
  static constexpr cstring_view kZypakSettingCefLibraryPath = "ZYPAK_CEF_LIBRARY_PATH";
};

}  // namespace zypak
