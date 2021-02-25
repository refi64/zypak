// Copyright 2020 Endless Mobile, Inc.
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

// A C++-friendly wrapper over sd-event. This is *not* thread-safe; in a multi-threaded context,
// it should be guarded separately.
class EvLoop {
 public:
  EvLoop(const EvLoop& other) = delete;
  EvLoop(EvLoop&& other) = default;
  ~EvLoop() {}

  // Creates a new event loop instance, returning an empty optional if creation fails.
  static std::optional<EvLoop> Create();

  // A set of Events that can occur / have occurred on an event source.
  class Events {
   public:
    enum class Status {
      kNone = 0,
      kRead = 1 << 0,
      kWrite = 1 << 1,
      kReadWrite = (1 << 0) | (1 << 1),
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

  class SourceRef;

  using EventHandler = std::function<void(SourceRef)>;
  using IoEventHandler = std::function<void(SourceRef, Events)>;
  using DestroyHandler = std::function<void()>;

  // A reference to a single source being tracked by the event loop. Unlike vanilla sd-event, *all*
  // SourceRefs are "floating" and partly owned by the event loop. Thus, if all SourceRefs pointing
  // to a source die, the source is *not* removed from the loop. Disable should be called instead.
  class SourceRef {
   public:
    SourceRef(const SourceRef& other)
        : source_(sd_event_source_ref(other.source_)), on_destroy_(other.on_destroy_) {}
    SourceRef(SourceRef&& other) : source_(other.source_), on_destroy_(other.on_destroy_) {
      other.source_ = nullptr;
    }
    ~SourceRef() { sd_event_source_unref(source_); }

    friend void swap(SourceRef& a, SourceRef& b) {
      using std::swap;
      swap(a.source_, b.source_);
    }

    SourceRef& operator=(SourceRef other) {
      swap(*this, other);
      return *this;
    }

    enum State {
      // The source is active until removed.
      kActiveForever,
      // The source is only active once, then automatically disabled.
      kActiveOnce,
      // The source is disabled.
      kDisabled
    };
    State state() const;

    void Disable();

    void AddDestroyHandler(DestroyHandler handler);

   private:
    explicit SourceRef(sd_event_source* source, std::vector<DestroyHandler>* on_destroy)
        : source_(source), on_destroy_(on_destroy) {}

    sd_event_source* source_;
    std::vector<DestroyHandler>* on_destroy_;

    friend class EvLoop;
    friend class TriggerSourceRef;
  };

  // A wrapper for a SourceRef that can be activated repeatedly. Once "triggered", the source will
  // run on the next event loop iteration, then be automatically disabled (i.e. on trigger, the
  // state becomes kActiveOnce).
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

  // Adds a function that should run on the next event loop iteration, in the event loop's thread
  // and environment.
  std::optional<SourceRef> AddTask(EventHandler handler);

  // Adds a function that will be run when the returned source is triggered.
  std::optional<TriggerSourceRef> AddTrigger(EventHandler handler);

  // Add a new timer that fires after the given # of seconds / milliseconds.
  std::optional<SourceRef> AddTimerSec(int seconds, EventHandler handler);
  std::optional<SourceRef> AddTimerMs(int ms, EventHandler handler);

  // Add a new file descriptor to poll. The file descriptor is not owned by the EvLoop instance.
  std::optional<SourceRef> AddFd(int fd, Events events, IoEventHandler handler);

  // Add a new file descriptor to poll. The file descriptor will be owned by the EvLoop instance.
  std::optional<SourceRef> TakeFd(unique_fd fd, Events events, IoEventHandler handler);

  // The current state of the event loop.
  enum class WaitResult {
    // The event loop is ready to dispatch events, call Dispatch next.
    kReady,
    // The event loop has no events ready, try Wait again later.
    kIdle,
    // The event loop is in an error state.
    kError,
  };
  // Waits on the event loop, returning with a state that is either ready, idle, or failed.
  WaitResult Wait();

  // The result of a dispatch operation.
  enum class DispatchResult {
    // The dispatch was a success, and the event loop is ready to wait again.
    kContinue,
    // The dispatch was a success, and the event loop was requested to exit. After this,
    // Wait/Dispatch should not be called any longer.
    kExit,
    // The dispatch encountered an error.
    kError,
  };
  // Dispatches any ready events; must be called after Wait returns kReady.
  DispatchResult Dispatch();

  enum class ExitStatus { kSuccess, kFailure };

  // Exits the event loop, setting its result to the given status.
  bool Exit(ExitStatus status);
  // Returns the status that was passed to Exit when the event loop exited. It is *not* allowed to
  // call this before Exit.
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

  void ClearNotifyDeferFd();

  std::optional<EvLoop::SourceRef> AddTaskNoNotify(EvLoop::EventHandler handler);

  template <typename Handler>
  SourceRef SourceSetup(sd_event_source* source, Handler handler);

  template <typename Handler, typename... Args>
  static int GenericHandler(sd_event_source* source, void* data, Args&&... args);

  static int HandleIoEvent(sd_event_source* source, int fd, std::uint32_t revents, void* data);
  static int HandleTimeEvent(sd_event_source* source, std::uint64_t us, void* data);
};

}  // namespace zypak
