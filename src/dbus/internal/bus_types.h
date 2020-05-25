// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <type_traits>

#include <dbus/dbus.h>

#include "base/base.h"
#include "base/unique_fd.h"

namespace zypak::dbus::internal {

enum class TypeCode {
  kByte = 'y',
  kBool = 'b',
  kInt16 = 'n',
  kUInt16 = 'q',
  kInt32 = 'i',
  kUInt32 = 'u',
  kInt64 = 'x',
  kUInt64 = 't',
  kDouble = 'd',
  kString = 's',
  kObject = 'o',
  kSignature = 'g',
  kHandle = 'h',

  kArray = 'a',
  kVariant = 'v',
  kStruct = 'r',
  kDictEntry = 'e',
};

enum class TypeCodeKind { kFixed, kString, kContainer };

template <TypeCodeKind KindValue>
struct BusTypeTraitsBase {
  static constexpr TypeCodeKind kind = KindValue;
};

template <TypeCodeKind KindValue, typename InternalType, typename ExternalType,
          typename ExternalViewType>
struct BusTypeTraitsConvertibleBase : BusTypeTraitsBase<KindValue> {
  using Internal = InternalType;
  using External = ExternalType;
  using ExternalView = ExternalViewType;
};

template <typename InternalType, typename ExternalType>
struct BusTypeTraitsFixedBase
    : BusTypeTraitsConvertibleBase<TypeCodeKind::kFixed, InternalType, ExternalType, ExternalType> {
};

struct BusTypeTraitsContainerBase : BusTypeTraitsBase<TypeCodeKind::kContainer> {};

template <typename Type>
struct BusTypeTraitsConvertibleFixed : BusTypeTraitsFixedBase<Type, Type> {
  static Type ConvertToExternal(Type value) { return value; }
  static Type ConvertToInternal(Type value) { return value; }
};

struct BusTypeTraitsConvertibleString
    : BusTypeTraitsConvertibleBase<TypeCodeKind::kString, const char*, std::string,
                                   std::string_view> {
  static std::string ConvertToExternal(const char* value) { return value; }
  static const char* ConvertToInternal(std::string_view value) { return value.data(); }
};

struct BusTypeTraitsHandleConvert : BusTypeTraitsFixedBase<int, unique_fd> {
  static unique_fd ConvertToExternal(int fd) { return unique_fd(fd); }
  static int ConvertToInternal(const unique_fd& fd) { return fd.get(); }
};

template <TypeCode Code>
struct BusTypeTraits;

#define BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_BASE(code, base) \
  template <>                                                \
  struct BusTypeTraits<TypeCode::code> : base {};

#define BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_FIXED(code, type) \
  BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_BASE(code, BusTypeTraitsConvertibleFixed<type>)

#define BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_STRING(code) \
  BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_BASE(code, BusTypeTraitsConvertibleString)

#define BUS_DECLARE_TYPE_TRAITS_CONTAINER(code) \
  template <>                                   \
  struct BusTypeTraits<TypeCode::code> : BusTypeTraitsContainerBase {};

BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_FIXED(kByte, std::byte)
// TODO: not sure what type this translates to, we don't really need it here
BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_FIXED(kBool, std::uint32_t)
BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_FIXED(kInt16, std::int16_t)
BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_FIXED(kUInt16, std::uint16_t)
BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_FIXED(kInt32, std::int32_t)
BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_FIXED(kUInt32, std::uint32_t)
BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_FIXED(kInt64, std::int64_t)
BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_FIXED(kUInt64, std::uint64_t)
BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_FIXED(kDouble, double)
BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_FIXED(kHandle, int)

BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_STRING(kString)
BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_STRING(kObject)
BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_STRING(kSignature)

BUS_DECLARE_TYPE_TRAITS_CONTAINER(kArray)
BUS_DECLARE_TYPE_TRAITS_CONTAINER(kVariant)
BUS_DECLARE_TYPE_TRAITS_CONTAINER(kStruct)
BUS_DECLARE_TYPE_TRAITS_CONTAINER(kDictEntry)

#undef BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_BASE
#undef BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_FIXED
#undef BUS_DECLARE_TYPE_TRAITS_CONVERTIBLE_STRING
#undef BUS_DECLARE_TYPE_TRAITS_CONTAINER

}  // namespace zypak::dbus::internal
