// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/evloop.h"

#include <sys/eventfd.h>
#include <sys/poll.h>

#include <array>
#include <cstdint>

#include <systemd/sd-event.h>

#include "base/base.h"
#include "base/debug.h"

namespace zypak {

namespace {

constexpr int kMillisecondsPerSecond = 1000;
constexpr int kMicrosecondsPerMillisecond = 1000;
constexpr int kDefaultAccuracyMs = 50;

template <typename Handler>
struct CallbackParams {
  zypak::EvLoop* evloop;
  Handler handler;
  std::vector<zypak::EvLoop::DestroyHandler> on_destroy;
};

void DisableSource(sd_event_source* source) {
  Debug() << "Disable source " << source;

  ZYPAK_ASSERT_SD_ERROR(sd_event_source_set_enabled(source, SD_EVENT_OFF));
  ZYPAK_ASSERT_SD_ERROR(sd_event_source_set_floating(source, false));
}

}  // namespace

// static
std::optional<EvLoop> EvLoop::Create() {
  unique_fd notify_defer(eventfd(0, EFD_NONBLOCK));
  if (notify_defer.invalid()) {
    Errno() << "Failed to create notify eventfd";
    return {};
  }

  sd_event* event = nullptr;
  if (int err = sd_event_new(&event); err < 0) {
    Errno(-err) << "Failed to create event loop";
    return {};
  }

  return EvLoop(event, std::move(notify_defer));
}

EvLoop::EvLoop(sd_event* event, unique_fd notify_defer_fd)
    : event_(event), notify_defer_fd_(std::move(notify_defer_fd)) {}

EvLoop::SourceRef::State EvLoop::SourceRef::state() const {
  int enabled = -1;
  ZYPAK_ASSERT_SD_ERROR(sd_event_source_get_enabled(source_, &enabled));

  switch (enabled) {
  case SD_EVENT_OFF:
    return State::kDisabled;
  case SD_EVENT_ONESHOT:
    return State::kActiveOnce;
  case SD_EVENT_ON:
    return State::kActiveForever;
  default:
    ZYPAK_ASSERT(false, << "Invalid sd-event state: " << enabled);
  }
}

void EvLoop::SourceRef::Disable() { DisableSource(source_); }

void EvLoop::SourceRef::AddDestroyHandler(DestroyHandler handler) {
  on_destroy_->push_back(std::move(handler));
}

void EvLoop::TriggerSourceRef::Trigger() {
  Debug() << "Trigger source " << source_.source_;

  ZYPAK_ASSERT_SD_ERROR(sd_event_source_set_enabled(source_.source_, SD_EVENT_ONESHOT));
  ZYPAK_ASSERT_SD_ERROR(sd_event_source_set_floating(source_.source_, true));

  ZYPAK_ASSERT_WITH_ERRNO(eventfd_write(notify_defer_fd_, 1) != -1);
}

std::optional<EvLoop::SourceRef> EvLoop::AddTask(EvLoop::EventHandler handler) {
  ZYPAK_ASSERT(handler, << "Missing handler for task");

  sd_event_source* source = nullptr;
  if (int err = sd_event_add_defer(event_.get(), &source, &GenericHandler<EventHandler>, nullptr);
      err < 0) {
    Errno(-err) << "Failed to add task";
    return {};
  }

  Debug() << "Added task source " << source;
  SourceRef source_ref = SourceSetup(source, std::move(handler));

  // Notify any waiters.
  if (eventfd_write(notify_defer_fd_.get(), 1) == -1) {
    Errno() << "WARNING: Failed to notify defer fd";
  }

  return source_ref;
}

std::optional<EvLoop::TriggerSourceRef> EvLoop::AddTrigger(EvLoop::EventHandler handler) {
  auto source = AddTask(handler);
  if (!source) {
    return {};
  }

  ZYPAK_ASSERT_SD_ERROR(sd_event_source_set_enabled(source->source_, SD_EVENT_OFF));

  Debug() << "Lifting task " << source->source_ << " to trigger";
  return TriggerSourceRef(std::move(*source), notify_defer_fd_.get());
}

std::optional<EvLoop::SourceRef> EvLoop::AddTimerSec(int seconds, EvLoop::EventHandler handler) {
  return AddTimerMs(seconds * kMillisecondsPerSecond, handler);
}

std::optional<EvLoop::SourceRef> EvLoop::AddTimerMs(int ms, EvLoop::EventHandler handler) {
  ZYPAK_ASSERT(handler, << "Missing handler for timer, ms = " << ms);

  constexpr int kClock = CLOCK_MONOTONIC;

  std::uint64_t now;
  if (int err = sd_event_now(event_.get(), kClock, &now); err < 0) {
    Errno(-err) << "Failed to get current clock tick";
    return {};
  }

  std::uint64_t target_time = now + ms * kMicrosecondsPerMillisecond;

  sd_event_source* source = nullptr;
  if (int err = sd_event_add_time(event_.get(), &source, kClock, target_time,
                                  kDefaultAccuracyMs * kMicrosecondsPerMillisecond,
                                  &HandleTimeEvent, nullptr);
      err < 0) {
    Errno(-err) << "Failed to add timer event";
    return {};
  }

  Debug() << "Added timer source " << source << " with duration " << ms << "ms";

  return SourceSetup(source, std::move(handler));
}

std::optional<EvLoop::SourceRef> EvLoop::AddFd(int fd, Events events,
                                               EvLoop::IoEventHandler handler) {
  ZYPAK_ASSERT(!events.empty(), << "Missing events for fd" << fd);
  ZYPAK_ASSERT(handler, << "Missing handler for fd " << fd);

  int epoll_events = 0;
  if (events.contains(Events::Status::kRead)) {
    epoll_events |= EPOLLIN;
  }
  if (events.contains(Events::Status::kWrite)) {
    epoll_events |= EPOLLOUT;
  }

  sd_event_source* source;
  if (int err = sd_event_add_io(event_.get(), &source, fd, epoll_events, &HandleIoEvent, nullptr);
      err < 0) {
    Errno(-err) << "Failed to add I/O event";
    return {};
  }

  Debug() << "Adding I/O source " << source << " for " << fd;
  return SourceSetup(source, std::move(handler));
}

std::optional<EvLoop::SourceRef> EvLoop::TakeFd(unique_fd fd, Events events,
                                                EvLoop::IoEventHandler handler) {
  auto source = AddFd(fd.get(), events, std::move(handler));
  if (!source) {
    return {};
  }

  sd_event_source_set_io_fd_own(source->source_, true);
  return source;
}

EvLoop::WaitResult EvLoop::Wait() {
  int pending = sd_event_prepare(event_.get());
  if (pending < 0) {
    Errno(-pending) << "Failed to prepare event loop";
    return WaitResult::kError;
  } else if (pending > 0) {
    // No need to wait, we know some events are ready.
    ClearNotifyDeferFd();
    return WaitResult::kReady;
  }

  std::array<struct pollfd, 2> pfds;

  pfds[0].fd = sd_event_get_fd(event_.get());
  ZYPAK_ASSERT_SD_ERROR(pfds[0].fd);

  pfds[1].fd = notify_defer_fd_.get();

  pfds[0].events = pfds[1].events = POLLIN;
  pfds[0].revents = pfds[1].revents = 0;

  int ready = HANDLE_EINTR(poll(pfds.data(), pfds.size(), -1));
  if (ready == -1) {
    Errno() << "Failed to poll on sd-event fd";
    return WaitResult::kError;
  }

  if (ready > 0) {
    if ((pfds[0].revents | pfds[1].revents) & (POLLHUP | POLLERR)) {
      Log() << "sd-event fd is in an error state";
      return WaitResult::kError;
    }

    if ((pfds[0].revents | pfds[1].revents) & POLLIN) {
      if (pfds[1].revents & POLLIN) {
        ClearNotifyDeferFd();
      }

      return WaitResult::kReady;
    }

    ZYPAK_ASSERT((pfds[0].revents | pfds[1].revents) == 0,
                 << "Unexpected revents values: " << pfds[0].revents << ' ' << pfds[1].revents);
  }

  Debug() << "Poll returned without events?";
  return WaitResult::kIdle;
}

EvLoop::DispatchResult EvLoop::Dispatch() {
  if (sd_event_get_state(event_.get()) != SD_EVENT_PENDING) {
    int pending = sd_event_wait(event_.get(), 0);
    if (pending < 0) {
      Errno(-pending) << "Failed to update event loop waiting state";
      return DispatchResult::kError;
    } else if (pending == 0) {
      Log() << "Wait found events, but sd-event found none";
      return DispatchResult::kContinue;
    }
  }

  int result = sd_event_dispatch(event_.get());
  if (result < 0) {
    Errno(-result) << "Failed to run event loop iteration";
    return DispatchResult::kError;
  } else if (result == 0) {
    return DispatchResult::kExit;
  }

  return DispatchResult::kContinue;
}

bool EvLoop::Exit(EvLoop::ExitStatus status) {
  if (int err = sd_event_exit(event_.get(), static_cast<int>(status)); err < 0) {
    Errno(-err) << "Failed to exit event loop";
    return false;
  }

  return true;
}

EvLoop::ExitStatus EvLoop::exit_status() const {
  int code;
  ZYPAK_ASSERT_SD_ERROR(sd_event_get_exit_code(event_.get(), &code));

  return static_cast<EvLoop::ExitStatus>(code);
}

void EvLoop::ClearNotifyDeferFd() {
  std::uint64_t value;
  ZYPAK_ASSERT_WITH_ERRNO(eventfd_read(notify_defer_fd_.get(), &value) != -1 || errno == EAGAIN);
}

template <typename Handler>
// static
EvLoop::SourceRef EvLoop::SourceSetup(sd_event_source* source, Handler handler) {
  auto* params = new CallbackParams<Handler>{this, std::move(handler)};

  sd_event_source_set_floating(source, true);
  sd_event_source_set_userdata(source, params);
  sd_event_source_set_destroy_callback(source, [](void* data) {
    auto* params = static_cast<CallbackParams<Handler>*>(data);
    for (DestroyHandler handler : params->on_destroy) {
      handler();
    }
    delete params;
  });

  return SourceRef(source, &params->on_destroy);
}

template <typename Handler, typename... Args>
// static
int EvLoop::GenericHandler(sd_event_source* source, void* data, Args&&... args) {
  Debug() << "Received event from " << source;

  auto* params = static_cast<CallbackParams<Handler>*>(data);

  // SourceRef doesn't ref its argument, as it usually "steals" a brand-new source. However, if we
  // don't ref it here, it'll be unref'd in SourceRef's destructor, thus unref-ing the floating
  // reference the event loop has and likely causing memory errors.
  sd_event_source_ref(source);
  SourceRef source_ref(source, &params->on_destroy);

  params->handler(std::move(source_ref), std::forward<Args>(args)...);

  int enabled = -1;
  ZYPAK_ASSERT_SD_ERROR(sd_event_source_get_enabled(source, &enabled));

  if (enabled != SD_EVENT_ON) {
    // If it's already disabled, make sure it's no longer floating so it won't leak.
    ZYPAK_ASSERT_SD_ERROR(sd_event_source_set_floating(source, false));
  }

  return 0;
}

// static
int EvLoop::HandleIoEvent(sd_event_source* source, int fd, std::uint32_t revents, void* data) {
  if (revents & (EPOLLHUP | EPOLLERR)) {
    std::string_view reason = revents & EPOLLHUP ? "connection closed" : "unknown error";
    Log() << "Dropping " << source << " (" << fd << ") because of " << reason;
    DisableSource(source);
    return -ECONNRESET;
  }

  Events events(Events::Status::kNone);
  if (revents & EPOLLIN) {
    events |= Events::Status::kRead;
  }
  if (revents & EPOLLOUT) {
    events |= Events::Status::kWrite;
  }

  return GenericHandler<IoEventHandler>(source, data, events);
}

// static
int EvLoop::HandleTimeEvent(sd_event_source* source, std::uint64_t us, void* data) {
  return GenericHandler<EventHandler>(source, data);
}

}  // namespace zypak
