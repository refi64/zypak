// Copyright 2021 Endless OS Foundation LLC.
// Copyright 2022 Ryan Gonzalez
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <sys/stat.h>

#include "base/base.h"
#include "base/cstring_view.h"

namespace zypak {

ATTR_NO_WARN_UNUSED inline constexpr cstring_view kCurrentExe = "/proc/self/exe";

// Tests if both paths are pointing to the exact same on-disk file.
inline bool PathsPointToSameFile(cstring_view a, cstring_view b) {
  struct stat a_st, b_st;
  return stat(a.c_str(), &a_st) != -1 && stat(b.c_str(), &b_st) != -1 &&
         a_st.st_dev == b_st.st_dev && a_st.st_ino == b_st.st_ino;
}

}  // namespace zypak
