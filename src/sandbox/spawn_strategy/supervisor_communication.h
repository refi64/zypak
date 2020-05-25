// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/base.h"

namespace zypak::sandbox {

ATTR_NO_WARN_UNUSED constexpr int kZypakSupervisorFd = 235;
ATTR_NO_WARN_UNUSED constexpr int kZypakSupervisorMaxMessageLength = 16 * 1024;
ATTR_NO_WARN_UNUSED constexpr std::string_view kZypakSupervisorSpawnRequest = "SPAWN";
ATTR_NO_WARN_UNUSED constexpr std::string_view kZypakSupervisorExitReply = "EXIT";

}  // namespace zypak::sandbox
