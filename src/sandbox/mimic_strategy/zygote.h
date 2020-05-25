// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstdint>

#include "base/base.h"

namespace zypak::sandbox::mimic_strategy {

ATTR_NO_WARN_UNUSED constexpr int kZygoteHostFd = 3;
ATTR_NO_WARN_UNUSED constexpr std::size_t kZygoteMaxMessageLength = 12288;

bool RunMimicZygote();

}  // namespace zypak::sandbox::mimic_strategy
