#!/usr/bin/bash

self_dir="$(dirname $(realpath $0))"
if [[ -d "$self_dir/build" ]]; then
  export ZYPAK_BIN="$self_dir/build"
  export ZYPAK_LIB="$self_dir/build"
else
  export ZYPAK_BIN="$self_dir"
  export ZYPAK_LIB="$self_dir/../lib"
fi

# http://crbug.com/376567
exec < /dev/null
exec > >(exec cat)
exec 2> >(exec cat >&2)

exec "$ZYPAK_BIN/zypak-helper" host - "$@"
