// Copyright 2020 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>
#include <mutex>
#include <ostream>
#include <sstream>

namespace zypak::debug_internal {

class LogStream : public std::ostream {
 public:
  LogStream(std::ostream* os, int errno_save = -1);
  ~LogStream();

 private:
  LogStream(std::unique_ptr<std::stringstream> ss, std::ostream* os, int errno_save);

  static std::mutex lock_;

  int errno_save_;
  std::unique_ptr<std::stringstream> ss_;
  std::ostream* os_;
  std::lock_guard<std::mutex> lock_guard_;
};

}  // namespace zypak::debug_internal
