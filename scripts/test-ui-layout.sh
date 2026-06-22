#!/bin/sh

set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
config="$repo_root/NeuralAmpModeler/config.h"
implementation="$repo_root/NeuralAmpModeler/NeuralAmpModeler.cpp"

grep -q '^#define PLUG_WIDTH 1200$' "$config"
grep -q '"CHANNEL A"' "$implementation"
grep -q '"CHANNEL B"' "$implementation"
grep -q 'attachChannelPanel' "$implementation"
grep -q 'kModelAOutputLevel' "$implementation"
grep -q 'kModelBOutputLevel' "$implementation"
grep -q 'kCtrlTagInputMeterB' "$implementation"
grep -q 'kCtrlTagOutputMeterB' "$implementation"

echo "DualNAM UI layout tests passed"
