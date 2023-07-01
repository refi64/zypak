// Copyright 2020 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Acquire a bus connection on startup. This is done by overriding main
// via __libc_start_main.

#include <cstdlib>

#include "base/base.h"
#include "base/debug.h"
#include "base/env.h"
#include "base/socket.h"
#include "dbus/bus.h"
#include "preload/declare_override.h"
#include "preload/host/spawn_strategy/supervisor.h"
#include "preload/main_override.h"

using namespace zypak;
using namespace zypak::preload;

int MAIN_OVERRIDE(int argc, char** argv, char** envp) {
  Env::Clear("LD_PRELOAD");

  DebugContext::instance()->LoadFromEnvironment();
  DebugContext::instance()->set_name("preload-host-spawn-strategy");

  dbus::Bus* bus = dbus::Bus::Acquire();
  ZYPAK_ASSERT(bus);

  Supervisor* supervisor = Supervisor::Acquire();
  ZYPAK_ASSERT(supervisor);
  ZYPAK_ASSERT(supervisor->InitAndAttachToBusThread(bus));

  int ret = true_main(argc, argv, envp);
  bus->Shutdown();

  return ret;
}

INSTALL_MAIN_OVERRIDE()
