// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/epoll.h"

#include <sys/epoll.h>
#include <sys/timerfd.h>

#include <array>
#include <cstring>

#include "base/base.h"
#include "base/debug.h"

/*static*/
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

std::optional<Epoll::Id> Epoll::AddTimer(int seconds, Epoll::Handler func) {
  unique_fd tfd(timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC));
  if (tfd.invalid()) {
    Errno() << "Failed to create timer";
    return {};
  }

  struct itimerspec ts;
  std::memset(reinterpret_cast<void*>(&ts), 0, sizeof(ts));
  ts.it_value.tv_sec = seconds;

  if (timerfd_settime(tfd.get(), 0, &ts, nullptr) == -1) {
    Errno() << "Failed to set timer expiration";
    return {};
  }

  return TakeFd(std::move(tfd), func);
}

std::optional<Epoll::Id> Epoll::AddFd(int fd, Epoll::Handler func) {
  struct epoll_event evt;
  evt.events = EPOLLIN;
  evt.data.fd = fd;

  if (epoll_ctl(epfd_.get(), EPOLL_CTL_ADD, fd, &evt) == -1) {
    Errno() << "Failed to add " << fd << " to epoll";
    return {};
  }

  fd_data_.emplace(fd, FdData{false, std::move(func)});
  return Id{fd};
}

std::optional<Epoll::Id> Epoll::TakeFd(unique_fd fd, Epoll::Handler func) {
  if (auto id = AddFd(fd.get(), func)) {
    fd_data_[fd.get()].owned = true;
    return id;
  }

  return {};
}

bool Epoll::Remove(Epoll::Id id) {
  auto it = fd_data_.find(id.fd);
  if (it == fd_data_.end()) {
    return false;
  }

  if (epoll_ctl(epfd_.get(), EPOLL_CTL_DEL, id.fd, nullptr) == -1) {
    Errno() << "Failed to delete " << id.fd << " from epoll";
    return false;
  }

  if (it->second.owned) {
    close(id.fd);
  }

  fd_data_.erase(it);
  return true;
}

bool Epoll::RunIteration() {
  constexpr int kMaxEvents = 64;
  std::array<struct epoll_event, kMaxEvents> events;

  int ready = HANDLE_EINTR(epoll_wait(epfd_.get(), events.data(), kMaxEvents, -1));
  if (ready == -1) {
    Errno() << "Failed to wait for events from epoll";
    return false;
  }

  for (int i = 0; i < ready; i++) {
    if (events[i].events & EPOLLIN) {
      if (!fd_data_[events[i].data.fd].func(this)) {
        return false;
      }
    } else if (events[i].events & (EPOLLERR | EPOLLHUP)) {
      Log() << "Error occurred polling on " << events[i].data.fd;
      return false;
    }
  }

  return true;
}
