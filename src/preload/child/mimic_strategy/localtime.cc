// Copyright 2020 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include <array>
#include <cstddef>
#include <unordered_set>

#include <nickle.h>

#include "base/debug.h"
#include "base/guarded_value.h"
#include "base/socket.h"
#include "preload/child/mimic_strategy/fd_storage.h"
#include "preload/declare_override.h"

using namespace zypak;
using namespace zypak::preload;

// The Zygote intercepts localtime calls and forwards them to an external
// service, the sandbox service. Since we're mimicking the Zygote, we also need
// to forward these calls.
// Based on sandbox/linux/services/libc_interceptor.cc

namespace {

constexpr int kSandboxServiceLocaltimeMethod = 32;
constexpr int kLocaltimeReplyMaxLength = 512;

GuardedValue<std::unordered_set<std::string>> g_timezones;

std::optional<std::vector<std::byte>>
SendSandboxRequestAndReadReply(const std::vector<std::byte>& request) {
  // In order to keep the clients separate, the sandbox service expects a request sent with a
  // separate fd to reply over.

  auto sockets = Socket::OpenSocketPair();
  if (!sockets) {
    Log() << "Failed to open sandbox request/reply sockets";
    return {};
  }

  auto [local_end, remote_end] = std::move(*sockets);

  std::vector<int> write_fds{remote_end.get()};
  Socket::WriteOptions write_options;
  write_options.fds = &write_fds;
  if (!Socket::Write(FdStorage::instance()->sandbox_service_fd().get(), request, write_options)) {
    Errno() << "Failed to send sandbox request";
    return {};
  }

  remote_end.reset();

  std::vector<std::byte> reply;
  reply.resize(kLocaltimeReplyMaxLength);

  ssize_t bytes_read = Socket::Read(local_end.get(), &reply);
  if (bytes_read == -1) {
    Errno() << "Failed to read sandbox reply";
    return {};
  }

  reply.resize(bytes_read);

  return reply;
}

void GetTimeFromSandboxService(time_t time, struct tm* result, std::string* timezone_storage) {
  memset(result, 0, sizeof(*result));

  std::vector<std::byte> request_data;
  nickle::buffers::ContainerBuffer request_buffer(&request_data);
  nickle::Writer writer(&request_buffer);

  ZYPAK_ASSERT(writer.Write<nickle::codecs::Int>(kSandboxServiceLocaltimeMethod));
  ZYPAK_ASSERT(writer.Write<nickle::codecs::StringView>(
      std::string_view(reinterpret_cast<char*>(&time), sizeof(time))));

  auto reply_data = SendSandboxRequestAndReadReply(request_data);
  if (!reply_data) {
    return;
  }

  nickle::buffers::ReadOnlyContainerBuffer reply_buffer(*reply_data);
  nickle::Reader reader(&reply_buffer);

  int gmtoff = 0;
  if (!reader.Read<nickle::codecs::Int>(&result->tm_sec) ||
      !reader.Read<nickle::codecs::Int>(&result->tm_min) ||
      !reader.Read<nickle::codecs::Int>(&result->tm_hour) ||
      !reader.Read<nickle::codecs::Int>(&result->tm_mday) ||
      !reader.Read<nickle::codecs::Int>(&result->tm_mon) ||
      !reader.Read<nickle::codecs::Int>(&result->tm_year) ||
      !reader.Read<nickle::codecs::Int>(&result->tm_wday) ||
      !reader.Read<nickle::codecs::Int>(&result->tm_yday) ||
      !reader.Read<nickle::codecs::Int>(&result->tm_isdst) ||
      !reader.Read<nickle::codecs::Int>(&gmtoff)) {
    Log() << "Failed to read time data from sandbox service reply";
    return;
  }

  result->tm_gmtoff = gmtoff;

  std::string timezone;
  if (!reader.Read<nickle::codecs::String>(&timezone)) {
    Log() << "Failed to read timezone from sandbox service reply";
    return;
  }

  if (timezone_storage != nullptr) {
    *timezone_storage = timezone;
    result->tm_zone = timezone_storage->c_str();
  } else {
    auto timezones = g_timezones.Acquire();
    result->tm_zone = timezones->insert(timezone).first->c_str();
  }
}

}  // namespace

DECLARE_OVERRIDE(struct tm*, localtime, const time_t* timep) {
  static struct tm result;
  static std::string timezone_storage;

  GetTimeFromSandboxService(*timep, &result, &timezone_storage);
  return &result;
}

DECLARE_OVERRIDE(struct tm*, localtime_r, const time_t* timep, struct tm* result) {
  GetTimeFromSandboxService(*timep, result, nullptr);
  return result;
}

DECLARE_OVERRIDE(struct tm*, localtime64, const time_t* timep) { return localtime(timep); }

DECLARE_OVERRIDE(struct tm*, localtime64_r, const time_t* timep, struct tm* result) {
  return localtime_r(timep, result);
}
