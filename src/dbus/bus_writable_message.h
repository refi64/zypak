// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <optional>

#include "base/base.h"
#include "base/cstring_view.h"
#include "base/debug.h"
#include "dbus/bus_message.h"

namespace zypak::dbus {

// A context that writes values to a D-Bus message.
class MessageWriter {
 public:
  MessageWriter(const MessageWriter& other) = delete;
  MessageWriter(MessageWriter&& other) = default;
  ~MessageWriter() {
    if (parent_ != nullptr) {
      dbus_message_iter_close_container(parent_, &iter_);
    }
  }

  // Writes the given value to the message.
  template <TypeCode Code>
  void Write(typename BusTypeTraits<Code>::ExternalView value) {
    static_assert(BusTypeTraits<Code>::kind != TypeCodeKind::kContainer);

    auto internal = BusTypeTraits<Code>::ConvertToInternal(value);
    ZYPAK_ASSERT(dbus_message_iter_append_basic(&iter_, static_cast<int>(Code), &internal));
  }

  // Enters a container of the given type. The container is opened on this call and will be closed
  // once the returned MessageWriter is destroyed.
  template <TypeCode Code>
  MessageWriter EnterContainer() {
    static_assert(BusTypeTraits<Code>::kind == TypeCodeKind::kContainer &&
                  Code != TypeCode::kArray && Code != TypeCode::kVariant);
    return MessageWriter(&iter_, Code, {});
  }

  // Identical to the above, but specialized for arrays. Takes a string representing the type of the
  // array's element.
  template <TypeCode Code>
  MessageWriter EnterContainer(cstring_view element) {
    static_assert(Code == TypeCode::kArray || Code == TypeCode::kVariant);
    return MessageWriter(&iter_, Code, element);
  }

  // Writes an array of items of the given type to the message.
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

  MessageWriter(DBusMessageIter* parent, TypeCode code, std::optional<cstring_view> signature)
      : parent_(parent) {
    ZYPAK_ASSERT(dbus_message_iter_open_container(parent, static_cast<int>(code),
                                                  signature ? signature->data() : nullptr, &iter_));
  }

  DBusMessageIter iter_;
  DBusMessageIter* parent_;

  friend class WritableMessage;
};

// An interface for a message that may be written to.
class WritableMessage : public Message {
 public:
  MessageWriter OpenWriter() { return MessageWriter(message()); }

 protected:
  using Message::Message;
};

// A D-Bus method call message.
class MethodCall : public WritableMessage {
 public:
  MethodCall(FloatingRef ref, cstring_view method);
};

}  // namespace zypak::dbus
