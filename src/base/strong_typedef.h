// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#define ZYPAK_STRONG_TYPEDEF(alias, underlying, member)                                   \
  struct alias {                                                                          \
    underlying member;                                                                    \
    alias(underlying value) : member(std::move(value)) {}                                 \
  };                                                                                      \
  inline bool operator==(const alias& a, const alias& b) { return a.member == b.member; } \
  inline bool operator!=(const alias& a, const alias& b) { return a.member != b.member; }

#define ZYPAK_STRONG_TYPEDEF_DEFINE_HASH(alias, underlying, member) \
  namespace std {                                                   \
  template <>                                                       \
  struct hash<alias> {                                              \
    std::size_t operator()(const alias& x) const noexcept {         \
      return std::hash<underlying>()(x.member);                     \
    }                                                               \
  };                                                                \
  }
