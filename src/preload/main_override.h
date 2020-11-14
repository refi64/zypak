// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "preload/declare_override.h"

namespace main_override_detail {

using MainType = int (*)(int, char**, char**);

MainType true_main = nullptr;

int ZypakMain(int argc, char** argv, char** envp);

}  // namespace main_override_detail

#define MAIN_OVERRIDE main_override_detail::ZypakMain

#define INSTALL_MAIN_OVERRIDE()                                                           \
  DECLARE_OVERRIDE(int, __libc_start_main, main_override_detail::MainType main, int argc, \
                   char** argv, void (*init)(void), void (*finalize)(void),               \
                   void (*rtld_finalize)(void), void(*stack_end)) {                       \
    auto original = LoadOriginal();                                                       \
    main_override_detail::true_main = main;                                               \
    return original(MAIN_OVERRIDE, argc, argv, init, finalize, rtld_finalize, stack_end); \
  }
