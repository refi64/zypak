// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstdint>
#include <set>

#include "base/base.h"
#include "base/epoll.h"

namespace zypak::sandbox::mimic_strategy {

ATTR_NO_WARN_UNUSED constexpr int kZygoteHostFd = 3;
ATTR_NO_WARN_UNUSED constexpr std::size_t kZygoteMaxMessageLength = 12288;

class MimicZygoteRunner {
 public:
  static std::optional<MimicZygoteRunner> Create();

  bool Run();

 private:
  MimicZygoteRunner(Epoll epoll) : epoll_(std::move(epoll)) {}

  void HandleMessage(Epoll::SourceRef source, Epoll::Events events);

  Epoll epoll_;
  std::set<pid_t> children_;
};

}  // namespace zypak::sandbox::mimic_strategy
