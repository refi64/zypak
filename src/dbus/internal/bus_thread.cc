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
  auto ev = EvLoop::Create();
  if (!ev) {
    return nullptr;
  }

  // Note that we can't just use sd-event's exit functionality here, as a bus thread may be
  // *temporarily* shut down so a fork can safely occur, but an sd-event exit is permanent.
  ShutdownFlag shutdown_flag = std::make_unique<std::atomic<bool>>(false);

  std::optional<EvLoop::TriggerSourceRef> shutdown_source, dispatch_source;

  // The execution of the task will itself be an iteration of the main loop,
  // so the loop will be able to check the trigger status in between.
  shutdown_source =
      ev->AddTrigger([flag = shutdown_flag.get()](EvLoop::SourceRef source) { flag->store(true); });

  dispatch_source = ev->AddTrigger([conn = connection.get()](EvLoop::SourceRef source) {
    Debug() << "Dispatching on bus thread";

    for (;;) {
      switch (dbus_connection_get_dispatch_status(conn)) {
      case DBUS_DISPATCH_DATA_REMAINS:
        dbus_connection_dispatch(conn);
        break;
      case DBUS_DISPATCH_COMPLETE:
        return;
      case DBUS_DISPATCH_NEED_MEMORY:
        ZYPAK_ASSERT(false, << "D-Bus hit OOM");
      }
    }
  });

  if (!shutdown_source || !dispatch_source) {
    Log() << "Could not add required sources for bus thread";
    return {};
  }

  Triggers tasks{std::move(*shutdown_source), std::move(*dispatch_source)};

  // Can't use make_unique, because our constructor is private.
  return std::unique_ptr<BusThread>(new BusThread(std::move(connection), std::move(signal_handler),
                                                  std::move(*ev), std::move(shutdown_flag),
                                                  std::move(tasks)));
}

bool BusThread::IsRunning() const { return thread_.joinable(); }

void BusThread::Start() {
  // Make sure if this is a restart, the shutdown flag isn't set.
  shutdown_flag_->store(false);

  // Re-initialize the thread value to start the main loop.
  thread_ = std::thread(std::bind(&BusThread::ThreadMain, this));
}

void BusThread::Shutdown() {
  Debug() << "Shutting down bus thread...";

  if (thread_.joinable()) {
    {
      // Need to lock to activate triggers.
      auto ev = ev_.Acquire();
      triggers_.shutdown.Trigger();
    }
    thread_.join();
  } else {
    Log() << "Bus thread is not joinable";
  }

  Debug() << "Bus thread shutdown complete";
  thread_ = std::thread();
}

void BusThread::SendCall(MethodCall call, CallHandler handler) {
  auto ev = ev_.Acquire();
  ZYPAK_ASSERT(ev->AddTask([this, call = std::move(call), handler](EvLoop::SourceRef source) {
    DBusPendingCall* pending = nullptr;
    ZYPAK_ASSERT(dbus_connection_send_with_reply(connection_.get(), call.message(), &pending, -1));
    ZYPAK_ASSERT(pending);

    ZYPAK_ASSERT(dbus_pending_call_set_notify(
        pending,
        [](DBusPendingCall* pending, void* data) {
          auto* handler = static_cast<CallHandler*>(data);
          (*handler)(Reply(dbus_pending_call_steal_reply(pending)));
        },
        new CallHandler(handler), [](void* data) { delete static_cast<CallHandler*>(data); }));
  }));
}

void BusThread::AddMatch(std::string match, MatchErrorHandler handler) {
  auto ev = ev_.Acquire();
  ev->AddTask([this, match = std::move(match), handler](EvLoop::SourceRef source) {
    Error error;
    dbus_bus_add_match(connection_.get(), match.c_str(), error.get());
    handler(std::move(error));
  });
}

BusThread::BusThread(BusConnection connection, SignalHandler signal_handler, EvLoop ev,
                     ShutdownFlag shutdown_flag, Triggers triggers)
    : ev_(std::move(ev)), signal_handler_(std::move(signal_handler)),
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
  while (!shutdown_flag_->load()) {
    Debug() << "Pumping bus thread";

    // Don't hold any lock while waiting, otherwise other threads won't be able to touch the bus at
    // all.
    switch (ev_.unsafe()->Wait()) {
    case EvLoop::WaitResult::kIdle:
      continue;
    case EvLoop::WaitResult::kReady:
      break;
    case EvLoop::WaitResult::kError:
      Log() << "EvLoop wait failed in bus thread! Aborting...";
      abort();
    }

    if (shutdown_flag_->load()) {
      Debug() << "Exit from bus thread with pending events";
    }

    Debug() << "Begin dispatch";
    auto ev = ev_.Acquire();
    switch (ev->Dispatch()) {
    case EvLoop::DispatchResult::kExit:
      // We never call Exit, so this is unexpected.
      ZYPAK_ASSERT(false, << "Unexpected loop exit");
    case EvLoop::DispatchResult::kContinue:
      continue;
    case EvLoop::DispatchResult::kError:
      Log() << "EvLoop iteration failed in bus thread! Aborting...";
      abort();
    }
  }
}

