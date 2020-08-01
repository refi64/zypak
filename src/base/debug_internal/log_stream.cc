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

// static
std::mutex LogStream::lock_;

LogStream::LogStream(std::ostream* os, int errno_save /*= -1*/)
    : LogStream(std::make_unique<std::stringstream>(), os, errno_save) {}

LogStream::LogStream(std::unique_ptr<std::stringstream> ss, std::ostream* os, int errno_save)
    : std::ostream(ss->rdbuf()), errno_save_(errno_save), ss_(std::move(ss)), os_(os),
      lock_guard_(lock_) {
  *ss_ << "[" << getpid() << ' ' << DebugContext::instance()->name() << "] ";
}

LogStream::~LogStream() {
  if (errno_save_ != -1) {
    *ss_ << ": " << std::strerror(errno_save_) << " (errno " << errno_save_ << ")";
  }

  *ss_ << std::endl;
  *os_ << ss_->str();
}

}  // namespace zypak::debug_internal
