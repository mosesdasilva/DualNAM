# DualNAM

DualNAM is a work-in-progress macOS VST3/AUv2 effect that runs two independent
[Neural Amp Modeler](https://www.neuralampmodeler.com/) models in one stereo
plug-in.

The initial routing target is:

```text
left input  -> NAM model A -> left output
right input -> NAM model B -> right output
```

Each slot will load its own `.nam` file and expose independent gain control.
Gig Performer can duplicate one mono guitar source to both plug-in inputs when
dual-mono processing is needed.

## Project status

DualNAM is currently in early development. The first stereo-routing increment
is implemented and tested:

- left input is routed to model slot A and the left output;
- right input is routed to model slot B and the right output;
- model slots stage, activate, reset, clear, and report latency independently;
- separate model browsers load and clear slots A and B;
- project state stores both model paths and reads legacy NAM state into slot A;
- shared input gain is followed by independent `Input A` and `Input B` trims,
  each ranging from -20 dB to +20 dB with a 0 dB default;
- independent `Output A` and `Output B` trims feed the left and right host
  outputs respectively, each ranging from -40 dB to +40 dB with a 0 dB
  default;
- the editor is a 1200x540 layout with a shared 140-pixel strip above the
  Channel A and Channel B panels;
- both panels have matching control positions; model loading, branch input
  gain, and branch output gain are active on both sides, while unimplemented
  Channel B processing controls are disabled placeholders;
- each panel has independent input and output meters: input meters observe the
  signal after `Input A/B`, and output meters observe the final signal after
  `Output A/B`;
- each panel has an independent EQ switch and independent Bass, Middle, and
  Treble controls, backed by separate mono tone-stack DSP instances;
- the top strip contains Global Input, the shared stereo Noise Gate threshold
  and toggle, and Global Output after the two branch output gains;
- opaque editor/strip backgrounds and separate global labels prevent bitmap
  gaps and compact-knob label overlap;
- an unloaded slot is silent.

The installed bundles now use the DualNAM name, version `0.1.0`, and unique
VST3/AUv2 identifiers. Independent IRs, latency compensation, and the final
control implementations are not yet implemented.

The current plan is documented in
[`docs/IMPLEMENTATION_PLAN.md`](docs/IMPLEMENTATION_PLAN.md).

For a beginner-oriented explanation of the architecture, C++ concepts, signal
flow, state handling, tests, and development history, read
[`docs/PROJECT_WALKTHROUGH.md`](docs/PROJECT_WALKTHROUGH.md).

## Upstream projects

DualNAM starts from and preserves the history of:

- [sdatkinson/NeuralAmpModelerPlugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin)
- [sdatkinson/NeuralAmpModelerCore](https://github.com/sdatkinson/NeuralAmpModelerCore)

The plug-in uses the upstream iPlug2 project and pinned Git submodules. Clone
recursively:

```sh
git clone --recursive https://github.com/mosesdasilva/DualNAM.git
cd DualNAM
```

## Development setup

Required on macOS:

- full Xcode;
- Apple Command Line Tools;
- Git and Python 3;
- the repository-managed VST3 SDK and iPlug2 prebuilt libraries.

Check the local setup:

```sh
./scripts/check-macos-toolchain.sh
```

If the VST3 SDK or iPlug2 libraries are missing:

```sh
cd iPlug2/Dependencies/IPlug
./download-iplug-sdks.sh

cd ..
./download-prebuilt-libs.sh
```

Build and validate the current DualNAM VST3/AUv2 development bundles:

```sh
./scripts/verify-baseline-macos.sh
```

The script uses the `macOS-VST3` and `macOS-AUv2` schemes from
`NeuralAmpModeler/projects/NeuralAmpModeler-macOS.xcodeproj`. It installs
unsigned `DualNAM.vst3` and `DualNAM.component` development bundles into the
user's plug-in folders, verifies their identifiers, and runs Apple's AU
validator.

The Xcode target, source class, and plist filenames intentionally retain their
upstream names for now. Hosts see the unique DualNAM bundle names and IDs.

For fast GUI and manual troubleshooting, build and open the dev-only standalone
app:

```sh
./scripts/run-standalone-macos.sh
```

This uses the existing `macOS-APP` scheme and writes the app to
`/tmp/DualNAMStandaloneApps/DualNAM.app` by default, leaving the supported
VST3/AUv2 install locations untouched. Use `--no-open` for build-only checks,
`--release` for a Release standalone build, and `--clean` to remove the
standalone app output and derived data before rebuilding.

The standalone app is a speed tool for GUI iteration, model-loading checks,
file-browser behavior, parameter interaction, basic audio I/O, and crash
reproduction. It does not replace VST3/AUv2 validation or Gig Performer
testing.

Run the focused routing tests:

```sh
./scripts/test-routing.sh
```

This command also checks the doubled editor width and mirrored channel-panel
structure.

Recommended local feedback loop:

```sh
./scripts/test-routing.sh
./scripts/run-standalone-macos.sh
./scripts/verify-baseline-macos.sh
```

Use the standalone app for fast manual feedback, then use the VST3/AUv2
validation path before treating a plug-in change as host-ready.

## Development approach

This project uses small increments, test-driven development, continuous
integration, and real-time-audio safety rules. Contributors and coding agents
must read [`AGENTS.md`](AGENTS.md) before making changes.

## License and attribution

The upstream Neural Amp Modeler plug-in is licensed under the MIT License.
The original copyright and license are retained in [`LICENSE`](LICENSE).

DualNAM is an independent project and is not an official Neural Amp Modeler
release.
