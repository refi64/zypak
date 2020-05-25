// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <dlfcn.h>

#include <string_view>

// Some helpers to assist with loading the original functions.
namespace declare_override_detail {
template <typename T>
T LoadOriginal(T* func, std::string_view name) {
  if (!*func) {
    *func = reinterpret_cast<T>(dlsym(RTLD_NEXT, name.data()));
  }

  return *func;
}
}  // namespace declare_override_detail

#define DECLARE_OVERRIDE_BASE(except, ret, func, ...)                                         \
  namespace func##_override_detail {                                                          \
    namespace {                                                                               \
    using type = ret (*)(__VA_ARGS__);                                                        \
    thread_local type original = nullptr;                                                     \
    ATTR_NO_WARN_UNUSED func##_override_detail::type LoadOriginal() {                         \
      return declare_override_detail::LoadOriginal(&func##_override_detail::original, #func); \
    }                                                                                         \
    }                                                                                         \
    extern "C" ret func(__VA_ARGS__) except;                                                  \
  }                                                                                           \
  extern "C" ret func##_override_detail::func(__VA_ARGS__) except

#define DECLARE_OVERRIDE(...) DECLARE_OVERRIDE_BASE(noexcept, __VA_ARGS__)
#define DECLARE_OVERRIDE_THROW(...) DECLARE_OVERRIDE_BASE(, __VA_ARGS__)
