// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/launcher.h"

#include <unistd.h>

#include <filesystem>
#include <iterator>
#include <string_view>
#include <unordered_map>

#include "base/container_util.h"
#include "base/debug.h"
#include "base/env.h"
#include "base/str_util.h"
#include "base/strace.h"

namespace zypak {

constexpr cstring_view kSandboxApiVar = "SBX_CHROME_API_PRV";
constexpr cstring_view kSandboxApiValue = "1";
constexpr cstring_view kSandboxPidNsVar = "SBX_PID_NS";
constexpr cstring_view kSandboxNetNsVar = "SBX_NET_NS";
constexpr cstring_view kSandboxNsEnabled = "1";

constexpr cstring_view kXdgConfigHomeVar = "XDG_CONFIG_HOME";

constexpr char kPreloadDelimiters[] = ": ";

std::vector<std::string> Launcher::Helper::BuildCommandWrapper(const FdMap& fd_map) const {
  std::vector<std::string> wrapper;

  if (Strace::ShouldTraceChild(child_type_)) {
    wrapper.push_back("strace");
    wrapper.push_back("-f");

    if (auto filter = Strace::GetSyscallFilter()) {
      wrapper.push_back("-e");
      wrapper.push_back(filter->data());
    }

    if (!Strace::HasLineLimit()) {
      wrapper.push_back("-v");
    }
  }

  wrapper.push_back(helper_path_);
  wrapper.push_back("child");

  for (const auto& assignment : fd_map) {
    wrapper.push_back(assignment.Serialize());
  }

  wrapper.push_back("-");

  return wrapper;
}

bool Launcher::Run(std::vector<std::string> command, const FdMap& fd_map) {
  // XXX: similar to HasTypeArg in the preload code
  std::string child_type;
  constexpr std::string_view kTypeArgPrefix = "--type=";
  for (const std::string& arg : command) {
    if (arg.starts_with(kTypeArgPrefix)) {
      child_type = arg.substr(kTypeArgPrefix.size());
      break;
    }
  }

  Flags flags = Flags::kWatchBus;

  if (child_type == "gpu-process" || Env::Test(Env::kZypakSettingAllowGpu)) {
    flags |= Flags::kAllowGpu;
  }

  if (!Env::Test(Env::kZypakSettingDisableSandbox)) {
    flags |= Flags::kSandbox;
  }

  auto bindir = Env::Require(Env::kZypakBin);
  auto libdir = Env::Require(Env::kZypakLib);

  Delegate::EnvMap env;

  env[Env::kZypakBin] = bindir;
  env[Env::kZypakLib] = libdir;

  if (DebugContext::instance()->enabled()) {
    env[Env::kZypakSettingEnableDebug] = "1";
  }

  if (auto spawn_strategy = Env::Get(Env::kZypakZygoteStrategySpawn)) {
    env[Env::kZypakZygoteStrategySpawn] = *spawn_strategy;
  }

  env[kSandboxApiVar] = kSandboxApiValue;
  env[kSandboxPidNsVar] = kSandboxNsEnabled;
  env[kSandboxNetNsVar] = kSandboxNsEnabled;

  std::vector<std::string> exposed_paths;

  if (auto preload = Env::Get(Env::kZypakSettingLdPreload)) {
    env[Env::kZypakSettingLdPreload] = *preload;

    // Make sure to expose the individual library filenames into the sandbox.
    SplitInto(*preload, kPreloadDelimiters, std::back_inserter(exposed_paths),
              PieceType<std::string>{});
  }

  if (auto path = Env::Get(Env::kZypakSettingExposeWidevinePath);
      path && !path->empty() && access(path->data(), F_OK) == 0) {
    exposed_paths.push_back(std::string(path->data()));

    // The Widevine data is found relative to $XDG_CONFIG_HOME, which is not set
    // by default when running a sandboxed process.
    env[kXdgConfigHomeVar] = Env::Require(kXdgConfigHomeVar);
  }

  auto helper_path = std::filesystem::path(bindir.ToOwned()) / "zypak-helper";
  Helper helper(helper_path.string(), child_type);

  return delegate_->Spawn(helper, std::move(command), fd_map, std::move(env),
                          std::move(exposed_paths), static_cast<Flags>(flags));
}

}  // namespace zypak
