// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/launcher.h"

#include <filesystem>
#include <iterator>
#include <string_view>
#include <unordered_map>

#include "base/debug.h"
#include "base/env.h"

namespace zypak::sandbox {

constexpr std::string_view kSandboxApiVar = "SBX_CHROME_API_PRV";
constexpr std::string_view kSandboxApiValue = "1";
constexpr std::string_view kSandboxPidNsVar = "SBX_PID_NS";
constexpr std::string_view kSandboxNetNsVar = "SBX_PID_NS";
constexpr std::string_view kSandboxNsEnabled = "1";

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

  env[kSandboxApiVar] = kSandboxApiValue;
  env[kSandboxPidNsVar] = kSandboxNsEnabled;
  env[kSandboxNetNsVar] = kSandboxNsEnabled;

  std::vector<std::string> full_command;
  auto helper_path = std::filesystem::path(bindir.data()) / "zypak-helper";
  full_command.push_back(helper_path.string());
  full_command.push_back("child");

  for (const auto& assignment : fd_map) {
    full_command.push_back(assignment.Serialize());
  }

  full_command.push_back("-");

  full_command.reserve(full_command.size() + command.size());
  std::move(command.begin(), command.end(), std::back_inserter(full_command));

  return delegate_->Spawn(std::move(full_command), fd_map, std::move(env),
                          static_cast<Flags>(flags));
}

}  // namespace zypak::sandbox
