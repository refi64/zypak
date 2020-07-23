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

namespace zypak {

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
  if (Env::Test(Env::kZypakSettingEnableDebug)) {
    enable();
  }
}

bool DebugContext::enabled() const { return enabled_; }

void DebugContext::enable() { enabled_ = true; }

std::string_view DebugContext::name() const { return name_; }

void DebugContext::set_name(std::string_view name) { name_ = name; }

DebugContext* DebugContext::instance() { return &instance_; }

DebugContext DebugContext::instance_;

debug_internal::LogStream Log() { return debug_internal::LogStream(&std::cerr); }

debug_internal::LogStream Errno(int value /*= 0*/) {
  return debug_internal::LogStream(&std::cerr, value ? value : errno);
}

debug_internal::LogStream Debug() {
  return DebugContext::instance()->enabled() ? Log()
                                             : debug_internal::LogStream(NullStream::instance());
}

}  // namespace zypak
