// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <unistd.h>

// An owned file descriptor, closed on object destruction.
class unique_fd {
public:
  static constexpr int kInvalidFd = -1;

  unique_fd() {}
  unique_fd(int fd): fd_(fd) {}

  unique_fd(const unique_fd& other)=delete;
  unique_fd(unique_fd&& other): unique_fd() {
    std::swap(fd_, other.fd_);
  }

  ~unique_fd() {
    reset();
  }

  bool invalid() const { return fd_ == kInvalidFd; }
  int get() const { return fd_; }

  __attribute__((warn_unused_result)) int release() {
    int tmp = fd_;
    fd_ = kInvalidFd;
    return tmp;
  }

  void reset() {
    if (!invalid()) {
      close(release());
    }
  }

private:
  int fd_ = kInvalidFd;
};
