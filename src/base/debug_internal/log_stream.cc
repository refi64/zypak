// Copyright 2020 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug_internal/log_stream.h"

#include <cstring>

#include "base/base.h"
#include "base/debug.h"

namespace zypak::debug_internal {

LogStream::LogStream(std::ostream* os, int print_errno)
    : std::ostream(os->rdbuf()), os_(os), print_errno_(print_errno) {
  *os_ << "[fake-sandbox: " << DebugContext::instance()->name() << "] ";
}

LogStream::~LogStream() {
  if (print_errno_) {
    int errno_save = errno;
    *os_ << ": " << strerror(errno_save) << " (errno " << errno_save << ")";
  }

  *os_ << std::endl;
}

}  // namespace zypak::debug_internal
