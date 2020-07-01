// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstdint>
#include <unordered_map>

#include "base/base.h"
#include "base/fd_map.h"
#include "dbus/bus.h"
#include "dbus/bus_message.h"
#include "dbus/bus_readable_message.h"

namespace zypak::dbus {

class FlatpakPortalProxy {
 public:
  using SpawnReply = std::variant<std::uint32_t, InvocationError>;
  using SpawnReplyHandler = std::function<void(SpawnReply)>;

  struct SpawnStartedMessage {
    std::uint32_t external_pid;
    std::uint32_t internal_pid;
  };
  using SpawnStartedHandler = std::function<void(SpawnStartedMessage)>;

  struct SpawnExitedMessage {
    std::uint32_t external_pid;
    std::uint32_t exit_status;
  };
  using SpawnExitedHandler = std::function<void(SpawnExitedMessage)>;

  enum Supports : std::uint32_t {
    kSupports_ExposePids = 1 << 0,
  };

  enum SpawnFlags : std::uint32_t {
    kSpawnFlags_ClearEnv = 1 << 0,
    kSpawnFlags_SpawnLatest = 1 << 1,
    kSpawnFlags_Sandbox = 1 << 2,
    kSpawnFlags_NoNetwork = 1 << 3,
    kSpawnFlags_WatchBus = 1 << 4,
    kSpawnFlags_ExposePids = 1 << 5,
    kSpawnFlags_EmitSpawnStarted = 1 << 6,
  };

  static constexpr SpawnFlags kNoSpawnFlags = static_cast<SpawnFlags>(0);

  struct SpawnOptions {
    enum SandboxFlags : std::uint32_t {
      kSandboxFlags_ShareDisplay = 1 << 0,
      kSandboxFlags_ShareSound = 1 << 1,
      kSandboxFlags_ShareGpu = 1 << 2,
      kSandboxFlags_SessionBus = 1 << 3,
      kSandboxFlags_A11yBus = 1 << 4,
    };

    static constexpr SandboxFlags kNoSandboxFlags = static_cast<SandboxFlags>(0);
    SandboxFlags sandbox_flags = kNoSandboxFlags;
  };

  FlatpakPortalProxy(Bus* bus = nullptr) : bus_(bus) {}

  void AttachToBus(Bus* bus);
  dbus::Bus* bus() { return bus_; }

  std::optional<std::uint32_t> GetVersionBlocking();
  std::optional<Supports> GetSupportsBlocking();

  struct SpawnCall {
    SpawnCall() {}

    std::string_view cwd;
    std::vector<std::string> argv;
    const FdMap* fds = nullptr;
    std::unordered_map<std::string, std::string> env;
    SpawnFlags flags = kNoSpawnFlags;
    SpawnOptions options;
  };

  std::optional<SpawnReply> SpawnBlocking(SpawnCall spawn);
  void SpawnAsync(SpawnCall spawn, SpawnReplyHandler handler);
  std::optional<InvocationError> SpawnSignalBlocking(std::uint32_t pid, std::uint32_t signal);

  void SubscribeToSpawnStarted(SpawnStartedHandler handler);
  void SubscribeToSpawnExited(SpawnExitedHandler handler);

 private:
  static constexpr FloatingRef kFlatpakPortalRef =
      FloatingRef("org.freedesktop.portal.Flatpak", "/org/freedesktop/portal/Flatpak",
                  "org.freedesktop.portal.Flatpak");

  MethodCall BuildSpawnMethodCall(SpawnCall spawn);
  // static so it can be used easily in a callback without having to capture `this`
  static std::optional<SpawnReply> GetSpawnReply(Reply reply);

  std::optional<std::uint32_t> GetUint32PropertyBlocking(std::string_view name);

  Bus* bus_;
};

}  // namespace zypak::dbus
