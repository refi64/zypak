// Copyright 2021 Endless OS Foundation LLC.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <type_traits>

namespace zypak {

namespace enum_util_detail {

template <typename T>
struct IsFlagSetTest : std::false_type {};

template <typename T>
constexpr bool IsFlagSet = IsFlagSetTest<T>::value;

template <typename T>
class FlagOrBool {
 public:
  FlagOrBool(T value) : value_(value) {}
  FlagOrBool(const FlagOrBool<T>& other) : value_(other.value_) {}

  operator T() { return value_; }
  operator bool() { return static_cast<bool>(value_); }

 private:
  T value_;
};

}  // namespace enum_util_detail

template <typename T, typename = std::enable_if_t<enum_util_detail::IsFlagSet<T>>>
enum_util_detail::FlagOrBool<T> operator&(T left, T right) {
  return static_cast<T>(static_cast<std::underlying_type_t<T>>(left) &
                        static_cast<std::underlying_type_t<T>>(right));
}

template <typename T, typename = std::enable_if_t<enum_util_detail::IsFlagSet<T>>>
T& operator&=(T& left, T right) {
  left = left & right;
  return left;
}

template <typename T, typename = std::enable_if_t<enum_util_detail::IsFlagSet<T>>>
T operator|(T left, T right) {
  return static_cast<T>(static_cast<std::underlying_type_t<T>>(left) |
                        static_cast<std::underlying_type_t<T>>(right));
}

template <typename T, typename = std::enable_if_t<enum_util_detail::IsFlagSet<T>>>
T& operator|=(T& left, T right) {
  left = left | right;
  return left;
}

}  // namespace zypak

#define ZYPAK_DEFINE_ENUM_FLAGS(enum_type)               \
  namespace zypak::enum_util_detail {                    \
  template <>                                            \
  struct IsFlagSetTest<enum_type> : ::std::true_type {}; \
  }
