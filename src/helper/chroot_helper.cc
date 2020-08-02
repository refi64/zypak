// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "helper/chroot_helper.h"

#include <sys/prctl.h>
#include <sys/signal.h>

#include <cstdlib>

#include "base/debug.h"
#include "base/socket.h"

namespace zypak {

namespace {

bool StubSandboxChrootHelper(unique_fd fd) {
  if (prctl(PR_SET_PDEATHSIG, SIGKILL, 0, 0, 0) == -1) {
    Errno() << "Warning: Failed to prctl(DEATHSIG)";
  }

  Debug() << "Waiting for chroot request";

  std::array<std::byte, 1> msg;
  if (ssize_t bytes_read = Socket::Read(fd.get(), &msg); bytes_read == -1) {
    Errno() << "Failed to read from chroot message pipe";
    return false;
  }

  if (msg[0] == static_cast<std::byte>(0)) {
    Log() << "Chroot pipe is empty";
    return false;
  } else if (msg[0] != static_cast<std::byte>('C')) {
    Log() << "Unexpected chroot pipe message: " << static_cast<int>(msg[0]);
    return false;
  }

  Debug() << "Sending chroot reply";

  std::array<std::byte, 1> reply{static_cast<std::byte>('O')};
  if (!Socket::Write(fd.get(), reply)) {
    Errno() << "Failed to send chroot reply";
    return false;
  }

  Debug() << "Sent chroot reply";
  return true;
}

}  // namespace

// static
std::optional<ChrootHelper> ChrootHelper::Spawn() {
  auto pair = Socket::OpenSocketPair();
  if (!pair) {
    return {};
  }

  auto [parent_end, helper_end] = std::move(*pair);

  pid_t helper = fork();
  if (helper == -1) {
    Errno() << "Helper fork failed";
    return {};
  } else if (helper == 0) {
    if (!StubSandboxChrootHelper(std::move(helper_end))) {
      std::exit(1);
    }

    std::exit(0);
  } else {
    return ChrootHelper{std::move(parent_end), helper};
  }
}

}  // namespace zypak
