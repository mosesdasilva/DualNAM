#!/bin/sh

set -eu

repo_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
project="$repo_root/NeuralAmpModeler/projects/NeuralAmpModeler-macOS.xcodeproj"
derived_data_path=${DERIVED_DATA_PATH:-/tmp/DualNAMDerivedData}
vst3="$HOME/Library/Audio/Plug-Ins/VST3/NeuralAmpModeler.vst3"
auv2="$HOME/Library/Audio/Plug-Ins/Components/NeuralAmpModeler.component"

cd "$repo_root"

./scripts/check-macos-toolchain.sh

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

test -x "$vst3/Contents/MacOS/NeuralAmpModeler"
test -x "$auv2/Contents/MacOS/NeuralAmpModeler"

plutil -lint "$vst3/Contents/Info.plist" "$auv2/Contents/Info.plist"

test "$(lipo -archs "$vst3/Contents/MacOS/NeuralAmpModeler")" = "x86_64 arm64"
test "$(lipo -archs "$auv2/Contents/MacOS/NeuralAmpModeler")" = "x86_64 arm64"

auval -v aufx 1YEo SDAa
