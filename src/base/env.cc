// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/env.h"

#include <cstdlib>

#include "base/debug.h"

namespace zypak {

// static
std::optional<cstring_view> Env::Get(cstring_view name) {
  if (auto env = getenv(name.c_str())) {
    return {env};
  } else {
    return {};
  }
}

// static
cstring_view Env::Require(cstring_view name) {
  if (auto env = Get(name)) {
    return *env;
  } else {
    Log() << "Failed to get environment variable: " << name;
    ZYPAK_ASSERT(false);
  }
}

// static
void Env::Set(cstring_view name, cstring_view value, bool overwrite /*= true*/) {
  ZYPAK_ASSERT(setenv(name.c_str(), value.c_str(), static_cast<int>(overwrite)) == 0);
}

// static
void Env::Clear(cstring_view name) { ZYPAK_ASSERT(unsetenv(name.c_str()) == 0); }

// static
bool Env::Test(cstring_view name, bool default_value /*= false*/) {
  if (auto env = Get(name)) {
    return !env->empty() && *env != "0" && *env != "false";
  } else {
    return default_value;
  }
}

}  // namespace zypak
