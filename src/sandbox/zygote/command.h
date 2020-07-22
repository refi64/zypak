// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <nickle.h>

enum class ZygoteCommand { kFork, kReap, kTerminationStatus, kSandboxStatus, kForkRealPID };

struct ZygoteCommandCodec : nickle::codecs::Enumerated<ZygoteCommand, int32_t, ZygoteCommand::kFork,
                                                       ZygoteCommand::kForkRealPID> {};
