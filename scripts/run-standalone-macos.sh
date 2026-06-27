#!/bin/sh

set -eu

usage() {
  cat <<'USAGE'
Usage: ./scripts/run-standalone-macos.sh [--no-open] [--release] [--clean]

Builds the dev-only DualNAM standalone macOS app and opens it for fast local
troubleshooting. This does not validate VST3/AUv2 host behavior.

Options:
  --no-open   Build and verify the app bundle, but do not launch it.
  --release   Build the standalone app with the Release configuration.
  --clean     Remove the standalone app output and derived data before building.
  --help      Show this help.

Environment:
  DERIVED_DATA_PATH             Override Xcode derived-data path.
  DUALNAM_STANDALONE_APP_PATH   Override standalone app output directory.
USAGE
}

repo_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
project="$repo_root/NeuralAmpModeler/projects/NeuralAmpModeler-macOS.xcodeproj"
configuration=Debug
open_app=1
clean=0

while [ "$#" -gt 0 ]; do
  case "$1" in
    --no-open)
      open_app=0
      ;;
    --release)
      configuration=Release
      ;;
    --clean)
      clean=1
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

derived_data_path=${DERIVED_DATA_PATH:-/tmp/DualNAMStandaloneDerivedData}
app_path=${DUALNAM_STANDALONE_APP_PATH:-/tmp/DualNAMStandaloneApps}
app="$app_path/DualNAM.app"
app_binary="$app/Contents/MacOS/DualNAM"
info_plist="$app/Contents/Info.plist"

cd "$repo_root"

if [ "$clean" -eq 1 ]; then
  case "$derived_data_path" in ""|"/") echo "Refusing to clean DERIVED_DATA_PATH=$derived_data_path" >&2; exit 2 ;; esac
  case "$app_path" in ""|"/") echo "Refusing to clean DUALNAM_STANDALONE_APP_PATH=$app_path" >&2; exit 2 ;; esac
  rm -rf "$derived_data_path" "$app_path"
fi

xcodebuild \
  -project "$project" \
  -scheme macOS-APP \
  -configuration "$configuration" \
  -derivedDataPath "$derived_data_path" \
  APP_PATH="$app_path" \
  CODE_SIGNING_ALLOWED=NO \
  build

test -x "$app_binary"
plutil -lint "$info_plist"
test "$(plutil -extract CFBundleIdentifier raw -o - "$info_plist")" = \
  "com.mosesdasilva.app.DualNAM"

echo "Built standalone app: $app"

if [ "$open_app" -eq 1 ]; then
  if pgrep -x DualNAM >/dev/null 2>&1; then
    osascript -e 'tell application "DualNAM" to quit' >/dev/null 2>&1 || true
    sleep 1
  fi
  open "$app"
fi
