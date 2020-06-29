// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/spawn_strategy/spawn_launcher_delegate.h"

#include <nickle.h>

#include "base/container_util.h"
#include "base/debug.h"
#include "base/fd_map.h"
#include "base/socket.h"
#include "dbus/flatpak_portal_proxy.h"
#include "sandbox/launcher.h"
#include "sandbox/spawn_strategy/supervisor_communication.h"

namespace zypak::sandbox::spawn_strategy {

bool SpawnLauncherDelegate::Spawn(const Launcher::Helper& helper, std::vector<std::string> command,
                                  const FdMap& fd_map, EnvMap env,
                                  Launcher::Flags flags) /*override*/ {
  std::vector<std::string> full_command;

  // Since we map the descriptors ourselves via the portal API, there's no need for zypak-helper to
  // adjust them as well.
  ExtendContainerMove(&full_command, helper.BuildCommandWrapper(FdMap()));
  ExtendContainerMove(&full_command, std::move(command));

  std::optional<unique_fd> request_pipe = OpenSpawnRequest();
  if (!request_pipe) {
    return false;
  }

  std::vector<std::byte> target;
  nickle::buffers::ContainerBuffer buffer(&target);
  nickle::Writer writer(&buffer);

  ZYPAK_ASSERT(writer.Write<nickle::codecs::UInt32>(full_command.size()));
  for (const std::string& item : full_command) {
    ZYPAK_ASSERT(writer.Write<nickle::codecs::StringView>(item));
  }

  std::vector<int> fds;
  for (const FdAssignment& assignment : fd_map) {
    fds.push_back(assignment.fd().get());
    ZYPAK_ASSERT(writer.Write<nickle::codecs::UInt32>(assignment.target()));
  }

  ZYPAK_ASSERT(writer.Write<nickle::codecs::UInt32>(env.size()));
  for (const auto& [var, value] : env) {
    ZYPAK_ASSERT(writer.Write<nickle::codecs::StringView>(var));
    ZYPAK_ASSERT(writer.Write<nickle::codecs::StringView>(value));
  }

  int spawn_flags = dbus::FlatpakPortalProxy::kSpawnFlags_ExposePids |
                    dbus::FlatpakPortalProxy::kSpawnFlags_EmitSpawnStarted;
  int sandbox_flags = dbus::FlatpakPortalProxy::SpawnOptions::kNoSandboxFlags;

  if (flags & Launcher::kAllowGpu) {
    sandbox_flags |= dbus::FlatpakPortalProxy::SpawnOptions::kSandboxFlags_ShareGpu;
  }

  if (!(flags & Launcher::kAllowNetwork)) {
    spawn_flags |= dbus::FlatpakPortalProxy::kSpawnFlags_NoNetwork;
  }

  if (flags & Launcher::kSandbox) {
    spawn_flags |= dbus::FlatpakPortalProxy::kSpawnFlags_Sandbox;
  }

  if (flags & Launcher::kWatchBus) {
    spawn_flags |= dbus::FlatpakPortalProxy::kSpawnFlags_WatchBus;
  }

  ZYPAK_ASSERT(writer.Write<nickle::codecs::UInt32>(spawn_flags));
  ZYPAK_ASSERT(writer.Write<nickle::codecs::UInt32>(sandbox_flags));

  Socket::WriteOptions options;
  options.fds = &fds;
  if (!Socket::Write(request_pipe->get(), target, options)) {
    Errno() << "Failed to write spawn request data";
    return false;
  }

  // Includes null terminator.
  std::array<std::byte, kZypakSupervisorExitReply.size() + 1> reply;
  if (ssize_t bytes_read = Socket::Read(request_pipe->get(), &reply); bytes_read == -1) {
    Errno() << "Failed to wait for supervisor exit reply";
    return false;
  }

  Debug() << "Got supervisor exit message";
  ZYPAK_ASSERT(kZypakSupervisorExitReply == reinterpret_cast<char*>(reply.data()));

  return true;
}

std::optional<unique_fd> SpawnLauncherDelegate::OpenSpawnRequest() {
  auto sockets = Socket::OpenSocketPair();
  if (!sockets) {
    Log() << "Socket pair open failed, aborting spawn";
    return false;
  }

  auto [our_end, supervisor_end] = std::move(*sockets);

  std::vector<int> spawn_request_fds;
  spawn_request_fds.push_back(supervisor_end.get());

  Socket::WriteOptions options;
  options.fds = &spawn_request_fds;
  if (!Socket::Write(kZypakSupervisorFd, kZypakSupervisorSpawnRequest, options)) {
    Errno() << "Failed to send spawn request to supervisor";
    return {};
  }

  return std::move(our_end);
}

}  // namespace zypak::sandbox::spawn_strategy
