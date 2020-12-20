// Copyright 2020 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "preload/child/mimic_strategy/fd_storage.h"

#include <fcntl.h>

#include <cstdlib>

#include "base/debug.h"
#include "base/singleton.h"
#include "sandbox/mimic_strategy/zygote.h"

namespace zypak::preload {

FdStorage* FdStorage::instance() {
  static Singleton<FdStorage> instance;
  return instance.get();
}

void FdStorage::Init() {
  sandbox_service_fd_ = sandbox::mimic_strategy::kSandboxServiceFd;

  urandom_fd_ = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
  ZYPAK_ASSERT_WITH_ERRNO(!urandom_fd_.invalid());
}

}  // namespace zypak::preload
