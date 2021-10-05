// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <dbus/dbus.h>

#include <ostream>

#include "base/base.h"
#include "base/cstring_view.h"

namespace zypak::dbus {

// An error that occurred when working with libdbus.
class Error {
 public:
  Error();
  Error(Error&& other) { *this = std::move(other); }
  Error(const Error& other) = delete;
  ~Error();

  Error& operator=(Error&& other);

  DBusError* get() { return &error_; }

  operator bool() const { return dbus_error_is_set(&error_); }
  cstring_view name() const;
  cstring_view message() const;

 private:
  DBusError error_;
};

std::ostream& operator<<(std::ostream& os, const zypak::dbus::Error& error);

}  // namespace zypak::dbus
