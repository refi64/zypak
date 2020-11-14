#include "preload/child/mimic_strategy/urandom_fd.h"
#include "preload/main_override.h"

using namespace zypak::preload;

int MAIN_OVERRIDE(int argc, char** argv, char** envp) {
  InitUrandomFd();
  return true_main(argc, argv, envp);
}

INSTALL_MAIN_OVERRIDE()
