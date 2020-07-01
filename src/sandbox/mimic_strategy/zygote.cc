// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mimic_strategy/zygote.h"

#include <array>

#include <nickle.h>

#include "base/base.h"
#include "base/debug.h"
#include "base/epoll.h"
#include "base/socket.h"
#include "base/unique_fd.h"
#include "sandbox/mimic_strategy/command.h"
#include "sandbox/mimic_strategy/fork.h"
#include "sandbox/mimic_strategy/reap.h"
#include "sandbox/mimic_strategy/status.h"

namespace zypak::sandbox::mimic_strategy {

// static
std::optional<MimicZygoteRunner> MimicZygoteRunner::Create() {
  auto epoll = Epoll::Create();
  if (!epoll) {
    return {};
  }

  return MimicZygoteRunner(std::move(*epoll));
}

void MimicZygoteRunner::HandleMessage(Epoll::SourceRef source, Epoll::Events events) {
  static std::array<std::byte, kZygoteMaxMessageLength> buffer;
  std::vector<unique_fd> fds;

  Socket::ReadOptions options;
  options.fds = &fds;
  ssize_t len = Socket::Read(kZygoteHostFd, &buffer, options);
  if (len <= 0) {
    if (len == 0) {
      Log() << "No data could be read (host died?)";
    } else {
      Errno() << "Failed to read message from host";
    }

    if (len == 0 || errno == ECONNRESET) {
      epoll_.Exit(Epoll::ExitStatus::kSuccess);
    } else {
      epoll_.Exit(Epoll::ExitStatus::kFailure);
    }

    return;
  }

  nickle::buffers::ReadOnlyContainerBuffer nbuf(buffer);
  nickle::Reader reader(&nbuf);

  ZygoteCommand command;
  if (!reader.Read<ZygoteCommandCodec>(&command)) {
    Log() << "Failed to parse host action";
    return;
  }

  if (!fds.empty() && command != ZygoteCommand::kFork) {
    Log() << "Unexpected FDs for non-fork command";
    return;
  }

  switch (command) {
  case ZygoteCommand::kFork:
    if (auto child = HandleFork(&reader, std::move(fds))) {
      if (auto [_, inserted] = children_.insert(*child); !inserted) {
        Log() << "Already tracking PID " << *child;
      }
    }
    break;
  case ZygoteCommand::kReap:
    HandleReap(&epoll_, &children_, &reader);
    break;
  case ZygoteCommand::kTerminationStatus:
    HandleTerminationStatusRequest(&children_, &reader);
    break;
  case ZygoteCommand::kSandboxStatus:
    HandleSandboxStatusRequest();
    break;
  case ZygoteCommand::kForkRealPID:
    Log() << "Got kForkRealPID in main command runner";
    epoll_.Exit(Epoll::ExitStatus::kFailure);
    return;
  }
}

bool MimicZygoteRunner::Run() {
  constexpr std::string_view kBootMessage = "ZYGOTE_BOOT";
  constexpr std::string_view kHelloMessage = "ZYGOTE_OK";

  if (!Socket::Write(kZygoteHostFd, kBootMessage)) {
    Errno() << "Failed to set boot message to Zygote";
    return false;
  }
  if (!Socket::Write(kZygoteHostFd, kHelloMessage)) {
    Errno() << "Failed to set hello message to Zygote";
    return false;
  }

  if (!epoll_.AddFd(kZygoteHostFd, Epoll::Events::Status::kRead,
                    std::bind(&MimicZygoteRunner::HandleMessage, this, std::placeholders::_1,
                              std::placeholders::_2))) {
    return false;
  }

  for (;;) {
    switch (epoll_.Wait()) {
    case Epoll::WaitResult::kError:
      Log() << "Wait error, aborting mimic Zygote...";
      return false;
    case Epoll::WaitResult::kIdle:
      continue;
    case Epoll::WaitResult::kReady:
      break;
    }

    switch (epoll_.Dispatch()) {
    case Epoll::DispatchResult::kError:
      Log() << "Dispatch error, aborting mimic Zygote...";
      return false;
    case Epoll::DispatchResult::kExit:
      break;
    case Epoll::DispatchResult::kContinue:
      continue;
    }
  }

  Log() << "Quitting Zygote...";
  return epoll_.exit_status() == Epoll::ExitStatus::kSuccess;
}

}  // namespace zypak::sandbox::mimic_strategy
