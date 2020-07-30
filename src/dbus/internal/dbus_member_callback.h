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

// MakeDBusMemberCallback takes the a member function as a template argument and returns a callback
// taking the same arguments as the member, except with the addition of a void* user data argument
// at the end. The member function's 'this' value is taken from the void* pointer, and the member
// function is called with the arguments forwarded.
// The second template parameter lets you ignore some arguments passed to the *front* of the
// callback's argument list, e.g.:
// MakeDBusMemberCallback<&Obj::AMethodTakingAnInt, Ignored<char>> will return a function fo type
// void (*)(char c, int i), but it will call AMethodTakingAnInt only with i.
template <auto MemFn, typename Ign = Ignored<>>
auto MakeDBusMemberCallback() {
  return &CallbackHolder<MemFn, Ign>::callback;
}

}  // namespace zypak::dbus::internal
