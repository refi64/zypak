// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <condition_variable>
#include <mutex>

#include "base/base.h"

namespace zypak {

enum class GuardReleaseNotify { kNone, kOne, kAll };

namespace guarded_value_internal {

template <typename T, typename Mutex>
class NonNotifyingGuardedValue;

template <typename T, typename Mutex>
struct BasicGuard {
 public:
  BasicGuard(const BasicGuard& other) = delete;
  BasicGuard(BasicGuard&& other) = delete;

  const T& operator*() const { return *value_; }
  T& operator*() { return *value_; }

  T* operator->() { return value_; }

  T* raw() { return value_; }

 protected:
  friend class NonNotifyingGuardedValue<T, Mutex>;

  BasicGuard(Mutex* mutex, T* value) : guard_(*mutex), value_(value) {}

  std::unique_lock<Mutex> guard_;
  T* value_;
};

template <typename T, typename Mutex>
class BasicGuardedValue {
 public:
  BasicGuardedValue() : value_() {}
  BasicGuardedValue(T value) : value_(std::move(value)) {}

  BasicGuardedValue(const BasicGuardedValue& other) = delete;
  BasicGuardedValue(BasicGuardedValue&& other) = delete;

  T* unsafe() { return &value_; }

 protected:
  Mutex mutex_;
  std::condition_variable condition_;
  T value_;
};

template <typename T, typename Mutex>
class NonNotifyingGuardedValue : public BasicGuardedValue<T, Mutex> {
 public:
  using BasicGuardedValue<T, Mutex>::BasicGuardedValue;

  BasicGuard<T, Mutex> Acquire() { return BasicGuard(&this->mutex_, &this->value_); }
};

}  // namespace guarded_value_internal

template <typename T>
class NotifyingGuardedValue;

template <typename T>
class NotifyingGuard : public guarded_value_internal::BasicGuard<T, std::mutex> {
 public:
  NotifyingGuard(const NotifyingGuard& other) = delete;
  NotifyingGuard(NotifyingGuard&& other) = delete;

  ~NotifyingGuard() {
    this->guard_.unlock();

    switch (release_notify_) {
    case GuardReleaseNotify::kNone:
      break;
    case GuardReleaseNotify::kOne:
      NotifyOne();
      break;
    case GuardReleaseNotify::kAll:
      NotifyAll();
      break;
    }
  }

  void NotifyOne() { condition_->notify_one(); }
  void NotifyAll() { condition_->notify_all(); }

 private:
  friend class NotifyingGuardedValue<T>;

  NotifyingGuard(std::mutex* mutex, T* value, std::condition_variable* condition,
                 GuardReleaseNotify notify)
      : guarded_value_internal::BasicGuard<T, std::mutex>(mutex, value), condition_(condition),
        release_notify_(notify) {}

  template <typename Pred>
  NotifyingGuard(std::mutex* mutex, T* value, std::condition_variable* condition,
                 GuardReleaseNotify notify, Pred pred)
      : NotifyingGuard(mutex, value, condition, notify) {
    condition_->wait(this->guard_, [&]() { return pred(value); });
  }

  std::condition_variable* condition_;
  GuardReleaseNotify release_notify_;
};

template <typename T>
class NotifyingGuardedValue : public guarded_value_internal::BasicGuardedValue<T, std::mutex> {
 public:
  using guarded_value_internal::BasicGuardedValue<T, std::mutex>::BasicGuardedValue;

  NotifyingGuard<T> Acquire(GuardReleaseNotify notify) {
    return NotifyingGuard(&this->mutex_, &this->value_, &condition_, notify);
  }

  template <typename Pred>
  NotifyingGuard<T> AcquireWhen(Pred&& pred,
                                GuardReleaseNotify notify = GuardReleaseNotify::kNone) {
    return NotifyingGuard(&this->mutex_, &this->value_, &condition_, notify,
                          std::forward<Pred>(pred));
  }

 private:
  std::condition_variable condition_;
};

template <typename T>
using Guard = guarded_value_internal::BasicGuard<T, std::mutex>;

template <typename T>
using GuardedValue = guarded_value_internal::NonNotifyingGuardedValue<T, std::mutex>;

template <typename T>
using RecursiveGuard = guarded_value_internal::BasicGuard<T, std::recursive_mutex>;

template <typename T>
using RecursiveGuardedValue =
    guarded_value_internal::NonNotifyingGuardedValue<T, std::recursive_mutex>;

}  // namespace zypak
