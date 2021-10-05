// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "base/base.h"
#include "base/enum_util.h"
#include "base/fd_map.h"
#include "dbus/bus.h"
#include "dbus/bus_message.h"
#include "dbus/bus_readable_message.h"

namespace zypak::dbus {

// A proxy that wraps a Bus instance for calling the Flatpak portal.
class FlatpakPortalProxy {
 public:
  using SpawnReply = std::variant<std::uint32_t, InvocationError>;
  using SpawnReplyHandler = std::function<void(SpawnReply)>;

  // The message emitted with the SpawnStarted signal.
  struct SpawnStartedMessage {
    std::uint32_t external_pid;
    std::uint32_t internal_pid;
  };
  using SpawnStartedHandler = std::function<void(SpawnStartedMessage)>;

  // The message emitted with the SpawnExited signal.
  struct SpawnExitedMessage {
    std::uint32_t external_pid;
    std::uint32_t exit_status;
  };
  using SpawnExitedHandler = std::function<void(SpawnExitedMessage)>;

  // The runtime options that the portal supports.
  enum Supports : std::uint32_t {
    kSupports_ExposePids = 1 << 0,
  };

  // The flags to be passed to the portal for spawning processes.
  enum class SpawnFlags : std::uint32_t {
    kClearEnv = 1 << 0,
    kSpawnLatest = 1 << 1,
    kSandbox = 1 << 2,
    kNoNetwork = 1 << 3,
    kWatchBus = 1 << 4,
    kExposePids = 1 << 5,
    kEmitSpawnStarted = 1 << 6,
  };

  static constexpr SpawnFlags kNoSpawnFlags = static_cast<SpawnFlags>(0);

  // The options to be passed to the portal for spawning processes.
  struct SpawnOptions {
    enum class SandboxFlags : std::uint32_t {
      kShareDisplay = 1 << 0,
      kShareSound = 1 << 1,
      kShareGpu = 1 << 2,
      kSessionBus = 1 << 3,
      kA11yBus = 1 << 4,
    };

    static constexpr SandboxFlags kNoSandboxFlags = static_cast<SandboxFlags>(0);
    SandboxFlags sandbox_flags = kNoSandboxFlags;

    std::vector<unique_fd> sandbox_expose_ro;

    bool ExposePathRo(std::string_view path);
  };

  FlatpakPortalProxy(Bus* bus = nullptr) : bus_(bus) {}

  // Attaches the portal to the given bus.
  void AttachToBus(Bus* bus);
  dbus::Bus* bus() { return bus_; }

  // Gets the current portal version.
  std::optional<std::uint32_t> GetVersionBlocking();
  // Gets the runtime flags supported by the portal.
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

  // Calls the Spawn method to spawn a new process using the given call options.
  std::optional<SpawnReply> SpawnBlocking(SpawnCall spawn);
  void SpawnAsync(SpawnCall spawn, SpawnReplyHandler handler);

  // Calls the SpawnSignal method to send a signal to a spawned process.
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

ZYPAK_DEFINE_ENUM_FLAGS(::zypak::dbus::FlatpakPortalProxy::SpawnFlags)
ZYPAK_DEFINE_ENUM_FLAGS(::zypak::dbus::FlatpakPortalProxy::SpawnOptions::SandboxFlags)
