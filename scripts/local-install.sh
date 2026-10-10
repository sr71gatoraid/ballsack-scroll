#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cmake -S "$project_dir" -B "$project_dir/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=ON
cmake --build "$project_dir/build" --parallel 4
ctest --test-dir "$project_dir/build" --output-on-failure
sudo cmake --install "$project_dir/build"
printf '%s\n' 'Smooth Autoscroll installed. Log out and back in, then enable it in Desktop Effects.'
