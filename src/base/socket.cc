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
  static constexpr size_t kUcredSize = CMSG_SPACE(sizeof(struct ucred));

  ControlBufferSpace(int nfds, bool with_ucred)
      : fd_buffer_size_(nfds * sizeof(int)), with_ucred_(with_ucred) {}

  size_t ctl_buffer_size() const {
    return CMSG_SPACE(fd_buffer_size_) + (with_ucred_ ? kUcredSize : 0);
  }

  size_t rights_cmsg_len() const { return CMSG_LEN(fd_buffer_size_); }
  size_t ucred_cmsg_len() const { return CMSG_LEN(sizeof(struct ucred)); }

  bool with_ucred() const { return with_ucred_; }

 private:
  size_t fd_buffer_size_;
  bool with_ucred_;
};

size_t GetCMsgSize(struct cmsghdr* cmsg) { return cmsg->cmsg_len - CMSG_LEN(0); }

}  // namespace

namespace zypak {

// static
ssize_t Socket::Read(int fd, std::byte* buffer, size_t size, ReadOptions options /*= {}*/) {
  ZYPAK_ASSERT(buffer != nullptr);

  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));

  struct iovec iov = {static_cast<void*>(buffer), size};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  std::unique_ptr<std::byte[]> ctl_buffer;
  if (options.fds != nullptr || options.pid != nullptr) {
    constexpr size_t kMaxFileDescriptors = 16;
    ControlBufferSpace space(options.fds != nullptr ? kMaxFileDescriptors : 0,
                             /*with_ucred=*/options.pid != nullptr);

    ctl_buffer = std::make_unique<std::byte[]>(space.ctl_buffer_size());
    msg.msg_control = ctl_buffer.get();
    msg.msg_controllen = space.ctl_buffer_size();

    if (options.pid != nullptr) {
      *options.pid = -1;
    }
  }

  ssize_t res = HANDLE_EINTR(recvmsg(fd, &msg, 0));
  if (res == -1) {
    return -1;
  }

  if (msg.msg_controllen > 0 && ctl_buffer) {
    for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr;
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {
      if (cmsg->cmsg_level == SOL_SOCKET) {
        if (cmsg->cmsg_type == SCM_RIGHTS) {
          if (options.fds == nullptr) {
            continue;
          }

          size_t nfds = GetCMsgSize(cmsg) / sizeof(int);
          options.fds->reserve(nfds);
          int* cmsg_fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
          for (int i = 0; i < nfds; i++) {
            options.fds->emplace_back(cmsg_fds[i]);
          }
        } else if (cmsg->cmsg_type == SCM_CREDENTIALS) {
          if (options.pid == nullptr) {
            continue;
          }

          ZYPAK_ASSERT(GetCMsgSize(cmsg) == sizeof(struct ucred));
          struct ucred* cred = reinterpret_cast<struct ucred*>(CMSG_DATA(cmsg));
          ZYPAK_ASSERT(cred->pid != 0);
          *options.pid = cred->pid;
        }
      }
    }
  }

  if (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) {
    errno = EMSGSIZE;
    return -1;
  }

  if (options.pid != nullptr && *options.pid == -1) {
    errno = ESRCH;
    return -1;
  }

  return res;
}

// static
ssize_t Socket::Read(int fd, std::vector<std::byte>* buffer, ReadOptions options /*= {}*/) {
  return Read(fd, buffer->data(), buffer->size(), std::move(options));
}

// static
bool Socket::Write(int fd, const std::byte* buffer, size_t size, WriteOptions options /*= {}*/) {
  ZYPAK_ASSERT(buffer != nullptr);

  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));

  struct iovec iov = {const_cast<void*>(reinterpret_cast<const void*>(buffer)), size};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  std::unique_ptr<std::byte[]> ctl_buffer;

  if (options.fds != nullptr) {
    ControlBufferSpace space(options.fds != nullptr ? options.fds->size() : 0,
                             /*with_ucred=*/false);

    ctl_buffer = std::make_unique<std::byte[]>(space.ctl_buffer_size());
    msg.msg_control = ctl_buffer.get();
    msg.msg_controllen = space.ctl_buffer_size();

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = space.rights_cmsg_len();

    int* it = reinterpret_cast<int*>(CMSG_DATA(cmsg));
    std::copy(options.fds->begin(), options.fds->end(), it);
  }

  return HANDLE_EINTR(sendmsg(fd, &msg, MSG_NOSIGNAL)) == size;
}

// static
bool Socket::Write(int fd, const std::vector<std::byte>& buffer, WriteOptions options /*= {}*/) {
  return Write(fd, buffer.data(), buffer.size(), std::move(options));
}

// static
bool Socket::Write(int fd, std::string_view buffer, WriteOptions options /*= {}*/) {
  return Write(fd, reinterpret_cast<const std::byte*>(buffer.data()),
               buffer.size() + 1,  // include the null terminator
               std::move(options));
}

// static
bool Socket::EnableReceivePid(int fd) {
  int value = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &value, sizeof(value)) == -1) {
    Errno() << "Failed to enable SO_PASSCRED";
    return false;
  }

  return true;
}

}  // namespace zypak
