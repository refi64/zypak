// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <stdio.h>

#include <algorithm>
#include <iostream>
#include <vector>

#include "base/base.h"
#include "base/debug.h"
#include "base/env.h"
#include "base/str_util.h"
#include "sandbox/mimic_strategy/zygote.h"
#include "sandbox/spawn_strategy/run.h"

using namespace zypak;
using namespace zypak::sandbox;

bool MakeStdinNull() {
  unique_fd stdin_fd(open("/dev/null", O_RDONLY));
  if (stdin_fd.invalid()) {
    Errno() << "Failed to open /dev/null";
    return false;
  } else if (dup2(stdin_fd.get(), 0) == -1) {
    Errno() << "Failed to dup2(/dev/null, 0)";
    return false;
  }

  return true;
}

bool RunCatOnFd(int fd) {
  int fds[2] = {-1, -1};
  if (pipe(fds) == -1) {
    Errno() << "pipe()";
    return false;
  }

  unique_fd rd = fds[0];
  unique_fd wr = fds[1];

  pid_t pid = fork();
  if (pid == -1) {
    Errno() << "Failed to fork";
    return false;
  } else if (pid == 0) {
    wr.reset();

    if (dup2(rd.get(), STDIN_FILENO) == -1) {
      Errno() << "Failed to dup2(reader, STDIN_FILENO)";
      return false;
    }

    if (fd != STDOUT_FILENO) {
      if (dup2(STDOUT_FILENO, fd) == -1) {
        Errno() << "Failed to dup2(STDOUT_FILENO, " << fd << ")";
        return false;
      }
    }

    execlp("cat", "cat", NULL);
    Errno() << "Failed to exec cat";
  }

  rd.reset();
  if (dup2(wr.get(), fd) == -1) {
    Errno() << "dup2(writer, " << fd << ")";
    return false;
  }

  return true;
}

bool SanitizeStdio() {
  // http://crbug.com/376567
  // We do it here instead of in a wrapper script so that Electron apps that want to mess with stdin
  // still can.
  return MakeStdinNull() && RunCatOnFd(STDOUT_FILENO) && RunCatOnFd(STDERR_FILENO);
}

int main(int argc, char** argv) {
  DebugContext::instance()->set_name("zypak-sandbox");
  DebugContext::instance()->LoadFromEnvironment();

  if (argc < 2) {
    Log() << "zypak-sandbox: wrong arguments";
    return 1;
  }

  std::vector<std::string> args(argv + 1, argv + argc);
  if (args.front() == "--get-api") {
    // Mimic sandbox API version 1.
    std::cout << 1 << std::endl;
    return 0;
  } else if (args.front() == "--adjust-oom-score") {
    Debug() << "XXX ignoring --adjust-oom-score " << args[1] << ' ' << args[2];
    return 0;
  } else {
    if (!SanitizeStdio()) {
      Log() << "Failed to sanitize stdio, quitting...";
      return 1;
    }

    ZYPAK_ASSERT(!isatty(STDIN_FILENO), << "stdin is a TTY");
    ZYPAK_ASSERT(!isatty(STDOUT_FILENO), << "stdout is a TTY");
    ZYPAK_ASSERT(!isatty(STDERR_FILENO), << "stderr is a TTY");

    if (Env::Test(Env::kZypakZygoteStrategySpawn)) {
      if (!spawn_strategy::RunSpawnStrategy(std::move(args))) {
        return 1;
      }
    } else {
      if (std::find(args.begin(), args.end(), "--type=zygote") == args.end()) {
        auto cmdline = Join(args.begin() + 1, args.end());
        Log() << "Ignoring non-Zygote command: " << cmdline;
        return 1;
      }

      auto runner = mimic_strategy::MimicZygoteRunner::Create();
      if (!runner) {
        Log() << "Failed to create zygote runner";
        return 1;
      }

      if (!runner->Run()) {
        return 1;
      }
    }
  }

  return 0;
}
