// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;

#define ATTR_NO_WARN_UNUSED __attribute__((unused))
#define ATTR_WARN_UNUSED_RESULT __attribute__((warn_unused_result))

// Inspired by the macro in base/posix/eintr_wrapper.h
#define HANDLE_EINTR(expr)                                                                   \
  ({                                                                                         \
    int eintr_wrapper_counter = 0;                                                           \
    decltype(expr) eintr_wrapper_result;                                                     \
    do {                                                                                     \
      eintr_wrapper_result = (expr);                                                         \
    } while (eintr_wrapper_result == -1 && errno == EINTR && eintr_wrapper_counter++ < 100); \
    eintr_wrapper_result;                                                                    \
  })
