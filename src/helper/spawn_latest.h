// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <vector>

#include "base/cstring_view.h"

namespace zypak {

bool SpawnLatest(std::vector<cstring_view> args, bool wrap_with_zypak);

}
