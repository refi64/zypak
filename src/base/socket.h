// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <array>
#include <vector>

#include "base/base.h"
#include "base/unique_fd.h"

// Utilities for reading and writing data to a socket.
// 'fds' is always a vector of file descriptors being passed over a socket.
class Socket {
 public:
  static ssize_t Read(int fd, std::vector<std::byte>* buffer,
                      std::vector<unique_fd>* fds = nullptr);
  static ssize_t Read(int fd, std::byte* buffer, size_t size,
                      std::vector<unique_fd>* fds = nullptr);

  template <size_t N>
  static ssize_t Read(int fd, std::array<std::byte, N>* buffer,
                      std::vector<unique_fd>* fds = nullptr) {
    return Read(fd, buffer->data(), N, fds);
  }

  static bool Write(int fd, const std::byte* buffer, size_t length,
                    const std::vector<int>* fds = nullptr);
  static bool Write(int fd, const std::vector<std::byte>& buffer,
                    const std::vector<int>* fds = nullptr);
  static bool Write(int fd, std::string_view buffer, const std::vector<int>* fds = nullptr);
};
