// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/internal/bus_thread.h"

#include <thread>

#include "base/debug.h"
#include "dbus/bus_readable_message.h"
#include "dbus/bus_writable_message.h"
#include "dbus/internal/dbus_member_callback.h"

namespace zypak::dbus::internal {

// static
std::unique_ptr<BusThread> BusThread::Create(BusConnection connection,
                                             SignalHandler signal_handler) {
  auto epoll = Epoll::Create();
  if (!epoll) {
    return nullptr;
  }

  ShutdownFlag shutdown_flag = std::make_unique<std::atomic<bool>>(false);

  std::optional<Epoll::TriggerReceiver> shutdown_receiver, trigger_receiver;

  // The execution of the trigger will itself be an iteration of the main loop,
  // so the loop will be able to check the trigger status in between.
  shutdown_receiver = epoll->AddTrigger(
      [flag = shutdown_flag.get()](Epoll* epoll, Epoll::TriggerReceiver receiver) mutable {
        flag->store(true);
        receiver.GetAndClear();
        return true;
      });

  trigger_receiver = epoll->AddTrigger(
      [conn = connection.get()](Epoll* epoll, Epoll::TriggerReceiver receiver) mutable {
        receiver.GetAndClear();

        Debug() << "Dispatching on bus thread";

        for (;;) {
          switch (dbus_connection_get_dispatch_status(conn)) {
          case DBUS_DISPATCH_DATA_REMAINS:
            dbus_connection_dispatch(conn);
            break;
          case DBUS_DISPATCH_COMPLETE:
            return true;
          case DBUS_DISPATCH_NEED_MEMORY:
            ZYPAK_ASSERT(false, << "D-Bus hit OOM");
          }
        }
      });

  if (!shutdown_receiver || !trigger_receiver) {
    Log() << "Could not get trigger receivers for bus thread";
    return {};
  }

  Triggers triggers{shutdown_receiver->trigger(), trigger_receiver->trigger()};

  // Can't use make_unique, because our constructor is private.
  return std::unique_ptr<BusThread>(new BusThread(std::move(connection), std::move(signal_handler),
                                                  std::move(*epoll), std::move(shutdown_flag),
                                                  triggers));
}

void BusThread::Start() {
  // Make sure if this is a restart, the shutdown flag isn't set.
  shutdown_flag_->store(false);

  // Re-initialize the thread value to start the main loop.
  thread_ = std::thread(std::bind(&BusThread::ThreadMain, this));
}

void BusThread::Shutdown() {
  Debug() << "Shutting down bus thread...";

  if (thread_.joinable()) {
    triggers_.shutdown.Add();
    thread_.join();
  } else {
    Log() << "Bus thread is not joinable";
  }

  Debug() << "Bus thread shutdown complete";
  thread_ = std::thread();
}

void BusThread::SendCall(MethodCall call, CallHandler handler) {
  auto ep = epoll_.Acquire();
  ZYPAK_ASSERT(ep->AddTask([this, call2 = std::move(call), handler](Epoll* unsafe_ep) {
    DBusPendingCall* pending = nullptr;
    ZYPAK_ASSERT(dbus_connection_send_with_reply(connection_.get(), call2.message(), &pending, -1));
    ZYPAK_ASSERT(pending);

    ZYPAK_ASSERT(dbus_pending_call_set_notify(
        pending,
        [](DBusPendingCall* pending, void* data) {
          auto* handler = static_cast<CallHandler*>(data);
          (*handler)(Reply(dbus_pending_call_steal_reply(pending)));
        },
        new CallHandler(handler), [](void* data) { delete static_cast<CallHandler*>(data); }));

    return true;
  }));
}

void BusThread::AddMatch(std::string match, MatchErrorHandler handler) {
  auto ep = epoll_.Acquire();
  ep->AddTask([this, match2 = std::move(match), handler](Epoll* unsafe_ep) {
    Error error;
    dbus_bus_add_match(connection_.get(), match2.c_str(), error.get());
    handler(std::move(error));
    return true;
  });
}

BusThread::BusThread(BusConnection connection, SignalHandler signal_handler, Epoll epoll,
                     ShutdownFlag shutdown_flag, Triggers triggers)
    : epoll_(std::move(epoll)), signal_handler_(std::move(signal_handler)),
      shutdown_flag_(std::move(shutdown_flag)), triggers_(triggers),
      connection_(std::move(connection)) {
  dbus_connection_set_dispatch_status_function(
      connection_.get(),
      MakeDBusMemberCallback<&BusThread::HandleDBusDispatchStatus, Ignored<DBusConnection*>>(),
      this, nullptr);

  dbus_connection_set_wakeup_main_function(
      connection_.get(), MakeDBusMemberCallback<&BusThread::HandleDBusWakeRequest>(), this,
      nullptr);

  ZYPAK_ASSERT(dbus_connection_set_watch_functions(
      connection_.get(), MakeDBusMemberCallback<&BusThread::HandleDBusWatchAdd>(),
      MakeDBusMemberCallback<&BusThread::HandleDBusWatchRemove>(),
      MakeDBusMemberCallback<&BusThread::HandleDBusWatchToggle>(), this, nullptr));

  ZYPAK_ASSERT(dbus_connection_set_timeout_functions(
      connection_.get(), MakeDBusMemberCallback<&BusThread::HandleDBusTimeoutAdd>(),
      MakeDBusMemberCallback<&BusThread::HandleDBusTimeoutRemove>(),
      MakeDBusMemberCallback<&BusThread::HandleDBusTimeoutToggle>(), this, nullptr));

  ZYPAK_ASSERT(dbus_connection_add_filter(
      connection_.get(),
      MakeDBusMemberCallback<&BusThread::HandleDBusMessage, Ignored<DBusConnection*>>(), this,
      nullptr));
}

void BusThread::ThreadMain() {
  static Epoll::EventSet events;

  if (events.count() != 0) {
    Debug() << "Delayed dispatch of " << events.count() << " events";
    auto ep = epoll_.Acquire();
    if (!ep->Dispatch(events)) {
      Log() << "Epoll iteration failed in bus thread! Aborting...";
      abort();
    }
  }

  while (!shutdown_flag_->load()) {
    Debug() << "Pumping bus thread";

    if (!epoll_.unsafe()->Wait(&events)) {
      Log() << "Epoll wait failed in bus thread! Aborting...";
      abort();
    }

    if (shutdown_flag_->load()) {
      Debug() << "Early exit from bus thread with " << events.count() << " pending events";
    }

    Debug() << "Begin dispatch";
    auto ep = epoll_.Acquire();
    if (!ep->Dispatch(events)) {
      Log() << "Epoll iteration failed in bus thread! Aborting...";
      abort();
    }
    Debug() << "End dispatch";
    events.Clear();
  }
}

void BusThread::HandleDBusDispatchStatus(DBusDispatchStatus status) {
  Debug() << "Got D-Bus dispatch status";

  ZYPAK_ASSERT(status != DBUS_DISPATCH_NEED_MEMORY);
  if (status == DBUS_DISPATCH_DATA_REMAINS) {
    triggers_.dispatch.Add();
  }
}

void BusThread::HandleDBusWakeRequest() {
  Debug() << "Got D-Bus wake request";
  // XXX: We need to dispatch here, otherwise it'll lock, but I'm not sure why
  // HandleDBusDispatchStatus isn't always called instead?
  triggers_.dispatch.Add();
}

dbus_bool_t BusThread::HandleDBusWatchAdd(DBusWatch* watch) {
  // Only handle enabled watchers.
  if (!dbus_watch_get_enabled(watch)) {
    return true;
  }

  int fd = dbus_watch_get_unix_fd(watch);
  uint flags = dbus_watch_get_flags(watch);
  ZYPAK_ASSERT(flags & (DBUS_WATCH_READABLE | DBUS_WATCH_WRITABLE));

  Debug() << "D-Bus watch add with flags " << flags;

  Epoll::Events events = Epoll::Events::Status::kNone;
  if (flags & DBUS_WATCH_READABLE) {
    events |= Epoll::Events::Status::kRead;
  }
  if (flags & DBUS_WATCH_WRITABLE) {
    events |= Epoll::Events::Status::kWrite;
  }

  auto ep = epoll_.Acquire();
  auto opt_id = ep->AddFd(fd, events, [watch](Epoll* unsafe_ep, Epoll::Events events) {
    uint flags = 0;
    ZYPAK_ASSERT(!events.empty());
    if (events.contains(Epoll::Events::Status::kRead)) {
      flags |= DBUS_WATCH_READABLE;
    }
    if (events.contains(Epoll::Events::Status::kWrite)) {
      flags |= DBUS_WATCH_WRITABLE;
    }

    ZYPAK_ASSERT(dbus_watch_handle(watch, flags));
    return true;
  });

  if (!opt_id) {
    Log() << "Failed to add event poller for D-Bus watcher";
    return false;
  }

  Epoll::Id* heap_id = new Epoll::Id(*opt_id);
  dbus_watch_set_data(watch, heap_id, [](void* data) { delete static_cast<Epoll::Id*>(data); });

  return true;
}

void BusThread::HandleDBusWatchRemove(DBusWatch* watch) {
  if (auto* id = static_cast<Epoll::Id*>(dbus_watch_get_data(watch))) {
    auto ep = epoll_.Acquire();
    ep->Remove(*id);
  }
}

void BusThread::HandleDBusWatchToggle(DBusWatch* watch) {
  if (dbus_watch_get_enabled(watch)) {
    HandleDBusWatchAdd(watch);
  } else {
    HandleDBusWatchRemove(watch);
  }
}

dbus_bool_t BusThread::HandleDBusTimeoutAdd(DBusTimeout* timeout) {
  // Only handle enabled timeouts.
  if (!dbus_timeout_get_enabled(timeout)) {
    return true;
  }

  auto ep = epoll_.Acquire();

  int ms = dbus_timeout_get_interval(timeout);
  auto opt_id = ep->AddTimerMs(
      ms,
      [timeout](Epoll* ep) {
        ZYPAK_ASSERT(dbus_timeout_handle(timeout));
        return true;
      },
      /*repeat=*/true);

  if (!opt_id) {
    Log() << "Failed to add event poller for D-Bus timeout";
    return false;
  }

  Epoll::Id* heap_id = new Epoll::Id(*opt_id);
  dbus_timeout_set_data(timeout, heap_id, [](void* data) { delete static_cast<Epoll::Id*>(data); });

  return true;
}

void BusThread::HandleDBusTimeoutRemove(DBusTimeout* timeout) {
  if (auto* id = static_cast<Epoll::Id*>(dbus_timeout_get_data(timeout))) {
    auto ep = epoll_.Acquire();
    ep->Remove(*id);
  }
}

void BusThread::HandleDBusTimeoutToggle(DBusTimeout* timeout) {
  if (dbus_timeout_get_enabled(timeout)) {
    HandleDBusTimeoutAdd(timeout);
  } else {
    HandleDBusTimeoutRemove(timeout);
  }
}

DBusHandlerResult BusThread::HandleDBusMessage(DBusMessage* message) {
  if (dbus_message_get_type(message) == DBUS_MESSAGE_TYPE_SIGNAL) {
    signal_handler_(Signal(message));
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

}  // namespace zypak::dbus::internal
