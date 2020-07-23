// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <sys/types.h>

#include <set>

#include <nickle.h>

namespace zypak::sandbox {

enum class ZygoteTerminationStatus { kNormal, kAbnormal, kKilled, kCrashed, kRunning };

struct ZygoteTerminationStatusCodec
    : nickle::codecs::Enumerated<ZygoteTerminationStatus, int32_t, ZygoteTerminationStatus::kNormal,
                                 ZygoteTerminationStatus::kRunning> {};

void HandleTerminationStatusRequest(std::set<pid_t>* children, nickle::Reader* reader);
void HandleSandboxStatusRequest();

}  // namespace zypak::sandbox
