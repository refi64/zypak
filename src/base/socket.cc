// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/socket.h"

#include <sys/socket.h>

#include <cstring>
#include <memory>

#include "base/debug.h"

namespace {

class ControlBufferSpace {
 public:
  constexpr ControlBufferSpace(int nfds) : fd_buffer_size(nfds * sizeof(int)) {}

  constexpr size_t ctl_buffer_size() const { return CMSG_SPACE(fd_buffer_size); }
  static constexpr size_t ucred_size() { return CMSG_SPACE(sizeof(struct ucred)); }
  constexpr size_t ctl_buffer_size_with_ucred() const { return ctl_buffer_size() + ucred_size(); }
  constexpr size_t cmsg_len() const { return CMSG_LEN(fd_buffer_size); }

 private:
  size_t fd_buffer_size;
};

}  // namespace

/*static*/
ssize_t Socket::Read(int fd, std::byte* buffer, size_t size,
                     std::vector<unique_fd>* fds /*= nullptr*/) {
  ZYPAK_ASSERT(buffer != nullptr);

  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));

  struct iovec iov = {static_cast<void*>(buffer), size};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  constexpr size_t kMaxFileDescriptors = 16;
  constexpr ControlBufferSpace kCtlBufferSpace(kMaxFileDescriptors);

  std::array<std::byte, kCtlBufferSpace.ctl_buffer_size()> ctl_buffer;
  msg.msg_control = reinterpret_cast<std::byte*>(ctl_buffer.data());
  msg.msg_controllen = ctl_buffer.size();

  ssize_t res = HANDLE_EINTR(recvmsg(fd, &msg, 0));
  if (res == -1) {
    return -1;
  }

  if (msg.msg_controllen > 0 && fds != nullptr) {
    for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr;
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {
      if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
        size_t cmsg_size = cmsg->cmsg_len - CMSG_LEN(0);
        size_t nfds = cmsg_size / sizeof(int);
        fds->reserve(nfds);
        int* cmsg_fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
        for (int i = 0; i < nfds; i++) {
          fds->emplace_back(cmsg_fds[i]);
        }
      }
    }
  }

  if (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) {
    errno = EMSGSIZE;
    return -1;
  }

  return res;
}

/*static*/
ssize_t Socket::Read(int fd, std::vector<std::byte>* buffer,
                     std::vector<unique_fd>* fds /*= nullptr*/) {
  return Read(fd, buffer->data(), buffer->size(), fds);
}

/*static*/
bool Socket::Write(int fd, const std::byte* buffer, size_t size,
                   const std::vector<int>* fds /*= nullptr*/) {
  ZYPAK_ASSERT(buffer != nullptr);

  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));

  struct iovec iov = {const_cast<void*>(reinterpret_cast<const void*>(buffer)), size};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  std::unique_ptr<std::byte[]> ctl_buffer;

  if (fds != nullptr) {
    ControlBufferSpace space(fds->size());

    ctl_buffer = std::make_unique<std::byte[]>(space.ctl_buffer_size());
    msg.msg_control = reinterpret_cast<std::byte*>(ctl_buffer.get());
    msg.msg_controllen = space.ctl_buffer_size();

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = space.cmsg_len();

    auto it = reinterpret_cast<int*>(CMSG_DATA(cmsg));
    std::copy(fds->begin(), fds->end(), it);
  }

  return HANDLE_EINTR(sendmsg(fd, &msg, MSG_NOSIGNAL)) == size;
}

/*static*/
bool Socket::Write(int fd, const std::vector<std::byte>& buffer,
                   const std::vector<int>* fds /*= nullptr*/) {
  return Write(fd, buffer.data(), buffer.size(), fds);
}

/*static*/
bool Socket::Write(int fd, std::string_view buffer, const std::vector<int>* fds /*= nullptr*/) {
  return Write(fd, reinterpret_cast<const std::byte*>(buffer.data()),
               buffer.size() + 1,  // include the null terminator
               fds);
}
