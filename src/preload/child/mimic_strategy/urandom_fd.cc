#include "preload/child/mimic_strategy/urandom_fd.h"

#include <fcntl.h>

namespace zypak::preload {

namespace {

int urandom_fd = -1;

}

void InitUrandomFd() { urandom_fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC); }

int GetUrandomFd() { return urandom_fd; }

}  // namespace zypak::preload
