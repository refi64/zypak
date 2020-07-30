// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/base.h"
#include "base/debug.h"
#include "dbus/bus_message.h"

namespace zypak::dbus {

// A context that reads values from a D-Bus message.
class MessageReader {
 public:
  MessageReader(const MessageReader& other) = delete;
  MessageReader(MessageReader&& other) = default;

  // Gets the type of the next value in the current message, or an empty optional if this is at the
  // end.
  std::optional<TypeCode> peek_type() const {
    int type = dbus_message_iter_get_arg_type(const_cast<DBusMessageIter*>(&iter_));
    return type != DBUS_TYPE_INVALID ? static_cast<TypeCode>(type) : std::optional<TypeCode>();
  }

  // Reads a value from the message into the given pointer, returning true on success and false
  // otherwise.
  template <TypeCode Code>
  bool Read(typename BusTypeTraits<Code>::External* dest) {
    static_assert(BusTypeTraits<Code>::kind != TypeCodeKind::kContainer);

    std::optional<TypeCode> type = peek_type();
    if (!type || *type != Code) {
      return false;
    }

    typename BusTypeTraits<Code>::Internal internal;
    dbus_message_iter_get_basic(&iter_, &internal);
    *dest = BusTypeTraits<Code>::ConvertToExternal(internal);

    dbus_message_iter_next(&iter_);
    return true;
  }

  // Enters a container, and returns a MessageReader pointing *inside* the container. If the
  // container cannot be opened, returns an empty optional.
  template <TypeCode Code>
  std::optional<MessageReader> EnterContainer() {
    static_assert(BusTypeTraits<Code>::kind == TypeCodeKind::kContainer);

    std::optional<TypeCode> type = peek_type();
    if (!type || *type != Code) {
      return {};
    }

    return MessageReader(&iter_);
  }

 private:
  MessageReader(DBusMessage* message) { ZYPAK_ASSERT(dbus_message_iter_init(message, &iter_)); }
  MessageReader(DBusMessageIter* parent) { dbus_message_iter_recurse(parent, &iter_); }

  DBusMessageIter iter_;

  friend class ReadableMessage;
};

// An interface for a message that may be read.
class ReadableMessage : public Message {
 public:
  MessageReader OpenReader() const { return MessageReader(message()); }

 protected:
  using Message::Message;
};

// An error that occurred while invoking a D-Bus method.
class InvocationError {
 public:
  InvocationError(std::optional<std::string> name, std::optional<std::string> message)
      : name_(std::move(name)), message_(std::move(message)) {}

  // Returns a string_view so value_or doesn't make extraneous copies.
  std::optional<std::string_view> name() const { return name_; }
  std::optional<std::string_view> message() const { return message_; }

 private:
  std::optional<std::string> name_;
  std::optional<std::string> message_;
};

// A reply to a D-Bus method call.
class Reply : public ReadableMessage {
 public:
  Reply(DBusMessage* message) : ReadableMessage(message) {}

  // Returns true if this is an error reply.
  bool is_error() const { return dbus_message_get_type(message()) == DBUS_MESSAGE_TYPE_ERROR; }

  // If this reply is an error, returns it, otherwise returns an empty optional.
  std::optional<InvocationError> ReadError();
};

// A signal emitted over the bus.
class Signal : public ReadableMessage {
 public:
  Signal(DBusMessage* message) : ReadableMessage(message) {
    ZYPAK_ASSERT(dbus_message_get_type(message) == DBUS_MESSAGE_TYPE_SIGNAL);
  }

  // Returns whether or not the current signal is the same as the given name and was emitted by the
  // given interface.
  bool Test(std::string_view iface, std::string_view signal) const;
};

}  // namespace zypak::dbus

std::ostream& operator<<(std::ostream& os, const zypak::dbus::InvocationError& error);
