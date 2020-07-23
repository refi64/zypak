// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "base/unique_fd.h"

// An FdAssignment represents an unspecified file descriptor and its desired target file
// descriptor. It can then be "assigned" to the target, and the original file descriptor
// will be closed.
class FdAssignment {
 public:
  FdAssignment(unique_fd fd, int target) : fd_(std::move(fd)), target_(target) {}

  const unique_fd& fd() const { return fd_; }
  int target() const { return target_; }
  std::optional<unique_fd> Assign();

  std::string Serialize() const;
  static std::optional<FdAssignment> Deserialize(std::string_view data);

 private:
  unique_fd fd_;
  int target_;
};

// A set of FdAssignment instances.
using FdMap = std::vector<FdAssignment>;
