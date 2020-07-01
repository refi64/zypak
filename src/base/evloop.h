// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

#include <systemd/sd-event.h>

#include "base/unique_fd.h"

namespace zypak {

// A C++-friendly wrapper over sd-event.
class EvLoop {
 public:
  EvLoop(const EvLoop& other) = delete;
  EvLoop(EvLoop&& other) = default;
  ~EvLoop() {}

  static std::optional<EvLoop> Create();

  class Events {
   public:
    enum class Status {
      kNone = 0,
      kRead = 1 << 0,
      kWrite = 1 << 1,
      kReadWrite = (1 << 0) | (1 << 1)
    };

    constexpr Events(Status status) : status_(status) {}

    constexpr Status status() const { return status_; }
    constexpr bool empty() const { return status_ == Status::kNone; }

    constexpr bool contains(Events events) const {
      return (static_cast<int>(status_) & static_cast<int>(events.status_)) != 0;
    }

    constexpr Events operator|(Events other) const {
      return {static_cast<Status>(static_cast<int>(status_) | static_cast<int>(other.status_))};
    }

    Events& operator|=(Events other) {
      *this = *this | other;
      return *this;
    }

   private:
    Status status_;
  };

  class SourceRef {
   public:
    SourceRef(const SourceRef& other) : source_(sd_event_source_ref(other.source_)) {}
    SourceRef(SourceRef&& other) : source_(other.source_) { other.source_ = nullptr; }
    ~SourceRef() { sd_event_source_unref(source_); }

    friend void swap(SourceRef& a, SourceRef& b) {
      using std::swap;
      swap(a.source_, b.source_);
    }

    SourceRef& operator=(SourceRef other) {
      swap(*this, other);
      return *this;
    }

    enum State { kActiveForever, kActiveOnce, kDisabled };
    State state() const;

    void Disable();

   private:
    explicit SourceRef(sd_event_source* source) : source_(source) {}

    sd_event_source* source_;

    friend class EvLoop;
    friend class TriggerSourceRef;
  };

  class TriggerSourceRef {
   public:
    void Trigger();

    const SourceRef& source() const { return source_; }

    SourceRef::State state() const { return source_.state(); }
    void Disable() { source_.Disable(); }

   private:
    TriggerSourceRef(SourceRef source, int notify_defer_fd)
        : source_(std::move(source)), notify_defer_fd_(notify_defer_fd) {}

    SourceRef source_;
    int notify_defer_fd_;

    friend class EvLoop;
  };

  using EventHandler = std::function<void(SourceRef)>;
  using IoEventHandler = std::function<void(SourceRef, Events)>;

  // Adds a function that should run in the event loop's thread and environment.
  std::optional<SourceRef> AddTask(EventHandler handler);

  // TODO: document
  std::optional<TriggerSourceRef> AddTrigger(EventHandler handler);

  // Add a new timer that fires after the given # of seconds / milliseconds.
  std::optional<SourceRef> AddTimerSec(int seconds, EventHandler handler);
  std::optional<SourceRef> AddTimerMs(int ms, EventHandler handler);

  // Add a new file descriptor to poll. The file descriptor is not owned by the EvLoop instance.
  std::optional<SourceRef> AddFd(int fd, Events events, IoEventHandler handler);

  // Add a new file descriptor to poll. The file descriptor will be owned by the EvLoop instance.
  std::optional<SourceRef> TakeFd(unique_fd fd, Events events, IoEventHandler handler);

  enum class WaitResult { kReady, kIdle, kError };
  enum class DispatchResult { kContinue, kExit, kError };

  WaitResult Wait();
  DispatchResult Dispatch();

  enum class ExitStatus { kSuccess, kFailure };

  bool Exit(ExitStatus status);
  ExitStatus exit_status() const;

 private:
  EvLoop(sd_event* event, unique_fd notify_defer_fd);

  struct SdEventDeleter {
    void operator()(sd_event* event) { sd_event_unref(event); }
  };

  std::unique_ptr<sd_event, SdEventDeleter> event_;

  // If the loop's epoll fd is currently being polled on by Wait, sd_event_add_defer will not
  // awaken it, as it does not interact with the epoll fd. Therefore, we create an eventfd that is
  // polled on along with the epoll fd, that way it'll cause Wait to return.
  // Note that, tasks and triggers could just use eventfds instead, but this is a bit more
  // efficient since it's not creating and polling on as many fds.
  unique_fd notify_defer_fd_;

  template <typename Handler>
  SourceRef SourceSetup(sd_event_source* source, Handler handler);

  template <typename Handler, typename... Args>
  static int GenericHandler(sd_event_source* source, void* data, Args&&... args);

  static int HandleIoEvent(sd_event_source* source, int fd, std::uint32_t revents, void* data);
  static int HandleTimeEvent(sd_event_source* source, std::uint64_t us, void* data);
};

}  // namespace zypak
