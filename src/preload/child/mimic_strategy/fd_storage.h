// Copyright 2020 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/unique_fd.h"

// See open_urandom.cc for information on why this is necessary.

namespace zypak::preload {

class FdStorage {
 public:
  static FdStorage* instance();

  void Init();

  const unique_fd& sandbox_service_fd() const { return sandbox_service_fd_; }
  const unique_fd& urandom_fd() const { return urandom_fd_; }

 private:
  unique_fd sandbox_service_fd_;
  unique_fd urandom_fd_;
};

}  // namespace zypak::preload
