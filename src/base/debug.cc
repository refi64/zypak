// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug.h"

#include <errno.h>
#include <ext/stdio_filebuf.h>  // libstdc++-only
#include <unistd.h>

#include <cstring>
#include <iostream>

#include "base/env.h"

debug_detail::LogStream::LogStream(std::ostream* os, int print_errno)
    : std::ostream(os->rdbuf()), os_(os), print_errno_(print_errno) {
  *os_ << "[fake-sandbox: " << DebugContext::instance()->name() << "] ";
}

debug_detail::LogStream::~LogStream() {
  if (print_errno_) {
    int errno_save = errno;
    *os_ << ": " << strerror(errno_save) << " (errno " << errno_save << ")";
  }

  *os_ << std::endl;
}

namespace {

class NullStream : public std::ostream {
 public:
  NullStream() : std::ostream(&nullbuf_) {}

  static NullStream* instance() { return &instance_; }

 private:
  class NullBuffer : public std::streambuf {
   public:
    int overflow(int c) override { return c; }
  };

  NullBuffer nullbuf_;

  static NullStream instance_;
};

}  // namespace

NullStream NullStream::instance_;

DebugContext::DebugContext() : enabled_(false), name_("<unset>") {}

void DebugContext::LoadFromEnvironment() {
  if (Env::Test(kDebugEnv)) {
    enable();
  }
}

bool DebugContext::enabled() const { return enabled_; }

void DebugContext::enable() { enabled_ = true; }

std::string_view DebugContext::name() const { return name_; }

void DebugContext::set_name(std::string_view name) { name_ = name; }

DebugContext* DebugContext::instance() { return &instance_; }

DebugContext DebugContext::instance_;

debug_detail::LogStream Log() { return debug_detail::LogStream(&std::cerr); }

debug_detail::LogStream Errno() { return debug_detail::LogStream(&std::cerr, true); }

debug_detail::LogStream Debug() {
  return DebugContext::instance()->enabled() ? Log()
                                             : debug_detail::LogStream(NullStream::instance());
}
