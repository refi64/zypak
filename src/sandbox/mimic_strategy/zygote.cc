// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mimic_strategy/zygote.h"

#include <array>

#include <nickle.h>

#include "base/base.h"
#include "base/debug.h"
#include "base/evloop.h"
#include "base/socket.h"
#include "base/unique_fd.h"
#include "sandbox/mimic_strategy/command.h"
#include "sandbox/mimic_strategy/fork.h"
#include "sandbox/mimic_strategy/reap.h"
#include "sandbox/mimic_strategy/status.h"

namespace zypak::sandbox::mimic_strategy {

// static
std::optional<MimicZygoteRunner> MimicZygoteRunner::Create() {
  auto ev = EvLoop::Create();
  if (!ev) {
    return {};
  }

  return MimicZygoteRunner(std::move(*ev));
}

void MimicZygoteRunner::HandleMessage(EvLoop::SourceRef source) {
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
      ev_.Exit(EvLoop::ExitStatus::kSuccess);
    } else {
      ev_.Exit(EvLoop::ExitStatus::kFailure);
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
    HandleReap(&ev_, &children_, &reader);
    break;
  case ZygoteCommand::kTerminationStatus:
    HandleTerminationStatusRequest(&children_, &reader);
    break;
  case ZygoteCommand::kSandboxStatus:
    HandleSandboxStatusRequest();
    break;
  case ZygoteCommand::kForkRealPID:
    Log() << "Got kForkRealPID in main command runner";
    ev_.Exit(EvLoop::ExitStatus::kFailure);
    break;
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

  {
    // Make sure to drop the reference, that way if the host drops off, an error will occur and the
    // last reference will be dropped, leading to the destroy handler being called.
    std::optional<EvLoop::SourceRef> zygote_host_ref =
        ev_.AddFd(kZygoteHostFd, EvLoop::Events::Status::kRead,
                  std::bind(&MimicZygoteRunner::HandleMessage, this, std::placeholders::_1));
    if (!zygote_host_ref) {
      Log() << "Failed to add zygote FD to event loop";
      return false;
    }

    Debug() << "Going to run main loop";
    zygote_host_ref->AddDestroyHandler([this]() {
      Log() << "Host is gone, preparing to exit...";
      ev_.Exit(EvLoop::ExitStatus::kSuccess);
    });
  }

  for (;;) {
    switch (ev_.Wait()) {
    case EvLoop::WaitResult::kError:
      Log() << "Wait error, aborting mimic Zygote...";
      return false;
    case EvLoop::WaitResult::kIdle:
      continue;
    case EvLoop::WaitResult::kReady:
      break;
    }

    switch (ev_.Dispatch()) {
    case EvLoop::DispatchResult::kError:
      Log() << "Dispatch error, aborting mimic Zygote...";
      return false;
    case EvLoop::DispatchResult::kExit:
      Log() << "Quitting Zygote...";
      return ev_.exit_status() == EvLoop::ExitStatus::kSuccess;
    case EvLoop::DispatchResult::kContinue:
      continue;
    }
  }
}

}  // namespace zypak::sandbox::mimic_strategy
