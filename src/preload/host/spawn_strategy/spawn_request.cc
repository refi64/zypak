#include "preload/host/spawn_strategy/spawn_request.h"

#include <nickle.h>

#include "base/env.h"
#include "base/socket.h"
#include "base/str_util.h"
#include "sandbox/launcher.h"

namespace zypak::preload {

bool SanityCheckSpawnCall(const dbus::FlatpakPortalProxy::SpawnCall& call,
                          const std::vector<std::string>& exposed_paths) {
  // XXX: Some duplication with preload's launcher.cc.

  if (call.flags & ~(dbus::FlatpakPortalProxy::kSpawnFlags_EmitSpawnStarted |
                     dbus::FlatpakPortalProxy::kSpawnFlags_ExposePids |
                     dbus::FlatpakPortalProxy::kSpawnFlags_NoNetwork |
                     dbus::FlatpakPortalProxy::kSpawnFlags_Sandbox |
                     dbus::FlatpakPortalProxy::kSpawnFlags_WatchBus)) {
    Log() << "Detected unexpected spawn flags: " << call.flags;
    return false;
  }

  if (call.options.sandbox_flags &
      ~dbus::FlatpakPortalProxy::SpawnOptions::kSandboxFlags_ShareGpu) {
    Log() << "Detected unexpected sandbox flags: " << call.options.sandbox_flags;
    return false;
  }

  if (!Env::Test(Env::kZypakSettingDisableSandbox) &&
      !(call.flags & dbus::FlatpakPortalProxy::kSpawnFlags_Sandbox)) {
    Log() << "Cannot run non-sandboxed process while full sandbox is enabled";
    return false;
  }

  if (!exposed_paths.empty()) {
    if (exposed_paths.size() > 1) {
      Log() << "Too many exposed paths: " << exposed_paths.size();
      return false;
    }

    if (auto path = Env::Get(Env::kZypakSettingExposeWidevinePath); path && !path->empty()) {
      if (exposed_paths.front() != *path) {
        Log() << "Unexpected path exposure: " << exposed_paths.front();
        return false;
      }
    } else {
      Log() << "Should not have any exposed paths";
      return false;
    }
  }

  return true;
}

bool FulfillSpawnRequest(dbus::FlatpakPortalProxy* portal, const unique_fd& fd,
                         dbus::FlatpakPortalProxy::SpawnReplyHandler handler) {
  constexpr std::string_view kSpawnDirectory = "/";

  std::array<std::byte, kZypakSupervisorMaxMessageLength> target;
  std::vector<unique_fd> fds;

  Socket::ReadOptions options;
  options.fds = &fds;
  ssize_t len = Socket::Read(fd.get(), &target, options);
  if (len <= 0) {
    if (len == 0) {
      Log() << "No data could be read from supervisor client";
    } else {
      Errno() << "Failed to read message from supervisor client";
    }
  }

  nickle::buffers::ReadOnlyContainerBuffer buffer(target);
  nickle::Reader reader(&buffer);

  dbus::FlatpakPortalProxy::SpawnCall spawn;
  spawn.cwd = kSpawnDirectory;

  std::uint32_t argc;
  if (!reader.Read<nickle::codecs::UInt32>(&argc)) {
    Log() << "Failed to read command size";
    return false;
  }

  for (std::uint32_t i = 0; i < argc; i++) {
    std::string item;
    if (!reader.Read<nickle::codecs::String>(&item)) {
      Log() << "Failed to read comment argument #" << i;
      return false;
    }

    spawn.argv.push_back(std::move(item));
  }

  FdMap fd_map;
  spawn.fds = &fd_map;
  for (std::uint32_t i = 0; i < fds.size(); i++) {
    std::uint32_t target_fd;
    if (!reader.Read<nickle::codecs::UInt32>(&target_fd)) {
      Log() << "Failed to read target fd #" << i;
      return false;
    }

    fd_map.push_back(FdAssignment(std::move(fds[i]), target_fd));
  }

  std::uint32_t env_size;
  if (!reader.Read<nickle::codecs::UInt32>(&env_size)) {
    Log() << "Failed to read env size";
    return false;
  }

  for (std::uint32_t i = 0; i < env_size; i++) {
    std::string var, value;
    if (!reader.Read<nickle::codecs::String>(&var) ||
        !reader.Read<nickle::codecs::String>(&value)) {
      Log() << "Failed to read env pair #" << i;
      return false;
    }

    spawn.env[var] = value;
  }

  std::uint32_t exposed_paths_size;
  if (!reader.Read<nickle::codecs::UInt32>(&exposed_paths_size)) {
    Log() << "Failed to read exposed paths size";
    return false;
  }

  std::vector<std::string> exposed_paths;
  for (std::uint32_t i = 0; i < exposed_paths_size; i++) {
    std::string path;
    if (!reader.Read<nickle::codecs::String>(&path)) {
      Log() << "Failed to read exposed path # " << i;
      return false;
    }

    if (!spawn.options.ExposePathRo(path)) {
      Log() << "Failed to open path for exposing into sandbox: " << path;
      return false;
    }

    exposed_paths.push_back(std::move(path));
  }

  std::uint32_t spawn_flags;
  std::uint32_t sandbox_flags;
  if (!reader.Read<nickle::codecs::UInt32>(&spawn_flags) ||
      !reader.Read<nickle::codecs::UInt32>(&sandbox_flags)) {
    Log() << "Failed to read spawn and sandbox flags";
    return false;
  }

  spawn.flags = static_cast<dbus::FlatpakPortalProxy::SpawnFlags>(spawn_flags);
  spawn.options.sandbox_flags =
      static_cast<dbus::FlatpakPortalProxy::SpawnOptions::SandboxFlags>(sandbox_flags);

  ZYPAK_ASSERT(SanityCheckSpawnCall(spawn, exposed_paths), << "Failed to sanity check spawn call");

  Log() << "Running: " << Join(spawn.argv.begin(), spawn.argv.end());
  portal->SpawnAsync(std::move(spawn), std::move(handler));

  return true;
}

}  // namespace zypak::preload
