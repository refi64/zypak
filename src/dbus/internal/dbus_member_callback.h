// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <utility>

namespace zypak::dbus::internal {

template <typename... Args>
struct Ignored {};

template <auto, typename Ign>
struct CallbackHolder;

template <typename Class, typename Ret, typename... IgnoredArgs, typename... PassedArgs,
          Ret (Class::*MemFn)(PassedArgs...)>
struct CallbackHolder<MemFn, Ignored<IgnoredArgs...>> {
  static Ret callback(IgnoredArgs... ignored, PassedArgs... passed, void* data) {
    return (reinterpret_cast<Class*>(data)->*MemFn)(std::forward<PassedArgs>(passed)...);
  }
};

template <auto MemFn, typename Ign = Ignored<>>
auto MakeDBusMemberCallback() {
  return &CallbackHolder<MemFn, Ign>::callback;
}

}  // namespace zypak::dbus::internal
