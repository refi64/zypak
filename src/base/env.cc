// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/env.h"

#include <cstdlib>

#include "base/debug.h"

// static
std::optional<std::string_view> Env::Get(std::string_view name) {
  if (auto env = getenv(name.data())) {
    return {env};
  } else {
    return {};
  }
}

// static
std::string_view Env::Require(std::string_view name) {
  if (auto env = Get(name)) {
    return *env;
  } else {
    Log() << "Failed to get environment variable: " << name;
    ZYPAK_ASSERT(false);
  }
}

// static
void Env::Set(std::string_view name, std::string_view value, bool overwrite /*=true*/) {
  ZYPAK_ASSERT(setenv(name.data(), value.data(), static_cast<int>(overwrite)) == 0);
}

// static
bool Env::Test(std::string_view name) {
  if (auto env = Get(name)) {
    return !env->empty();
  } else {
    return false;
  }
}
