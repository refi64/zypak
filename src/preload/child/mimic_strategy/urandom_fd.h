#pragma once

// See open_urandom.cc for information on why this is necessary.

namespace zypak::preload {

void InitUrandomFd();
int GetUrandomFd();

}  // namespace zypak::preload
