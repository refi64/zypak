// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/base.h"
#include "base/debug.h"
#include "dbus/bus_message.h"

namespace zypak::dbus {

class MessageWriter {
 public:
  MessageWriter(const MessageWriter& other) = delete;
  MessageWriter(MessageWriter&& other) = default;
  ~MessageWriter() {
    if (parent_ != nullptr) {
      dbus_message_iter_close_container(parent_, &iter_);
    }
  }

  template <TypeCode Code>
  void Write(typename BusTypeTraits<Code>::ExternalView value) {
    static_assert(BusTypeTraits<Code>::kind != TypeCodeKind::kContainer);

    auto internal = BusTypeTraits<Code>::ConvertToInternal(value);
    ZYPAK_ASSERT(dbus_message_iter_append_basic(&iter_, static_cast<int>(Code), &internal));
  }

  template <TypeCode Code>
  MessageWriter EnterContainer() {
    static_assert(BusTypeTraits<Code>::kind == TypeCodeKind::kContainer &&
                  Code != TypeCode::kArray && Code != TypeCode::kVariant);
    return MessageWriter(&iter_, Code, {});
  }

  template <TypeCode Code>
  MessageWriter EnterContainer(std::string_view string) {
    static_assert(Code == TypeCode::kArray || Code == TypeCode::kVariant);
    return MessageWriter(&iter_, Code, string);
  }

  template <TypeCode Code>
  void WriteFixedArray(const typename BusTypeTraits<Code>::External* view, std::size_t count) {
    static_assert(BusTypeTraits<Code>::kind != TypeCodeKind::kContainer &&
                  Code != TypeCode::kHandle);

    constexpr char string[] = {static_cast<char>(Code), '\0'};
    MessageWriter array_writer = EnterContainer<TypeCode::kArray>(string);
    ZYPAK_ASSERT(dbus_message_iter_append_fixed_array(&array_writer.iter_, static_cast<int>(Code),
                                                      static_cast<const void*>(&view), count));
  }

 private:
  MessageWriter(DBusMessage* message) : parent_(nullptr) {
    dbus_message_iter_init_append(message, &iter_);
  }

  MessageWriter(DBusMessageIter* parent, TypeCode code, std::optional<std::string_view> signature)
      : parent_(parent) {
    ZYPAK_ASSERT(dbus_message_iter_open_container(parent, static_cast<int>(code),
                                                  signature ? signature->data() : nullptr, &iter_));
  }

  DBusMessageIter iter_;
  DBusMessageIter* parent_;

  friend class WritableMessage;
};

class WritableMessage : public Message {
 public:
  MessageWriter OpenWriter() { return MessageWriter(message()); }

 protected:
  using Message::Message;
};

class MethodCall : public WritableMessage {
 public:
  MethodCall(FloatingRef ref, std::string_view method);

  MessageKind kind() const override { return MessageKind::kMethodCall; }
};

}  // namespace zypak::dbus
