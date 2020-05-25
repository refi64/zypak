// Copyright 2020 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Acquire a bus connection on startup. This is done by overriding main
// via __libc_start_main.

#include <cstdlib>

#include "base/base.h"
#include "base/debug.h"
#include "base/socket.h"
#include "dbus/bus.h"
#include "preload/declare_override.h"
#include "preload/host/spawn_strategy/supervisor.h"

using namespace zypak;
using namespace zypak::preload;

namespace {

using MainType = int (*)(int, char**, char**);

MainType true_main = nullptr;

int ZypakMain(int argc, char** argv, char** envp) {
  DebugContext::instance()->LoadFromEnvironment();
  DebugContext::instance()->set_name("preload-host-spawn-strategy");

  // Unset LD_PRELOAD so exec'd processes don't call back into here again.
  // This would particularly break anything that assumes only one thread is running.
  unsetenv("LD_PRELOAD");

  dbus::Bus* bus = dbus::Bus::Acquire();
  ZYPAK_ASSERT(bus);

  Supervisor* supervisor = Supervisor::Acquire();
  ZYPAK_ASSERT(supervisor);
  ZYPAK_ASSERT(supervisor->InitAndAttachToBusThread(bus));

  int ret = true_main(argc, argv, envp);
  bus->Shutdown();

  return ret;
}

}  // namespace

DECLARE_OVERRIDE(int, __libc_start_main, MainType main, int argc, char** argv, void (*init)(void),
                 void (*finalize)(void), void (*rtld_finalize)(void), void(*stack_end)) {
  auto original = LoadOriginal();

  true_main = main;
  return original(ZypakMain, argc, argv, init, finalize, rtld_finalize, stack_end);
}
