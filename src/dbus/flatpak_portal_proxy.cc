// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/flatpak_portal_proxy.h"

#include <fcntl.h>

#include "base/debug.h"
#include "dbus/bus_message.h"
#include "dbus/bus_readable_message.h"
#include "dbus/bus_writable_message.h"

namespace zypak::dbus {

void FlatpakPortalProxy::SpawnOptions::ExposePathRo(std::string_view path) {
  unique_fd fd(HANDLE_EINTR(open(path.data(), O_PATH | O_NOFOLLOW)));
  if (fd.invalid()) {
    Errno() << "Failed to expose '" << path << "' into sandbox";
    return;
  }

  sandbox_expose_ro.push_back(std::move(fd));
}

void FlatpakPortalProxy::AttachToBus(Bus* bus) {
  ZYPAK_ASSERT(bus_ == nullptr);
  bus_ = bus;
}

std::optional<std::uint32_t> FlatpakPortalProxy::GetVersionBlocking() {
  return GetUint32PropertyBlocking("version");
}

std::optional<FlatpakPortalProxy::Supports> FlatpakPortalProxy::GetSupportsBlocking() {
  if (auto supports = GetUint32PropertyBlocking("supports")) {
    return static_cast<Supports>(*supports);
  } else {
    return {};
  }
}

std::optional<FlatpakPortalProxy::SpawnReply> FlatpakPortalProxy::SpawnBlocking(SpawnCall spawn) {
  MethodCall method_call = BuildSpawnMethodCall(std::move(spawn));
  return GetSpawnReply(bus_->CallBlocking(method_call));
}

void FlatpakPortalProxy::SpawnAsync(FlatpakPortalProxy::SpawnCall spawn,
                                    SpawnReplyHandler handler) {
  MethodCall method_call = BuildSpawnMethodCall(std::move(spawn));
  bus_->CallAsync(std::move(method_call), [handler](Reply reply) {
    if (auto spawn_reply = GetSpawnReply(std::move(reply))) {
      handler(std::move(*spawn_reply));
    }
  });
}

std::optional<InvocationError> FlatpakPortalProxy::SpawnSignalBlocking(std::uint32_t pid,
                                                                       std::uint32_t signal) {
  MethodCall call(kFlatpakPortalRef, "SpawnSignal");
  MessageWriter writer = call.OpenWriter();

  writer.Write<TypeCode::kUInt32>(pid);
  writer.Write<TypeCode::kUInt32>(signal);

  Reply reply = bus_->CallBlocking(std::move(call));
  return reply.ReadError();
}

void FlatpakPortalProxy::SubscribeToSpawnStarted(SpawnStartedHandler handler) {
  std::string interface = kFlatpakPortalRef.interface().data();
  bus_->SignalConnect(std::move(interface), "SpawnStarted", [handler](Signal signal) {
    MessageReader reader = signal.OpenReader();
    SpawnStartedMessage message;
    if (!reader.Read<TypeCode::kUInt32>(&message.external_pid) ||
        !reader.Read<TypeCode::kUInt32>(&message.internal_pid)) {
      Log() << "Failed to read SpawnStarted message";
    } else {
      handler(message);
    }
  });
}

void FlatpakPortalProxy::SubscribeToSpawnExited(SpawnExitedHandler handler) {
  std::string interface = kFlatpakPortalRef.interface().data();
  bus_->SignalConnect(std::move(interface), "SpawnExited", [handler](Signal signal) {
    MessageReader reader = signal.OpenReader();
    SpawnExitedMessage message;
    if (!reader.Read<TypeCode::kUInt32>(&message.external_pid) ||
        !reader.Read<TypeCode::kUInt32>(&message.exit_status)) {
      Log() << "Failed to read SpawnExited message";
    } else {
      handler(message);
    }
  });
}

MethodCall FlatpakPortalProxy::BuildSpawnMethodCall(SpawnCall spawn) {
  MethodCall call(kFlatpakPortalRef, "Spawn");
  MessageWriter writer = call.OpenWriter();

  ZYPAK_ASSERT(!spawn.cwd.empty());
  writer.WriteFixedArray<TypeCode::kByte>(reinterpret_cast<const std::byte*>(spawn.cwd.data()),
                                          spawn.cwd.size() + 1);  // include null terminator

  {
    ZYPAK_ASSERT(!spawn.argv.empty());
    MessageWriter argv_writer = writer.EnterContainer<TypeCode::kArray>("ay");
    for (const std::string& arg : spawn.argv) {
      argv_writer.WriteFixedArray<TypeCode::kByte>(reinterpret_cast<const std::byte*>(arg.data()),
                                                   arg.size() + 1);  // include null terminator
    }
  }

  {
    MessageWriter fds_writer = writer.EnterContainer<TypeCode::kArray>("{uh}");
    if (spawn.fds != nullptr) {
      for (const FdAssignment& assignment : *spawn.fds) {
        MessageWriter pair_writer = fds_writer.EnterContainer<TypeCode::kDictEntry>();
        pair_writer.Write<TypeCode::kUInt32>(assignment.target());
        pair_writer.Write<TypeCode::kHandle>(assignment.fd().get());
      }
    }
  }

  {
    MessageWriter env_writer = writer.EnterContainer<TypeCode::kArray>("{ss}");
    for (const auto& [var, value] : spawn.env) {
      MessageWriter pair_writer = env_writer.EnterContainer<TypeCode::kDictEntry>();
      pair_writer.Write<TypeCode::kString>(var);
      pair_writer.Write<TypeCode::kString>(value);
    }
  }

  writer.Write<TypeCode::kUInt32>(static_cast<std::uint32_t>(spawn.flags));

  {
    constexpr std::string_view kOptionSandboxFlags = "sandbox-flags";
    constexpr std::string_view kOptionSandboxExposeFdRo = "sandbox-expose-fd-ro";

    MessageWriter options_writer = writer.EnterContainer<TypeCode::kArray>("{sv}");

    if (spawn.options.sandbox_flags != SpawnOptions::kNoSandboxFlags) {
      MessageWriter pair_writer = options_writer.EnterContainer<TypeCode::kDictEntry>();
      pair_writer.Write<TypeCode::kString>(kOptionSandboxFlags);

      MessageWriter value_writer = pair_writer.EnterContainer<TypeCode::kVariant>("u");
      value_writer.Write<TypeCode::kUInt32>(
          static_cast<std::uint32_t>(spawn.options.sandbox_flags));
    }

    if (!spawn.options.sandbox_expose_ro.empty()) {
      MessageWriter pair_writer = options_writer.EnterContainer<TypeCode::kDictEntry>();
      pair_writer.Write<TypeCode::kString>(kOptionSandboxExposeFdRo);

      MessageWriter value_writer = pair_writer.EnterContainer<TypeCode::kVariant>("ah");
      MessageWriter array_writer = value_writer.EnterContainer<TypeCode::kArray>("h");

      for (const unique_fd& fd : spawn.options.sandbox_expose_ro) {
        array_writer.Write<TypeCode::kHandle>(fd.get());
      }
    }
  }

  return call;
}

// static
std::optional<FlatpakPortalProxy::SpawnReply> FlatpakPortalProxy::GetSpawnReply(Reply reply) {
  if (std::optional<InvocationError> error = reply.ReadError()) {
    return std::move(*error);
  }

  MessageReader reader = reply.OpenReader();
  std::uint32_t pid;
  if (!reader.Read<TypeCode::kUInt32>(&pid)) {
    Log() << "Failed to read u32 pid from Spawn reply";
    return {};
  } else {
    return pid;
  }
}

std::optional<std::uint32_t> FlatpakPortalProxy::GetUint32PropertyBlocking(std::string_view name) {
  Bus::PropertyResult<TypeCode::kUInt32> reply =
      bus_->GetPropertyBlocking<TypeCode::kUInt32>(kFlatpakPortalRef, name);

  if (auto* value = std::get_if<std::uint32_t>(&reply)) {
    return *value;
  } else if (auto* error = std::get_if<InvocationError>(&reply)) {
    Log() << "Error retrieving " << name << " property: " << *error;
  } else {
    Log() << "Unknown error retrieving " << name << " property";
  }

  return {};
}

}  // namespace zypak::dbus
