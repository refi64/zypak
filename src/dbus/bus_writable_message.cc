// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/bus_writable_message.h"

namespace zypak::dbus {

MethodCall::MethodCall(FloatingRef ref, std::string_view method)
    : WritableMessage(dbus_message_new_method_call(ref.service().data(), ref.object().data(),
                                                   ref.interface().data(), method.data())) {}

}  // namespace zypak::dbus
