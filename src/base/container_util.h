// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/base.h"

namespace zypak {

namespace container_util_internal {

template <typename Target, typename Extend>
void ExtendWithOneContainerCopy(Target* target, const Extend& extend) {
  std::copy(extend.begin(), extend.end(), std::back_inserter(*target));
}

template <typename Target, typename Extend>
void ExtendWithOneContainerMove(Target* target, Extend&& extend) {
  std::move(extend.begin(), extend.end(), std::back_inserter(*target));
}

}  // namespace container_util_internal

template <typename Target, typename... Extends>
void ExtendContainerCopy(Target* target, const Extends&... extends) {
  target->reserve(target->size() + (... + extends.size()));
  (container_util_internal::ExtendWithOneContainerCopy(target, extends), ...);
}

template <typename Target, typename... Extends>
void ExtendContainerMove(Target* target, Extends&&... extends) {
  target->reserve(target->size() + (... + extends.size()));
  (container_util_internal::ExtendWithOneContainerMove(target, std::move(extends)), ...);
}

}  // namespace zypak
