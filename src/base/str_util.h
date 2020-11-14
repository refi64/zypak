// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/base.h"

namespace zypak {

// Determines if the string starts with the given prefix.
inline bool StartsWith(std::string_view str, std::string_view prefix) {
  return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
}

// Splits the given string view by the character c, storing each item in the given output iterator.
template <typename It>
void SplitInto(std::string_view str, char c, It out) {
  std::size_t start = 0;
  std::size_t pos;

  for (;;) {
    pos = str.find_first_of(c, start);
    if (pos == std::string_view::npos) {
      break;
    }

    *out++ = str.substr(start, pos - start);
    start = pos + 1;
  }

  std::string_view leftover = str.substr(start);
  if (!leftover.empty()) {
    *out++ = leftover;
  }
}

// Join the items of the iterator by the given delimeter.
template <typename It>
std::string Join(It first, It last, std::string_view sep = " ") {
  if (first == last) {
    return "";
  }

  std::string result(*first);

  for (auto it = first + 1; it != last; it++) {
    result += sep;
    result += *it;
  }

  return result;
}

}  // namespace zypak
