// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ostream>

#include "base/base.h"

namespace debug_detail {

class LogStream : public std::ostream {
public:
  LogStream(std::ostream* os, int print_errno=false);
  ~LogStream();

private:
  std::ostream* os_;
  int print_errno_;
};

}  // debug_detail

// Represents a global context holding debugging information.
class DebugContext {
public:
  DebugContext();
  void LoadFromEnvironment();

  bool enabled() const;
  void enable();

  std::string_view name() const;
  void set_name(std::string_view name);

  static DebugContext* instance();

  static constexpr std::string_view kDebugEnv = "ZYPAK_DEBUG";

private:
  bool enabled_;
  std::string name_;

  static DebugContext instance_;
};

// Output logging streams. All log to stderr, but Errno also prints the string value of
// POSIX errno.
debug_detail::LogStream Log();
debug_detail::LogStream Errno();
debug_detail::LogStream Debug();

#define ZYPAK_ASSERT(cond) \
  do {\
    if (!(cond)) { \
      ::Log() << __FILE__ << ":" << __LINE__ << "(" << __func__ <<  "): " \
              << "assertion failed: " #cond; \
      abort(); \
    } \
  } while (0)
