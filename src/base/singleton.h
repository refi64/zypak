// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <type_traits>

#include "base/base.h"

namespace zypak {

template <typename T>
class Singleton {
 public:
  template <typename... Args>
  Singleton(Args&&... args) {
    new (&storage_) T(std::forward<Args>(args)...);
  }

  T* get() { return std::launder(reinterpret_cast<T*>(&storage_)); }

 private:
  std::aligned_storage_t<sizeof(T), alignof(T)> storage_;
};

}  // namespace zypak
