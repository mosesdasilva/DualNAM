#!/bin/sh

set -eu

repo_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
config="$repo_root/NeuralAmpModeler/config.h"
mac_config="$repo_root/NeuralAmpModeler/config/NeuralAmpModeler-mac.xcconfig"
validator="$repo_root/scripts/verify-baseline-macos.sh"

grep -q '^#define PLUG_NAME "DualNAM"$' "$config"
grep -q "^#define PLUG_UNIQUE_ID 'DuNM'$" "$config"
grep -q "^#define PLUG_MFR_ID 'MoDa'$" "$config"
grep -q '^#define BUNDLE_NAME "DualNAM"$' "$config"
grep -q '^BINARY_NAME = DualNAM$' "$mac_config"
grep -q 'DualNAM\.vst3' "$validator"
grep -q 'DualNAM\.component' "$validator"
grep -q 'auval -v aufx DuNM MoDa' "$validator"

echo "DualNAM plug-in identity tests passed"
