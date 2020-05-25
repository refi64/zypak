// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/bus_error.h"

#include "base/debug.h"

namespace zypak::dbus {

Error::Error() { dbus_error_init(&error_); }

Error::~Error() {
  if (*this) {
    dbus_error_free(&error_);
  }
}

Error& Error::operator=(Error&& other) {
  if (dbus_error_is_set(&other.error_)) {
    dbus_move_error(&other.error_, &error_);
  } else {
    dbus_error_init(&error_);
  }

  return *this;
}

std::string_view Error::name() const {
  ZYPAK_ASSERT(*this);
  return error_.name;
}

std::string_view Error::message() const {
  ZYPAK_ASSERT(*this);
  return error_.message;
}

}  // namespace zypak::dbus

std::ostream& operator<<(std::ostream& os, const zypak::dbus::Error& error) {
  if (error) {
    os << "[" << error.name() << "] " << error.message();
  } else {
    os << "(empty bus error)";
  }

  return os;
}
