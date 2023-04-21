#!/usr/bin/bash

self_dir="$(dirname $(realpath $0))"
if [[ -d "$self_dir/build" ]]; then
  export ZYPAK_BIN="$self_dir/build"
  export ZYPAK_LIB="$self_dir/build"
else
  export ZYPAK_BIN="$self_dir"
  export ZYPAK_LIB="$self_dir/../lib"
fi

exec "$ZYPAK_BIN/zypak-helper" host - "$@"
