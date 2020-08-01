// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

// Defines a struct named 'alias' with a single 'member' of type 'underlying'. Equality operators
// are defined on the top and simply foward to the underlying type. In essence, this is a rough
// strong typedef.
#define ZYPAK_STRONG_TYPEDEF(alias, underlying, member)                                   \
  struct alias {                                                                          \
    underlying member;                                                                    \
    alias(underlying value) : member(std::move(value)) {}                                 \
  };                                                                                      \
  inline bool operator==(const alias& a, const alias& b) { return a.member == b.member; } \
  inline bool operator!=(const alias& a, const alias& b) { return a.member != b.member; }

// Equivalent to the above, but also defines a hash function.
#define ZYPAK_STRONG_TYPEDEF_DEFINE_HASH(alias, underlying, member) \
  namespace std {                                                   \
  template <>                                                       \
  struct hash<alias> {                                              \
    std::size_t operator()(const alias& x) const noexcept {         \
      return std::hash<underlying>()(x.member);                     \
    }                                                               \
  };                                                                \
  }
