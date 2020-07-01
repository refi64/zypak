// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glib.h>

#include <condition_variable>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>

#include "base/debug.h"
#include "base/env.h"
#include "base/guarded_value.h"
#include "base/str_util.h"
#include "dbus/bus.h"
#include "dbus/bus_readable_message.h"
#include "dbus/flatpak_portal_proxy.h"

namespace zypak {

namespace {

bool CheckRequiredPortalFeatures(dbus::FlatpakPortalProxy* portal) {
  Debug() << "Testing portal features";

  constexpr std::uint32_t kMinPortalSupportingSpawnStarted = 4;

  auto version = portal->GetVersionBlocking();
  if (!version) {
    Log() << "WARNING: Unknown portal version";
    return false;
  } else if (*version < kMinPortalSupportingSpawnStarted) {
    Debug() << "Portal v4 is not available";
    return false;
  }

  auto supports = portal->GetSupportsBlocking();
  if (!supports) {
    Log() << "WARNING: Unknown portal supports";
    return false;
  } else if (!(*supports & dbus::FlatpakPortalProxy::kSupports_ExposePids)) {
    Debug() << "Portal does not support expose-pids";
    return false;
  }

  return true;
}

bool IsDeviceAllPermissionGranted() {
  constexpr std::string_view kFlatpakInfoPath = "/.flatpak-info";
  constexpr std::string_view kContextSection = "Context";
  constexpr std::string_view kDevicesKey = "devices";

  g_autoptr(GKeyFile) key_file = g_key_file_new();
  g_autoptr(GError) error = nullptr;

  ZYPAK_ASSERT(
      g_key_file_load_from_file(key_file, kFlatpakInfoPath.data(), G_KEY_FILE_NONE, &error),
      << error->message);

  std::size_t values;
  g_auto(GStrv) devices = g_key_file_get_string_list(key_file, kContextSection.data(),
                                                     kDevicesKey.data(), &values, &error);
  if (devices == nullptr) {
    Debug() << "Failed to get devices key: " << error->message;
    return false;
  }

  char** last = devices + values;
  return std::find(devices, last, "all") != last;
}

// XXX: SubscribeToSpawnExited doesn't have any way of canceling a subscription without just killing
// the bus, and the handler needs somewhere to store the found pids. It can't be stored on the
// stack, because, IsExposePidsBroken returns *before* the bus is destroyed. Thus, this object is
// created once its parent's stack and passed around.
struct ExposePidsStorage {
  NotifyingGuardedValue<std::unordered_map<std::uint32_t, std::uint32_t>> pid_exit_statuses;
};

bool IsExposePidsBroken(dbus::FlatpakPortalProxy* portal, ExposePidsStorage* storage) {
  // Make sure https://github.com/flatpak/flatpak/issues/3722 won't bite us
  if (!IsDeviceAllPermissionGranted()) {
    return false;
  }

  portal->SubscribeToSpawnExited([storage](dbus::FlatpakPortalProxy::SpawnExitedMessage message) {
    auto pid_exit_statuses = storage->pid_exit_statuses.Acquire(GuardReleaseNotify::kAll);
    pid_exit_statuses->emplace(message.external_pid, message.exit_status);
  });

  dbus::FlatpakPortalProxy::SpawnCall spawn;
  spawn.cwd = "/";
  spawn.argv = {"true"};
  spawn.fds = nullptr;
  spawn.flags = static_cast<dbus::FlatpakPortalProxy::SpawnFlags>(
      dbus::FlatpakPortalProxy::kSpawnFlags_Sandbox |
      dbus::FlatpakPortalProxy::kSpawnFlags_ExposePids |
      dbus::FlatpakPortalProxy::kSpawnFlags_WatchBus);

  auto reply = portal->SpawnBlocking(std::move(spawn));
  if (!reply) {
    Log() << "Unknown error running expose-pids test";
    return false;
  }

  if (dbus::InvocationError* error = std::get_if<dbus::InvocationError>(&*reply)) {
    Log() << "Failed to run expose-pids test: " << *error;
    return false;
  }

  std::uint32_t* pid = std::get_if<std::uint32_t>(&*reply);
  ZYPAK_ASSERT(pid != nullptr);

  auto pid_exit_statuses = storage->pid_exit_statuses.AcquireWhen([pid](auto* pid_exit_statuses) {
    return pid_exit_statuses->find(*pid) != pid_exit_statuses->end();
  });

  auto it = pid_exit_statuses->find(*pid);
  ZYPAK_ASSERT(it != pid_exit_statuses->end());

  // If the command failed, then this is considered broken.
  return it->second != 0;
}

}  // namespace

void DetermineZygoteStrategy() {
  Debug() << "Determining spawn strategy...";

  if (auto spawn_strategy = Env::Get(Env::kZypakZygoteStrategySpawn)) {
    Log() << "Using spawn strategy test " << *spawn_strategy << " as set by environment";
    return;
  }

  dbus::Bus* bus = dbus::Bus::Acquire();
  ZYPAK_ASSERT(bus);

  dbus::FlatpakPortalProxy portal(bus);
  ExposePidsStorage storage;

  if (CheckRequiredPortalFeatures(&portal) && !IsExposePidsBroken(&portal, &storage)) {
    Debug() << "Spawn strategy is enabled";
    Env::Set(Env::kZypakZygoteStrategySpawn, "1");
  } else {
    Debug() << "Spawn strategy is not supported; using mimic strategy";
    Env::Set(Env::kZypakZygoteStrategySpawn, "0");
  }

  bus->Shutdown();
}

}  // namespace zypak
