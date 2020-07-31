// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstring>
#include <ostream>
#include <sstream>

#include "base/base.h"
#include "base/debug_internal/log_stream.h"

namespace zypak {

namespace debug_internal {

ATTR_NO_WARN_UNUSED constexpr std::string_view kAssertMsgSeparator = ": ";

}

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

 private:
  bool enabled_;
  std::string name_;

  static DebugContext instance_;
};

// Output logging streams. All log to stderr, but Errno also prints the string value of
// POSIX errno.
debug_internal::LogStream Log();
debug_internal::LogStream Errno(int value = 0);
debug_internal::LogStream Debug();

#define ZYPAK_ASSERT_BASE(cond, setup, ...)                                          \
  do {                                                                               \
    if (!(cond)) {                                                                   \
      setup;                                                                         \
      std::stringstream zypak_assert_ss;                                             \
      zypak_assert_ss << ::zypak::debug_internal::kAssertMsgSeparator __VA_ARGS__;   \
      std::string zypak_assert_msg = zypak_assert_ss.str();                          \
      ::zypak::Log() << __FILE__ << ":" << __LINE__ << "(" << __func__ << "): "      \
                     << "Assertion failed: " #cond                                   \
                     << (zypak_assert_msg.size() >                                   \
                                 ::zypak::debug_internal::kAssertMsgSeparator.size() \
                             ? zypak_assert_msg                                      \
                             : "");                                                  \
      abort();                                                                       \
    }                                                                                \
  } while (0)

#define ZYPAK_ASSERT(cond, ...) ZYPAK_ASSERT_BASE(cond, , __VA_ARGS__)

#define ZYPAK_ASSERT_WITH_ERRNO(cond) \
  ZYPAK_ASSERT_BASE(cond, int zypak_errno = errno, << std::strerror(zypak_errno))

#define ZYPAK_ASSERT_SD_ERROR(expr)                                             \
  do {                                                                          \
    int zypak_assert_r;                                                         \
    ZYPAK_ASSERT((zypak_assert_r = (expr)) >= 0, << strerror(-zypak_assert_r)); \
  } while (0)

}  // namespace zypak
