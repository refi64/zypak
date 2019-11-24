// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base.h"
#include "base/debug.h"
#include "base/unique_fd.h"

#include "../epoll.h"
#include "../socket.h"
#include "command.h"
#include "fork.h"
#include "reap.h"
#include "status.h"
#include "zygote.h"

#include <array>
#include <set>

#include <nickle.h>

static bool HandleZygoteMessage(std::set<pid_t>* children, Epoll* ep) {
  static std::array<std::byte, kZygoteMaxMessageLength> buffer;
  std::vector<unique_fd> fds;

  ssize_t len = Socket::Read(kZygoteHostFd, &buffer, &fds);
  if (len <= 0) {
    if (len == 0) {
      Log() << "No data could be read (host died?)";
    } else {
      Errno() << "Failed to read message from host";
    }

    if (len == 0 || errno == ECONNRESET) {
      return false;
    }

    return true;
  }

  nickle::buffers::ReadOnlyContainerBuffer nbuf(buffer);
  nickle::Reader reader(&nbuf);

  ZygoteCommand command;
  if (!reader.Read<ZygoteCommandCodec>(&command)) {
    Log() << "Failed to parse host action";
    return true;
  }

  if (!fds.empty() && command != ZygoteCommand::kFork) {
    Log() << "Unexpected FDs for non-fork command";
    return true;
  }

  switch (command) {
  case ZygoteCommand::kFork:
    if (auto child = HandleFork(&reader, std::move(fds))) {
      if (auto [_, inserted] = children->insert(*child); !inserted) {
        Log() << "Already tracking PID " << *child;
      }
    }
    break;
  case ZygoteCommand::kReap:
    HandleReap(ep, children, &reader);
    break;
  case ZygoteCommand::kTerminationStatus:
    HandleTerminationStatusRequest(children, &reader);
    break;
  case ZygoteCommand::kSandboxStatus:
    HandleSandboxStatusRequest();
    break;
  case ZygoteCommand::kForkRealPID:
    Log() << "Got kForkRealPID in main command runner";
    return false;
  }

  return true;
}

bool RunZygote() {
  auto opt_ep = Epoll::Create();
  if (!opt_ep) {
    return false;
  }

  auto ep = std::move(*opt_ep);

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

  std::set<int> children;

  if (!ep.AddFd(kZygoteHostFd,
                std::bind(HandleZygoteMessage, &children, std::placeholders::_1))) {
    return false;
  }

  while (ep.RunIteration());
  Log() << "Quitting Zygote...";
  return false;
}
