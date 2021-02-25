// Copyright 2020 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/base.h"
#include "dbus/flatpak_portal_proxy.h"
#include "sandbox/spawn_strategy/supervisor_communication.h"

namespace zypak::preload {

ATTR_NO_WARN_UNUSED constexpr int kZypakSupervisorFd = sandbox::kZypakSupervisorFd;
ATTR_NO_WARN_UNUSED constexpr int kZypakSupervisorMaxMessageLength =
    sandbox::kZypakSupervisorMaxMessageLength;
ATTR_NO_WARN_UNUSED constexpr std::string_view kZypakSupervisorSpawnRequest =
    sandbox::kZypakSupervisorSpawnRequest;
ATTR_NO_WARN_UNUSED constexpr std::string_view kZypakSupervisorExitReply =
    sandbox::kZypakSupervisorExitReply;

bool FulfillSpawnRequest(dbus::FlatpakPortalProxy* portal, const unique_fd& fd,
                         dbus::FlatpakPortalProxy::SpawnReplyHandler handler);

}  // namespace zypak::preload
