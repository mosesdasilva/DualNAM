#!/bin/sh

set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
config="$repo_root/NeuralAmpModeler/config.h"
implementation="$repo_root/NeuralAmpModeler/NeuralAmpModeler.cpp"

grep -q '^#define PLUG_WIDTH 1200$' "$config"
grep -q '^#define PLUG_HEIGHT 520$' "$config"
grep -q '"CHANNEL A"' "$implementation"
grep -q '"CHANNEL B"' "$implementation"
grep -q 'attachGlobalStrip' "$implementation"
grep -q '"GLOBAL INPUT"' "$implementation"
grep -q '"GLOBAL OUTPUT"' "$implementation"
grep -q '"GATE THRESHOLD"' "$implementation"
grep -q 'attachChannelPanel' "$implementation"
grep -q 'kModelAOutputLevel' "$implementation"
grep -q 'kModelBOutputLevel' "$implementation"
grep -q 'kCtrlTagInputMeterB' "$implementation"
grep -q 'kCtrlTagOutputMeterB' "$implementation"
grep -q 'kModelAEQActive' "$implementation"
grep -q 'kModelBEQActive' "$implementation"
grep -q '"EQ_A_KNOBS"' "$implementation"
grep -q '"EQ_B_KNOBS"' "$implementation"

echo "DualNAM UI layout tests passed"
