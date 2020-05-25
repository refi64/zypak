// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>
#include <thread>
#include <unordered_set>
#include <variant>

#include <dbus/dbus.h>

#include "base/base.h"
#include "base/epoll.h"
#include "dbus/bus_message.h"
#include "dbus/bus_readable_message.h"
#include "dbus/bus_writable_message.h"
#include "dbus/internal/bus_thread.h"

namespace zypak::dbus {

class Bus {
 public:
  using CallHandler = internal::BusThread::CallHandler;
  using SignalHandler = internal::BusThread::SignalHandler;

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

  static Bus* Acquire();

  RecursiveGuardedValue<Epoll>* loop() { return bus_thread_->loop(); }

  bool IsInitialized() const;
  void Shutdown();

  void Pause();
  void Resume();

  void CallAsync(MethodCall call, CallHandler callback);
  Reply CallBlocking(MethodCall call);

  void SignalConnect(std::string interface, std::string signal, SignalHandler handler);

  template <TypeCode Code>
  void GetPropertyAsync(FloatingRef ref, std::string_view property, PropertyHandler<Code> handler) {
    CallAsync(BuildGetPropertyCall(ref, property),
              [this, handler2 = std::move(handler), prop2 = std::string(property)](Reply reply) {
                handler2(ParseGetPropertyResult<Code>(reply, prop2));
              });
  }

  template <TypeCode Code>
  PropertyResult<Code> GetPropertyBlocking(FloatingRef ref, std::string_view property) {
    Reply reply = CallBlocking(BuildGetPropertyCall(std::move(ref), property));
    return ParseGetPropertyResult<Code>(std::move(reply), property);
  }

 private:
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
