#!/bin/sh

set -u

failures=0

check_command()
{
  command_name=$1
  if command -v "$command_name" >/dev/null 2>&1; then
    printf "ok: %s\n" "$command_name"
  else
    printf "missing: %s\n" "$command_name"
    failures=$((failures + 1))
  fi
}

check_command git
check_command clang++
check_command python3
check_command xcodebuild

developer_dir=$(xcode-select -p 2>/dev/null || true)
case "$developer_dir" in
  */Xcode.app/Contents/Developer)
    printf "ok: full Xcode selected (%s)\n" "$developer_dir"
    ;;
  *)
    printf "missing: full Xcode is not selected (current: %s)\n" "${developer_dir:-none}"
    failures=$((failures + 1))
    ;;
esac

if [ -f "iPlug2/Dependencies/IPlug/VST3_SDK/CMakeLists.txt" ]; then
  printf "ok: iPlug2 VST3 SDK\n"
else
  printf "missing: iPlug2 VST3 SDK; run iPlug2/Dependencies/IPlug/download-iplug-sdks.sh\n"
  failures=$((failures + 1))
fi

if [ -d "iPlug2/Dependencies/Build/mac/lib" ]; then
  printf "ok: iPlug2 prebuilt macOS libraries\n"
else
  printf "missing: iPlug2 prebuilt libraries; run iPlug2/Dependencies/download-prebuilt-libs.sh\n"
  failures=$((failures + 1))
fi

if [ "$failures" -ne 0 ]; then
  printf "\nToolchain is not ready: %s check(s) failed.\n" "$failures"
  exit 1
fi

printf "\nToolchain is ready for the baseline build.\n"
