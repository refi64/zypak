// Copyright 2020 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Safely handle forks while preserving the bus thread.

#include <unistd.h>

#include <filesystem>
#include <mutex>

#include "base/base.h"
#include "base/debug.h"
#include "dbus/bus.h"
#include "preload/declare_override.h"
#include "preload/host/spawn_strategy/close/no_close_host_fd.h"

using namespace zypak;

namespace {

// If multiple threads call fork at the same time, it can result in a race when pausing and resuming
// the bus instance, resulting in various std::thread options terminating because they were
// performed on a joinable thread instance.
std::mutex bus_shutdown_lock_;

}  // namespace

DECLARE_OVERRIDE(pid_t, fork) {
  auto original = LoadOriginal();

  std::unique_lock<std::mutex> guard(bus_shutdown_lock_);

  dbus::Bus* bus = dbus::Bus::Acquire();
  if (!bus->IsRunning()) {
    Debug() << "Note: bus thread is not running, skipping fork override";
    return original();
  }

  Debug() << "Prepare for fork";
  bus->Pause();

  pid_t result = original();

  if (result == 0) {
    // Make sure the supervisor fd won't get closed.
    zypak::preload::block_supervisor_fd_close = true;
    return result;
  }

  Debug() << "Resume bus in parent";
  // In the parent, re-init the bus, regardless of the fork result.
  bus->Resume();

  return result;
}
