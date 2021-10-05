// Copyright 2021 Endless OS Foundation LLC.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <string>
#include <string_view>

#include "base/base.h"
#include "base/debug.h"

namespace zypak {

// A string_view-like type that's guaranteed to be null terminated.
class cstring_view : public std::string_view {
 public:
  constexpr cstring_view() : std::string_view() {}
  constexpr cstring_view(const cstring_view& other) : std::string_view(other) {}
  constexpr cstring_view(const char* str) : std::string_view(str) {}
  cstring_view(const std::string& str) : cstring_view(str.c_str()) {}

  static cstring_view AssertFromNullTerminated(std::string_view view) {
    ZYPAK_ASSERT(*(view.data() + view.size()) == '\0');
    return cstring_view(view.data());
  }

  const char* c_str() const { return data(); }

  std::string ToOwned() const { return std::string(c_str()); }
};

}  // namespace zypak
