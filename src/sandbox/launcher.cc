// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/launcher.h"

#include <filesystem>
#include <iterator>
#include <string_view>
#include <unordered_map>

#include "base/container_util.h"
#include "base/debug.h"
#include "base/env.h"

namespace zypak::sandbox {

constexpr std::string_view kSandboxApiVar = "SBX_CHROME_API_PRV";
constexpr std::string_view kSandboxApiValue = "1";
constexpr std::string_view kSandboxPidNsVar = "SBX_PID_NS";
constexpr std::string_view kSandboxNetNsVar = "SBX_PID_NS";
constexpr std::string_view kSandboxNsEnabled = "1";

std::vector<std::string> Launcher::Helper::BuildCommandWrapper(const FdMap& fd_map) const {
  std::vector<std::string> wrapper;

  // wrapper.push_back("strace");
  // wrapper.push_back("-F");

  wrapper.push_back(helper_path_);
  wrapper.push_back("child");

  for (const auto& assignment : fd_map) {
    wrapper.push_back(assignment.Serialize());
  }

  wrapper.push_back("-");

  return wrapper;
}

bool Launcher::Run(std::vector<std::string> command, const FdMap& fd_map) {
  int flags = kWatchBus;

  if (std::find(command.begin(), command.end(), "--type=gpu-process") != command.end() ||
      Env::Test(Env::kZypakSettingAllowGpu)) {
    flags |= kAllowGpu;
  }

  if (Env::Test(Env::kZypakSettingAllowNetwork)) {
    flags |= kAllowNetwork;
  }

  if (!Env::Test(Env::kZypakSettingDisableSandbox)) {
    flags |= kSandbox;
  }

  auto bindir = Env::Require(Env::kZypakBin);
  auto libdir = Env::Require(Env::kZypakLib);

  Delegate::EnvMap env;

  env[Env::kZypakBin] = bindir;
  env[Env::kZypakLib] = libdir;

  if (DebugContext::instance()->enabled()) {
    env[Env::kZypakSettingEnableDebug] = "1";
  }

  if (Env::Test(Env::kZypakZygoteStrategySpawn)) {
    env[Env::kZypakZygoteStrategySpawn] = "1";
  }

  env[kSandboxApiVar] = kSandboxApiValue;
  env[kSandboxPidNsVar] = kSandboxNsEnabled;
  env[kSandboxNetNsVar] = kSandboxNsEnabled;

  auto helper_path = std::filesystem::path(bindir.data()) / "zypak-helper";
  Helper helper(helper_path.string());

  return delegate_->Spawn(helper, std::move(command), fd_map, std::move(env),
                          static_cast<Flags>(flags));
}

}  // namespace zypak::sandbox
