// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/base.h"

namespace zypak {

// Tag type representing a string piece.
template <typename P>
struct PieceType {};

// Splits the given string view by any of the characters in 'delims' (or a single char, if that's
// passed), storing each item in the given output iterator. The PieceType<P> argument can be used to
// signify a type P that each string_view will be given to before saving into the output iterator.
template <typename Delims, typename It, typename P = std::string_view>
void SplitInto(std::string_view str, Delims delims, It out, PieceType<P> = {}) {
  std::size_t start = 0;
  std::size_t pos;

  for (;;) {
    pos = str.find_first_of(delims, start);
    if (pos == std::string_view::npos) {
      break;
    }

    *out++ = P(str.substr(start, pos - start));
    start = pos + 1;
  }

  std::string_view leftover = str.substr(start);
  if (!leftover.empty()) {
    *out++ = P(leftover);
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
