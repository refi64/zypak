// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <type_traits>

#include "base/base.h"

namespace zypak {

// A class that wraps an instance that should only be constructed once, with no destructor called.
// Example usage:
// static int* Func() { static Singleton<int> value(2); return value.get(); }
template <typename T>
class Singleton {
 public:
  // Creates a new singleton value, forwarding the given arguments to the value's constructor.
  template <typename... Args>
  Singleton(Args&&... args) {
    new (&storage_) T(std::forward<Args>(args)...);
  }

  // Gets a pointer to the singleton value.
  T* get() { return std::launder(reinterpret_cast<T*>(&storage_)); }

 private:
  std::aligned_storage_t<sizeof(T), alignof(T)> storage_;
};

}  // namespace zypak
