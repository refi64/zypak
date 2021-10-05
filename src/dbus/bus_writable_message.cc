// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/bus_writable_message.h"

namespace zypak::dbus {

MethodCall::MethodCall(FloatingRef ref, cstring_view method)
    : WritableMessage(dbus_message_new_method_call(ref.service().c_str(), ref.object().c_str(),
                                                   ref.interface().c_str(), method.c_str())) {}

}  // namespace zypak::dbus
