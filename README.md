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

DualNAM is currently in development bootstrap. The official NAM plug-in source
and its pinned dependencies have been cloned, but the dual-model DSP and UI
have not been implemented yet.

The current plan is documented in
[`docs/IMPLEMENTATION_PLAN.md`](docs/IMPLEMENTATION_PLAN.md).

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

The first baseline build will use the `macOS-VST3` and `macOS-AUv2` schemes in
`NeuralAmpModeler/NeuralAmpModeler.xcworkspace`.

## Development approach

This project uses small increments, test-driven development, continuous
integration, and real-time-audio safety rules. Contributors and coding agents
must read [`AGENTS.md`](AGENTS.md) before making changes.

## License and attribution

The upstream Neural Amp Modeler plug-in is licensed under the MIT License.
The original copyright and license are retained in [`LICENSE`](LICENSE).

DualNAM is an independent project and is not an official Neural Amp Modeler
release.
