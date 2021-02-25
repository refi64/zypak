// Copyright 2020 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <unistd.h>

#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "base/base.h"
#include "base/guarded_value.h"
#include "base/strong_typedef.h"
#include "dbus/bus.h"
#include "dbus/flatpak_portal_proxy.h"
#include "sandbox/spawn_strategy/supervisor_communication.h"

namespace zypak::preload::supervisor_internal {

ZYPAK_STRONG_TYPEDEF(ExternalPid, pid_t, pid)
ZYPAK_STRONG_TYPEDEF(InternalPid, pid_t, pid)
ZYPAK_STRONG_TYPEDEF(StubPid, pid_t, pid)

}  // namespace zypak::preload::supervisor_internal

ZYPAK_STRONG_TYPEDEF_DEFINE_HASH(zypak::preload::supervisor_internal::ExternalPid, pid_t, pid)
ZYPAK_STRONG_TYPEDEF_DEFINE_HASH(zypak::preload::supervisor_internal::InternalPid, pid_t, pid)
ZYPAK_STRONG_TYPEDEF_DEFINE_HASH(zypak::preload::supervisor_internal::StubPid, pid_t, pid)

namespace zypak::preload {

class Supervisor {
 public:
  static Supervisor* Acquire();

  bool InitAndAttachToBusThread(dbus::Bus* bus);

  enum Result { kOk, kNotFound, kTryLater, kFailed };

  Result GetExitStatus(pid_t stub_pid, int* status);
  Result WaitForExitStatus(pid_t stub_pid, int* status);
  Result SendSignal(pid_t stub_pid, int signal);

  Result FindInternalPidBlocking(pid_t stub_pid, pid_t* internal_pid);

 private:
  struct StubPidData {
    supervisor_internal::ExternalPid external = -1;
    supervisor_internal::InternalPid internal = -1;
    std::uint32_t exit_status = -1;
    unique_fd notify_exit;
  };

  StubPidData* FindStubPidData(
      supervisor_internal::ExternalPid external,
      const std::unordered_map<supervisor_internal::StubPid, StubPidData>& stub_pids_data);
  StubPidData* FindStubPidData(
      supervisor_internal::StubPid stub,
      const std::unordered_map<supervisor_internal::StubPid, StubPidData>& stub_pids_data);

  void ReapProcess(supervisor_internal::StubPid stub, StubPidData* data, int* status);

  void HandleSpawnStarted(dbus::FlatpakPortalProxy::SpawnStartedMessage message);
  void HandleSpawnExited(dbus::FlatpakPortalProxy::SpawnExitedMessage message);

  void HandleSpawnRequest(EvLoop::SourceRef source);
  void HandleSpawnReply(pid_t stub_pid, dbus::FlatpakPortalProxy::SpawnReply reply);

  unique_fd request_fd_;

  dbus::FlatpakPortalProxy portal_;

  // NOTE: This doesn't need to be guarded, because it's only ever accessed by the bus thread.
  std::unordered_map<supervisor_internal::ExternalPid, supervisor_internal::StubPid>
      external_to_stub_pids_;
  NotifyingGuardedValue<std::unordered_map<supervisor_internal::StubPid, StubPidData>>
      stub_pids_data_;
};

}  // namespace zypak::preload
