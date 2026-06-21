#!/bin/sh

set -eu

repo_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
project="$repo_root/NeuralAmpModeler/projects/NeuralAmpModeler-macOS.xcodeproj"
derived_data_path=${DERIVED_DATA_PATH:-/tmp/DualNAMDerivedData}
vst3="$HOME/Library/Audio/Plug-Ins/VST3/DualNAM.vst3"
auv2="$HOME/Library/Audio/Plug-Ins/Components/DualNAM.component"

cd "$repo_root"

./scripts/check-macos-toolchain.sh
./scripts/test-plugin-identity.sh

xcodebuild \
  -project "$project" \
  -scheme macOS-VST3 \
  -configuration Release \
  -derivedDataPath "$derived_data_path" \
  CODE_SIGNING_ALLOWED=NO \
  build

xcodebuild \
  -project "$project" \
  -scheme macOS-AUv2 \
  -configuration Release \
  -derivedDataPath "$derived_data_path" \
  CODE_SIGNING_ALLOWED=NO \
  build

test -x "$vst3/Contents/MacOS/DualNAM"
test -x "$auv2/Contents/MacOS/DualNAM"

plutil -lint "$vst3/Contents/Info.plist" "$auv2/Contents/Info.plist"

test "$(plutil -extract CFBundleIdentifier raw -o - "$vst3/Contents/Info.plist")" = \
  "com.mosesdasilva.vst3.DualNAM"
test "$(plutil -extract CFBundleIdentifier raw -o - "$auv2/Contents/Info.plist")" = \
  "com.mosesdasilva.audiounit.DualNAM"
test "$(plutil -extract AudioComponents.0.subtype raw -o - "$auv2/Contents/Info.plist")" = "DuNM"
test "$(plutil -extract AudioComponents.0.manufacturer raw -o - "$auv2/Contents/Info.plist")" = "MoDa"

test "$(lipo -archs "$vst3/Contents/MacOS/DualNAM")" = "x86_64 arm64"
test "$(lipo -archs "$auv2/Contents/MacOS/DualNAM")" = "x86_64 arm64"

auval -v aufx DuNM MoDa
