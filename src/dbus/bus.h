// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <dbus/dbus.h>

#include <memory>
#include <thread>
#include <unordered_set>
#include <variant>

#include "base/base.h"
#include "base/evloop.h"
#include "dbus/bus_message.h"
#include "dbus/bus_readable_message.h"
#include "dbus/bus_writable_message.h"
#include "dbus/internal/bus_thread.h"

namespace zypak::dbus {

// A connection to the session bus. This is a singleton value, acquired with Acquire.
// A bus instance works by using a separate thread that is running event loop, then communicating
// with that thread as needed.
class Bus {
 public:
  using CallHandler = internal::BusThread::CallHandler;
  using SignalHandler = internal::BusThread::SignalHandler;

  // Result from attempting to access a property. InvocationError is used if D-Bus itself
  // returned an error accessing the property, whereas monostate is used if some error local
  // to the current application occurred.
  template <TypeCode Code>
  using PropertyResult =
      std::variant<typename BusTypeTraits<Code>::External, InvocationError, std::monostate>;

  template <TypeCode Code>
  using PropertyHandler = std::function<void(PropertyResult<Code>)>;

  Bus() {}
  // No destructor, because this is a singleton and should *not* be deleted.
  ~Bus() = delete;
  Bus(const Bus& other) = delete;
  Bus(Bus&& other) = delete;

  // Acquires a shared instance of the bus. If nullptr is returned, then creation of the bus object
  // failed.
  static Bus* Acquire();

  // Returns the event loop underlying the bus instance.
  RecursiveGuardedValue<EvLoop>* evloop() { return bus_thread_->evloop(); }

  // Fully shuts down the bus and its associated thread. The bus is considered invalid at this
  // point and may not be reused.
  void Shutdown();

  // Pauses the bus by stopping the bus thread. This is useful to avoid having threads in an
  // undefined state when forking.
  void Pause();
  // Resumes the bus thread after a call to Pause.
  void Resume();

  // Performs an async call to the given MethodCall. The handler will be called with the reply
  // when available.
  void CallAsync(MethodCall call, CallHandler handler);
  // Performs a blocking call to the given MethodCall, returning the reply once complete.
  Reply CallBlocking(MethodCall call);

  // Connects to the given signal emitted by the given interface.
  void SignalConnect(std::string interface, std::string signal, SignalHandler handler);

  // Gets a property of the given interface asynchronously, calling the given handler once the
  // property is available, or an error has occurred.
  template <TypeCode Code>
  void GetPropertyAsync(FloatingRef ref, std::string_view property, PropertyHandler<Code> handler) {
    CallAsync(BuildGetPropertyCall(ref, property),
              [this, handler = std::move(handler), prop = std::string(property)](Reply reply) {
                handler(ParseGetPropertyResult<Code>(reply, prop));
              });
  }

  // Gets a property of theg given interface blocking, returning the result or error.
  template <TypeCode Code>
  PropertyResult<Code> GetPropertyBlocking(FloatingRef ref, std::string_view property) {
    Reply reply = CallBlocking(BuildGetPropertyCall(std::move(ref), property));
    return ParseGetPropertyResult<Code>(std::move(reply), property);
  }

 private:
  bool IsInitialized() const;
  bool Initialize();

  MethodCall BuildGetPropertyCall(FloatingRef ref, std::string_view property);

  template <TypeCode Code>
  PropertyResult<Code> ParseGetPropertyResult(Reply reply, std::string_view property) {
    if (auto error = reply.ReadError()) {
      return std::move(*error);
    }

    MessageReader reader = reply.OpenReader();
    if (std::optional<MessageReader> variant = reader.EnterContainer<TypeCode::kVariant>()) {
      typename BusTypeTraits<Code>::External external;
      if (variant->Read<Code>(&external)) {
        return external;
      }
    }

    Log() << "Failed to read property '" << property << "'";
    return std::monostate();
  }

  void HandleSignal(Signal signal);

  std::unique_ptr<internal::BusThread> bus_thread_;

  struct SignalHandlerSpec {
    std::string interface;
    std::string signal;
    SignalHandler handler;
  };
  std::vector<SignalHandlerSpec> signal_handlers_;
};

}  // namespace zypak::dbus
