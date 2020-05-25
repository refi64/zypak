// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/bus.h"

#include <functional>
#include <future>

#include "base/base.h"
#include "base/debug.h"
#include "base/singleton.h"
#include "dbus/bus_error.h"
#include "dbus/bus_message.h"
#include "dbus/bus_readable_message.h"
#include "dbus/bus_writable_message.h"

namespace zypak::dbus {

// static
Bus* Bus::Acquire() {
  static Singleton<Bus> instance;
  Bus* bus = instance.get();

  if (!bus->IsInitialized() && !bus->Initialize()) {
    return nullptr;
  }

  return bus;
}

void Bus::Shutdown() {
  if (!IsInitialized()) {
    Log() << "Attempted to shutdown non-initialized bus thread";
    return;
  }

  bus_thread_->Shutdown();
  bus_thread_.reset();

  signal_handlers_.clear();
  signal_handlers_.shrink_to_fit();
}

void Bus::Pause() { bus_thread_->Shutdown(); }
void Bus::Resume() { bus_thread_->Start(); }

void Bus::CallAsync(MethodCall call, CallHandler callback) {
  bus_thread_->SendCall(std::move(call), std::move(callback));
}

Reply Bus::CallBlocking(MethodCall call) {
  std::promise<Reply> promise;
  CallAsync(std::move(call), [&](Reply reply) {
    Debug() << "CallAsync returned";
    promise.set_value(reply);
  });

  std::future<Reply> future = promise.get_future();
  future.wait();
  return future.get();
}

void Bus::SignalConnect(std::string interface, std::string signal, SignalHandler handler) {
  bool watching_interface = false;
  for (const SignalHandlerSpec& handler : signal_handlers_) {
    if (handler.interface == interface) {
      watching_interface = true;
      break;
    }
  }

  if (!watching_interface) {
    std::string rule = "type='signal',interface='"s + interface + "'";
    bus_thread_->AddMatch(rule, [rule](Error error) {
      if (error) {
        Log() << "Warning: match rule " << rule << " failed: " << error.message();
      }
    });
  }

  signal_handlers_.push_back(
      SignalHandlerSpec{std::move(interface), std::move(signal), std::move(handler)});
}

bool Bus::IsInitialized() const { return !!bus_thread_; }

bool Bus::Initialize() {
  Error error;
  internal::BusConnection connection(dbus_bus_get_private(DBUS_BUS_SESSION, error.get()));
  if (!connection) {
    ZYPAK_ASSERT(error);
    Log() << "Failed to connect to session bus: " << error;
    return false;
  }

  ZYPAK_ASSERT(connection);
  dbus_connection_set_exit_on_disconnect(connection.get(), false);

  bus_thread_ = internal::BusThread::Create(
      std::move(connection), std::bind(&Bus::HandleSignal, this, std::placeholders::_1));
  if (!bus_thread_) {
    return false;
  }

  bus_thread_->Start();

  return true;
}

MethodCall Bus::BuildGetPropertyCall(FloatingRef ref, std::string_view property) {
  constexpr std::string_view kPropertiesIface = "org.freedesktop.DBus.Properties";
  constexpr std::string_view kGetMethod = "Get";

  MethodCall call(FloatingRef(ref.service(), ref.object(), kPropertiesIface), kGetMethod);
  MessageWriter writer = call.OpenWriter();
  writer.Write<TypeCode::kString>(ref.interface());
  writer.Write<TypeCode::kString>(property);

  return call;
}

void Bus::HandleSignal(Signal signal) {
  for (const auto& handler : signal_handlers_) {
    if (signal.Test(handler.interface, handler.signal)) {
      handler.handler(signal);
    }
  }
}

}  // namespace zypak::dbus
