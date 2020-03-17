// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fd_map.h"
#include "debug.h"

std::optional<unique_fd> FdAssignment::Assign() {
  ZYPAK_ASSERT(!fd_.invalid());

  if (fd_.get() == target_) {
    return std::move(fd_);
  }

  if (dup2(fd_.get(), target_) == -1) {
    Errno() << "Failed to assign fd " << fd_.get() << " to " << target_;
    return {};
  }

  fd_.reset();
  return {unique_fd(target_)};
}

std::string FdAssignment::Serialize() const {
  return std::to_string(target_) + "=" + std::to_string(fd_.get());
}

/*static*/
std::optional<FdAssignment> FdAssignment::Deserialize(std::string_view data) {
  std::string::size_type eq = data.find('=');
  if (eq == std::string::npos) {
    return {};
  }

  int target;
  int fd;
  if (std::sscanf(data.data(), "%d=%d", &target, &fd) < 2) {
    return {};
  }

  return {FdAssignment(unique_fd(fd), target)};
}
