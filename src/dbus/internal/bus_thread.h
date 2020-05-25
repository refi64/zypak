// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include <dbus/dbus.h>

#include "base/base.h"
#include "base/epoll.h"
#include "base/guarded_value.h"
#include "dbus/bus_error.h"

namespace zypak::dbus {

class MethodCall;
class Reply;
class Signal;

namespace internal {

struct BusConnectionDeleter {
  void operator()(DBusConnection* connection) {
    if (connection != nullptr) {
      dbus_connection_close(connection);
      dbus_connection_unref(connection);
    }
  }
};

using BusConnection = std::unique_ptr<DBusConnection, BusConnectionDeleter>;

class BusThread {
 public:
  using CallHandler = std::function<void(Reply)>;
  using MatchErrorHandler = std::function<void(Error)>;
  using SignalHandler = std::function<void(Signal)>;

  BusThread(const BusThread& other) = delete;
  // Once the thread is started, moving this instance no longer safe.
  BusThread(BusThread&& other) = delete;
  ~BusThread() = default;

  static std::unique_ptr<BusThread> Create(BusConnection connection, SignalHandler signal_handler);

  DBusConnection* connection() { return connection_.get(); }
  RecursiveGuardedValue<Epoll>* loop() { return &epoll_; }

  void Start();
  void Shutdown();

  void SendCall(MethodCall call, CallHandler handler);
  void AddMatch(std::string match, MatchErrorHandler handler);

 private:
  using ShutdownFlag = std::unique_ptr<std::atomic<bool>>;

  struct Triggers {
    Epoll::Trigger shutdown;
    Epoll::Trigger dispatch;
  };

  BusThread(BusConnection connection, SignalHandler handler, Epoll epoll,
            ShutdownFlag shutdown_flag, Triggers triggers);

  void ThreadMain();

  dbus_bool_t HandleDBusWatchAdd(DBusWatch* watch);
  void HandleDBusWatchRemove(DBusWatch* watch);
  void HandleDBusWatchToggle(DBusWatch* watch);

  dbus_bool_t HandleDBusTimeoutAdd(DBusTimeout* timeout);
  void HandleDBusTimeoutRemove(DBusTimeout* timeout);
  void HandleDBusTimeoutToggle(DBusTimeout* timeout);

  void HandleDBusDispatchStatus(DBusDispatchStatus status);
  void HandleDBusWakeRequest();

  DBusHandlerResult HandleDBusMessage(DBusMessage* message);

  RecursiveGuardedValue<Epoll> epoll_;

  SignalHandler signal_handler_;
  ShutdownFlag shutdown_flag_;
  Triggers triggers_;

  std::thread thread_;

  // This *MUST* be last, as D-Bus will call into callbacks as it closes the connection, so epoll_
  // must still be alive.
  BusConnection connection_;
};

}  // namespace internal

}  // namespace zypak::dbus
