// Copyright 2020 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug_internal/log_stream.h"

#include <unistd.h>

#include <cstring>

#include "base/base.h"
#include "base/debug.h"

namespace zypak::debug_internal {

LogStream::LogStream(std::ostream* os, int errno_save /*= -1*/)
    : std::ostream(os->rdbuf()), os_(os), errno_save_(errno_save) {
  *os_ << "[" << getpid() << ' ' << DebugContext::instance()->name() << "] ";
}

LogStream::~LogStream() {
  if (errno_save_ != 0) {
    *os_ << ": " << std::strerror(errno_save_) << " (errno " << errno_save_ << ")";
  }

  *os_ << std::endl;
}

}  // namespace zypak::debug_internal
