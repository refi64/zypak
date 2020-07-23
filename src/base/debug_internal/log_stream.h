// Copyright 2020 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ostream>
#include <sstream>

namespace zypak::debug_internal {

class LogStream : public std::ostream {
 public:
  LogStream(std::ostream* os, int errno_save = 0);
  ~LogStream();

 private:
  std::ostream* os_;
  int errno_save_;
};

}  // namespace zypak::debug_internal
