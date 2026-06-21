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
- an unloaded slot is silent.

The installed bundles now use the DualNAM name, version `0.1.0`, and unique
VST3/AUv2 identifiers. Independent A/B gains, latency compensation, and the
final UI are not yet implemented.

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

Run the focused routing tests:

```sh
./scripts/test-routing.sh
```

## Development approach

This project uses small increments, test-driven development, continuous
integration, and real-time-audio safety rules. Contributors and coding agents
must read [`AGENTS.md`](AGENTS.md) before making changes.

## License and attribution

The upstream Neural Amp Modeler plug-in is licensed under the MIT License.
The original copyright and license are retained in [`LICENSE`](LICENSE).

DualNAM is an independent project and is not an official Neural Amp Modeler
release.
