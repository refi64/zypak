// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/base.h"

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
