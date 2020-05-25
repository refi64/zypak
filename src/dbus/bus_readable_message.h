// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/base.h"
#include "base/debug.h"
#include "dbus/bus_message.h"

namespace zypak::dbus {

class MessageReader {
 public:
  MessageReader(const MessageReader& other) = delete;
  MessageReader(MessageReader&& other) = default;

  std::optional<TypeCode> peek_type() const {
    int type = dbus_message_iter_get_arg_type(const_cast<DBusMessageIter*>(&iter_));
    return type != DBUS_TYPE_INVALID ? static_cast<TypeCode>(type) : std::optional<TypeCode>();
  }

  bool done() const {
    return dbus_message_iter_get_arg_type(const_cast<DBusMessageIter*>(&iter_)) !=
           DBUS_TYPE_INVALID;
  }

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

    return true;
  }

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

class ReadableMessage : public Message {
 public:
  MessageReader OpenReader() const { return MessageReader(message()); }

 protected:
  using Message::Message;
};

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

class Reply : public ReadableMessage {
 public:
  Reply(DBusMessage* message) : ReadableMessage(message) {}

  std::optional<InvocationError> ReadError();

  MessageKind kind() const override {
    return dbus_message_get_type(message()) == DBUS_MESSAGE_TYPE_ERROR ? MessageKind::kError
                                                                       : MessageKind::kMethodReturn;
  }
};

class Signal : public ReadableMessage {
 public:
  Signal(DBusMessage* message) : ReadableMessage(message) {
    ZYPAK_ASSERT(dbus_message_get_type(message) == DBUS_MESSAGE_TYPE_SIGNAL);
  }

  bool Test(std::string_view iface, std::string_view signal) const;

  MessageKind kind() const override { return MessageKind::kSignal; }
};

}  // namespace zypak::dbus

std::ostream& operator<<(std::ostream& os, const zypak::dbus::InvocationError& error);
