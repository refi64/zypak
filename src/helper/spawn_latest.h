// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <string_view>
#include <vector>

namespace zypak {

bool SpawnLatest(std::vector<std::string_view> args, bool wrap_with_zypak);

}
