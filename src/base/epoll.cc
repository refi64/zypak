// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/epoll.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/poll.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <memory>

#include <systemd/sd-event.h>

#include "base/base.h"
#include "base/debug.h"

namespace {

constexpr int kMillisecondsPerSecond = 1000;
constexpr int kMicrosecondsPerMillisecond = 1000;
constexpr int kDefaultAccuracyMs = 50;

template <typename Handler>
struct CallbackParams {
  zypak::Epoll* epoll;
  Handler handler;
};

}  // namespace

namespace zypak {

// static
std::optional<Epoll> Epoll::Create() {
  unique_fd notify_defer(eventfd(0, 0));
  if (notify_defer.invalid()) {
    Errno() << "Failed to create notify eventfd";
    return {};
  }

  sd_event* event = nullptr;
  if (int err = sd_event_new(&event); err < 0) {
    Errno(-err) << "Failed to create event loop";
    return {};
  }

  return Epoll(event, std::move(notify_defer));
}

Epoll::Epoll(sd_event* event, unique_fd notify_defer_fd)
    : event_(event), notify_defer_fd_(std::move(notify_defer_fd)) {}

Epoll::SourceRef::State Epoll::SourceRef::state() const {
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

void Epoll::SourceRef::Disable() {
  Debug() << "Disable source " << source_;

  ZYPAK_ASSERT_SD_ERROR(sd_event_source_set_enabled(source_, SD_EVENT_OFF));
  ZYPAK_ASSERT_SD_ERROR(sd_event_source_set_floating(source_, false));
}

void Epoll::TriggerSourceRef::Trigger() {
  Debug() << "Trigger source " << source_.source_;

  ZYPAK_ASSERT_SD_ERROR(sd_event_source_set_enabled(source_.source_, SD_EVENT_ONESHOT));
  ZYPAK_ASSERT_SD_ERROR(sd_event_source_set_floating(source_.source_, true));

  ZYPAK_ASSERT_WITH_ERRNO(eventfd_write(notify_defer_fd_, 1) != -1);
}

std::optional<Epoll::SourceRef> Epoll::AddTask(Epoll::EventHandler handler) {
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

std::optional<Epoll::TriggerSourceRef> Epoll::AddTrigger(Epoll::EventHandler handler) {
  auto source = AddTask(handler);
  if (!source) {
    return {};
  }

  ZYPAK_ASSERT_SD_ERROR(sd_event_source_set_enabled(source->source_, SD_EVENT_OFF));

  Debug() << "Lifting task " << source->source_ << " to trigger";
  return TriggerSourceRef(std::move(*source), notify_defer_fd_.get());
}

std::optional<Epoll::SourceRef> Epoll::AddTimerSec(int seconds, Epoll::EventHandler handler) {
  return AddTimerMs(seconds * kMillisecondsPerSecond, handler);
}

std::optional<Epoll::SourceRef> Epoll::AddTimerMs(int ms, Epoll::EventHandler handler) {
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

std::optional<Epoll::SourceRef> Epoll::AddFd(int fd, Events events, Epoll::IoEventHandler handler) {
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

std::optional<Epoll::SourceRef> Epoll::TakeFd(unique_fd fd, Events events,
                                              Epoll::IoEventHandler handler) {
  auto source = AddFd(fd.get(), events, std::move(handler));
  if (!source) {
    return {};
  }

  sd_event_source_set_io_fd_own(source->source_, true);
  return source;
}

Epoll::WaitResult Epoll::Wait() {
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
        std::uint64_t value;
        ZYPAK_ASSERT_WITH_ERRNO(eventfd_read(notify_defer_fd_.get(), &value) != -1);
      }

      return WaitResult::kReady;
    }

    ZYPAK_ASSERT((pfds[0].revents | pfds[1].revents) == 0,
                 << "Unexpected revents values: " << pfds[0].revents << ' ' << pfds[1].revents);
  }

  Debug() << "Poll returned without events?";
  return WaitResult::kIdle;
}

Epoll::DispatchResult Epoll::Dispatch() {
  int result = sd_event_run(event_.get(), 0);
  if (result < 0) {
    Errno(-result) << "Failed to run event loop iteration";
    return DispatchResult::kError;
  } else if (result == 0) {
    if (sd_event_get_state(event_.get()) == SD_EVENT_FINISHED) {
      return DispatchResult::kExit;
    }

    Debug() << "Nothing to dispatch";
  }

  return DispatchResult::kContinue;
}

bool Epoll::Exit(Epoll::ExitStatus status) {
  if (int err = sd_event_exit(event_.get(), static_cast<int>(status)); err < 0) {
    Errno(-err) << "Failed to exit event loop";
    return false;
  }

  return true;
}

Epoll::ExitStatus Epoll::exit_status() const {
  int code;
  ZYPAK_ASSERT_SD_ERROR(sd_event_get_exit_code(event_.get(), &code));

  return static_cast<Epoll::ExitStatus>(code);
}

template <typename Handler>
// static
Epoll::SourceRef Epoll::SourceSetup(sd_event_source* source, Handler handler) {
  sd_event_source_set_floating(source, true);
  sd_event_source_set_userdata(source, new CallbackParams<Handler>{this, std::move(handler)});
  sd_event_source_set_destroy_callback(
      source, [](void* data) { delete static_cast<CallbackParams<Handler>*>(data); });
  return SourceRef(source);
}

template <typename Handler, typename... Args>
// static
int Epoll::GenericHandler(sd_event_source* source, void* data, Args&&... args) {
  Debug() << "Received event from " << source;

  auto* params = static_cast<CallbackParams<Handler>*>(data);

  // SourceRef doesn't ref its argument, as it usually "steals" a brand-new source. However, if we
  // don't ref it here, it'll be unref'd in SourceRef's destructor, thus unref-ing the floating
  // reference the event loop has and likely causing memory errors.
  sd_event_source_ref(source);
  SourceRef source_ref(source);

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
int Epoll::HandleIoEvent(sd_event_source* source, int fd, std::uint32_t revents, void* data) {
  if (revents & (EPOLLHUP | EPOLLERR)) {
    std::string_view reason = revents & EPOLLHUP ? "connection closed" : "unknown error";
    Log() << "Dropping " << fd << " because of " << reason;
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
int Epoll::HandleTimeEvent(sd_event_source* source, std::uint64_t us, void* data) {
  return GenericHandler<EventHandler>(source, data);
}

}  // namespace zypak
