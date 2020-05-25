// Copyright 2020 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Safely handle forks while preserving the bus thread.

#include <unistd.h>

#include <filesystem>

#include "base/base.h"
#include "base/debug.h"
#include "dbus/bus.h"
#include "preload/declare_override.h"
#include "preload/host/spawn_strategy/no_close_host_fd.h"

using namespace zypak;

DECLARE_OVERRIDE(pid_t, fork) {
  auto original = LoadOriginal();

  dbus::Bus* bus = dbus::Bus::Acquire();

  Log() << "Prepare for fork";

  bus->Pause();
  pid_t result = original();

  if (result == 0) {
    Log() << "Shut down bus in child";
    // Fully shut down the bus in the child. That way, if it is unused, no resources are wasted.
    bus->Shutdown();
    // Make sure the supervisor fd won't get closed.
    zypak::preload::block_supervisor_fd_close = true;
    return result;
  }

  Log() << "Resume bus in parent";
  // In the parent, re-init the bus, regardless of the fork result.
  bus->Resume();
  return result;
}