void BusThread::HandleDBusDispatchStatus(DBusDispatchStatus status) {
  Debug() << "Got D-Bus dispatch status";

  ZYPAK_ASSERT(status != DBUS_DISPATCH_NEED_MEMORY);
  if (status == DBUS_DISPATCH_DATA_REMAINS) {
    triggers_.dispatch.Trigger();
  }
}

void BusThread::HandleDBusWakeRequest() {
  Debug() << "Got D-Bus wake request";

  // XXX: We need to dispatch here, otherwise it'll lock, but I'm not sure why
  // HandleDBusDispatchStatus isn't always called instead?
  triggers_.dispatch.Trigger();
}

dbus_bool_t BusThread::HandleDBusWatchAdd(DBusWatch* watch) {
  // Only handle enabled watchers.
  if (!dbus_watch_get_enabled(watch)) {
    return true;
  }

  int fd = dbus_watch_get_unix_fd(watch);
  uint flags = dbus_watch_get_flags(watch);
  ZYPAK_ASSERT(flags & (DBUS_WATCH_READABLE | DBUS_WATCH_WRITABLE));

  Debug() << "D-Bus watch add " << dbus_watch_get_unix_fd(watch) << " with flags " << flags;

  EvLoop::Events events = EvLoop::Events::Status::kNone;
  if (flags & DBUS_WATCH_READABLE) {
    events |= EvLoop::Events::Status::kRead;
  }
  if (flags & DBUS_WATCH_WRITABLE) {
    events |= EvLoop::Events::Status::kWrite;
  }

  auto ev = ev_.Acquire();
  auto source = ev->AddFd(fd, events, [watch](EvLoop::SourceRef source, EvLoop::Events events) {
    Debug() << "Incoming events on D-Bus watch " << dbus_watch_get_unix_fd(watch) << ": "
            << static_cast<int>(events.status());

    uint flags = 0;
    ZYPAK_ASSERT(!events.empty());
    if (events.contains(EvLoop::Events::Status::kRead)) {
      flags |= DBUS_WATCH_READABLE;
    }
    if (events.contains(EvLoop::Events::Status::kWrite)) {
      flags |= DBUS_WATCH_WRITABLE;
    }

    ZYPAK_ASSERT(dbus_watch_handle(watch, flags));
    return true;
  });

  if (!source) {
    Log() << "Failed to add event poller for D-Bus watcher";
    return false;
  }

  EvLoop::SourceRef* heap_source = new EvLoop::SourceRef(*source);
  dbus_watch_set_data(watch, heap_source,
                      [](void* data) { delete static_cast<EvLoop::SourceRef*>(data); });

  return true;
}

void BusThread::HandleDBusWatchRemove(DBusWatch* watch) {
  Debug() << "D-Bus watch remove " << dbus_watch_get_unix_fd(watch);

  if (auto* source = static_cast<EvLoop::SourceRef*>(dbus_watch_get_data(watch))) {
    // Need to lock to disable sources.
    auto ev = ev_.Acquire();
    source->Disable();
    dbus_watch_set_data(watch, nullptr, nullptr);
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

  auto ev = ev_.Acquire();
  int ms = dbus_timeout_get_interval(timeout);
  auto source = ev->AddTimerMs(ms, [this, timeout](EvLoop::SourceRef source) {
    ZYPAK_ASSERT(dbus_timeout_handle(timeout));
    // XXX: Ugly code to re-arm the timer.
    HandleDBusTimeoutAdd(timeout);
  });

  if (!source) {
    Log() << "Failed to add event poller for D-Bus timeout";
    return false;
  }

  EvLoop::SourceRef* heap_source = new EvLoop::SourceRef(*source);
  dbus_timeout_set_data(timeout, heap_source,
                        [](void* data) { delete static_cast<EvLoop::SourceRef*>(data); });

  return true;
}

void BusThread::HandleDBusTimeoutRemove(DBusTimeout* timeout) {
  if (auto* source = static_cast<EvLoop::SourceRef*>(dbus_timeout_get_data(timeout))) {
    // Need to lock to disable sources.
    auto ev = ev_.Acquire();
    source->Disable();
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
