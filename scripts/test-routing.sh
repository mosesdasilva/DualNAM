#!/bin/sh

set -eu

repo_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
test_binary=${TMPDIR:-/tmp}/dualnam-routing-tests

xcrun clang++ \
  -std=c++20 \
  -Wall \
  -Wextra \
  -Werror \
  "$repo_root/tests/DualNAMRoutingTests.cpp" \
  -o "$test_binary"

"$test_binary"
