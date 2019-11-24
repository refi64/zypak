// Copyright 2019 Endless Mobile, Inc.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/base.h"
#include "../epoll.h"

#include <set>

#include <nickle.h>

void HandleReap(Epoll* ep, std::set<pid_t>* children, nickle::Reader* reader);
