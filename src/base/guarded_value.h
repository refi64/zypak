// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Guarded values are a mechanism that allows you to protect a value behind a
// std::mutex, while also potentially emitting notifications to a paired
// std::condition_variable.
// Acquiring a guarded value's content involves the construction of a guard, whose
// lifetime determines when the mutex is release (i.e. the mutex is release on guard
// destructor).

#pragma once

#include <condition_variable>
#include <mutex>

#include "base/base.h"

namespace zypak {

// The listeners who should be notified when a guard is release.
enum class GuardReleaseNotify { kNone, kOne, kAll };

namespace guarded_value_internal {

template <typename T, typename Mutex>
class NonNotifyingGuardedValue;

template <typename T, typename Mutex>
struct BasicGuard {
 public:
  BasicGuard(const BasicGuard& other) = delete;
  BasicGuard(BasicGuard&& other) = delete;

  // Various accessors for the underlying value.

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

  // Returns an unsafe pointer to the guarded value. The mutex is *not* acquired in this case.
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

  // Acquires the given value, holding the mutex until the guard's destruction.
  BasicGuard<T, Mutex> Acquire() { return BasicGuard(&this->mutex_, &this->value_); }
};

}  // namespace guarded_value_internal

template <typename T>
class NotifyingGuardedValue;

// A guard that optionally notifies listeners on destruction.
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

  // Notifies one listener.
  void NotifyOne() { condition_->notify_one(); }
  // Notifies all listeners.
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

// A guarded value that can be acquired when a condition is fulfilled, and the condition
// will be checked on every notify.
template <typename T>
class NotifyingGuardedValue : public guarded_value_internal::BasicGuardedValue<T, std::mutex> {
 public:
  using guarded_value_internal::BasicGuardedValue<T, std::mutex>::BasicGuardedValue;

  // Acquires the guarded value; upon description, the guard will notify the given listeners.
  NotifyingGuard<T> Acquire(GuardReleaseNotify notify) {
    return NotifyingGuard(&this->mutex_, &this->value_, &condition_, notify);
  }

  // Acquires the guarded value when the given predicate returns true; upon description, the guard
  // will notify the given listeners. Note that the predicate is only checked when a notify event
  // is set out, i.e. either NotifyOne/All is called, or a guard acquired with a notify value
  // is destroyed.
  template <typename Pred>
  NotifyingGuard<T> AcquireWhen(Pred&& pred,
                                GuardReleaseNotify notify = GuardReleaseNotify::kNone) {
    return NotifyingGuard(&this->mutex_, &this->value_, &condition_, notify,
                          std::forward<Pred>(pred));
  }

 private:
  std::condition_variable condition_;
};

// A basic guard, wrapping a std::mutex. See BasicGuard for relevant documentation.
template <typename T>
using Guard = guarded_value_internal::BasicGuard<T, std::mutex>;

// A basic guarded value, wrapping a std::mutex. See NonNotifyingGuardedValue for relevant
// documentation.
template <typename T>
using GuardedValue = guarded_value_internal::NonNotifyingGuardedValue<T, std::mutex>;

// A guard for a value that can be acquired recursively, wrapping a std::recursive_Mutex.
// See BasicGuard for relevant documentation.
template <typename T>
using RecursiveGuard = guarded_value_internal::BasicGuard<T, std::recursive_mutex>;

// A guarded value that can be acquired recursively, wrapping a std::recursive_Mutex.
// See NonNotifyingGuardedValue for relevant documentation.
template <typename T>
using RecursiveGuardedValue =
    guarded_value_internal::NonNotifyingGuardedValue<T, std::recursive_mutex>;

}  // namespace zypak
