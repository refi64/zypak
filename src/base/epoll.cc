// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/epoll.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>

#include "base/base.h"
#include "base/debug.h"

namespace {

constexpr int kMillisecondsPerSecond = 1000;
constexpr int kNanosecondsPerMillisecond = 1000000;

}  // namespace

namespace zypak {

// static
std::optional<Epoll> Epoll::Create() {
  unique_fd epfd(epoll_create1(EPOLL_CLOEXEC));
  if (epfd.invalid()) {
    Errno() << "Failed to create epoll loop";
    return {};
  }

  return Epoll(std::move(epfd));
}

Epoll::Epoll(unique_fd epfd) : epfd_(std::move(epfd)) {}

Epoll::~Epoll() {
  for (const auto& [fd, data] : fd_data_) {
    if (data.owned) {
      close(fd);
    }
  }
}

bool Epoll::Trigger::Add(std::uint64_t count /*= 1*/) {
  ZYPAK_ASSERT(count > 0);

  static_assert(sizeof(count) == 8);

  ssize_t bytes_written =
      HANDLE_EINTR(write(id_.fd_, reinterpret_cast<void*>(&count), sizeof(count)));
  // XXX: Not sure if < sizeof(count) is possible for eventfd?
  ZYPAK_ASSERT(bytes_written == sizeof(count) || bytes_written == -1);

  if (bytes_written == -1) {
    Errno() << "Failed to write to trigger";
    return false;
  }

  return true;
}

std::optional<std::uint64_t> Epoll::TriggerReceiver::GetAndClear() {
  std::uint64_t value;

  ssize_t bytes_read = HANDLE_EINTR(read(id_.fd_, reinterpret_cast<void*>(&value), sizeof(value)));
  // XXX: See comment above in Trigger::Add.
  ZYPAK_ASSERT(bytes_read == sizeof(value) || bytes_read == -1);

  if (bytes_read == -1) {
    Errno() << "Failed to get trigger state from " << id_.fd_;
    return {};
  }

  return value;
}

std::optional<Epoll::TriggerReceiver> Epoll::AddTrigger(std::uint64_t initial,
                                                        TriggerHandler func) {
  unique_fd efd(eventfd(initial, 0));
  if (efd.invalid()) {
    Errno() << "Failed to create eventfd";
    return {};
  }

  auto opt_id = TakeFd(std::move(efd), Handler());
  if (!opt_id) {
    return {};
  }

  TriggerReceiver receiver(*opt_id);

  fd_data_[opt_id->fd_].func = [func2 = std::move(func), receiver](Epoll* ep, Events events) {
    return func2(ep, receiver);
  };

  return receiver;
}

std::optional<Epoll::Id> Epoll::AddTask(Handler func) {
  // Start with a value of 1 so it's immediately ready.
  auto opt_receiver = AddTrigger(
      1, [func2 = std::move(func)](Epoll* ep, TriggerReceiver receiver) { return func2(ep); });
  if (!opt_receiver) {
    return {};
  }

  fd_data_[opt_receiver->id().fd_].once = true;

  Debug() << "Task fd is " << opt_receiver->id().fd_;
  return opt_receiver->id();
}

std::optional<Epoll::Id> Epoll::AddTimerSec(int seconds, Epoll::Handler func,
                                            bool repeat /*= false*/) {
  return AddTimerMs(seconds * kMillisecondsPerSecond, func);
}

std::optional<Epoll::Id> Epoll::AddTimerMs(int ms, Epoll::Handler func, bool repeat /*= false*/) {
  unique_fd tfd(timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC));
  if (tfd.invalid()) {
    Errno() << "Failed to create timer";
    return {};
  }

  struct itimerspec ts;
  std::memset(reinterpret_cast<void*>(&ts), 0, sizeof(ts));
  ts.it_value.tv_sec = ms / kMillisecondsPerSecond;
  ts.it_value.tv_nsec = (ms % kMillisecondsPerSecond) * kNanosecondsPerMillisecond;

  if (repeat) {
    ts.it_interval = ts.it_value;
  }

  if (timerfd_settime(tfd.get(), 0, &ts, nullptr) == -1) {
    Errno() << "Failed to set timer expiration";
    return {};
  }

  auto id = TakeFd(std::move(tfd), func);
  if (!id) {
    return {};
  }

  if (!repeat) {
    fd_data_[id->fd_].once = true;
  }

  return id;
}

std::optional<Epoll::Id> Epoll::AddFd(int fd, Events events, Epoll::EventsHandler func) {
  ZYPAK_ASSERT(!events.empty(), << "Missing events for fd" << fd);
  ZYPAK_ASSERT(func, << "Missing handler for fd " << fd);

  struct epoll_event evt;
  evt.data.fd = fd;
  evt.events = 0;

  if (events.contains(Events::Status::kRead)) {
    evt.events |= EPOLLIN;
  }
  if (events.contains(Events::Status::kWrite)) {
    evt.events |= EPOLLOUT;
  }

  if (epoll_ctl(epfd_.get(), EPOLL_CTL_ADD, fd, &evt) == -1) {
    Errno() << "Failed to add " << fd << " to epoll";
    return {};
  }

  fd_data_.emplace(fd, FdData{false, std::move(func)});
  return Id{fd};
}

std::optional<Epoll::Id> Epoll::TakeFd(unique_fd fd, Events events, Epoll::EventsHandler func) {
  if (auto id = AddFd(fd.get(), events, func)) {
    fd_data_[fd.release()].owned = true;
    return id;
  }

  return {};
}

bool Epoll::Remove(Epoll::Id id) {
  Debug() << "Remove fd " << id.fd_;

  auto it = fd_data_.find(id.fd_);
  if (it == fd_data_.end()) {
    Log() << "Cannot remove non-existent fd " << id.fd_;
    return false;
  }

  if (epoll_ctl(epfd_.get(), EPOLL_CTL_DEL, id.fd_, nullptr) == -1) {
    Errno() << "Failed to delete " << id.fd_ << " from epoll";
    return false;
  }

  if (it->second.owned) {
    close(id.fd_);
  }

  fd_data_.erase(it);
  return true;
}

bool Epoll::Wait(EventSet* events) {
  events->Clear();

  int ready = HANDLE_EINTR(epoll_wait(epfd_.get(), events->data(), EventSet::kMaxEvents, -1));
  if (ready == -1) {
    Errno() << "Failed to wait for events from epoll";
    return false;
  }

  events->count_ = ready;
  return true;
}

bool Epoll::Dispatch(const EventSet& events) {
  for (int i = 0; i < events.count(); i++) {
    auto event = events.data()[i];
    const FdData& data = fd_data_[event.data.fd];

    Debug() << "epoll got info for fd " << event.data.fd;

    if (event.events & (EPOLLIN | EPOLLOUT)) {
      Events events = Events::Status::kNone;

      if (event.events & EPOLLIN) {
        events |= Events::Status::kRead;
      }
      if (event.events & EPOLLOUT) {
        events |= Events::Status::kWrite;
      }

      ZYPAK_ASSERT(data.func, << "Missing handler for fd " << event.data.fd);

      if (!data.func(this, events)) {
        return false;
      }

      if (data.once && !Remove(event.data.fd)) {
        Log() << "Could not remove run-once event " << event.data.fd;
      }
    } else if (event.events & (EPOLLERR | EPOLLHUP)) {
      Log() << "Error occurred polling on " << event.data.fd;
      return false;
    }
  }

  return true;
}

}  // namespace zypak
