#!/bin/sh

set -eu

repo_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
test_dir=${TMPDIR:-/tmp}/dualnam-tests

mkdir -p "$test_dir"

xcrun clang++ \
  -std=c++20 \
  -Wall \
  -Wextra \
  -Werror \
  "$repo_root/tests/DualNAMRoutingTests.cpp" \
  -o "$test_dir/routing-tests"

xcrun clang++ \
  -std=c++20 \
  -Wall \
  -Wextra \
  -Werror \
  "$repo_root/tests/DualNAMModelSlotsTests.cpp" \
  -o "$test_dir/model-slot-tests"

xcrun clang++ \
  -std=c++20 \
  -Wall \
  -Wextra \
  -Werror \
  "$repo_root/tests/DualNAMStateTests.cpp" \
  -o "$test_dir/state-tests"

"$test_dir/routing-tests"
"$test_dir/model-slot-tests"
"$test_dir/state-tests"
