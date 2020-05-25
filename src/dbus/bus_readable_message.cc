// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/bus_readable_message.h"

namespace zypak::dbus {

std::optional<InvocationError> Reply::ReadError() {
  if (kind() != MessageKind::kError) {
    return {};
  }

  std::optional<std::string> name_opt, message_opt;

  if (const char* name = dbus_message_get_error_name(message())) {
    name_opt.emplace(name);
  }

  MessageReader reader = OpenReader();
  std::string message;
  if (reader.Read<TypeCode::kString>(&message)) {
    message_opt.emplace(std::move(message));
  }

  return InvocationError(name_opt, message_opt);
}

bool Signal::Test(std::string_view iface, std::string_view signal) const {
  return dbus_message_is_signal(message(), iface.data(), signal.data());
}

}  // namespace zypak::dbus

std::ostream& operator<<(std::ostream& os, const zypak::dbus::InvocationError& error) {
  os << error.name().value_or("<unknown>") << ": " << error.message().value_or("<unknown>");
  return os;
}
