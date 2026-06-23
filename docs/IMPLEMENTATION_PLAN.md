# DualNAM implementation plan

## Product target

DualNAM is a local macOS audio effect built from the official Neural Amp Modeler
plug-in:

- formats: VST3 and Audio Unit v2;
- input: one stereo signal carrying two independently routed guitar channels;
- model slots: two independent `.nam` files, A and B;
- routing: left input feeds model A and right input feeds model B;
- output: model A is the left channel and model B is the right channel;
- controls for the first usable version:
  - input gain;
  - model A gain;
  - model B gain;
  - output gain;
  - load/clear model A;
  - load/clear model B.

The product name is **DualNAM**. “NAM” means Neural Amp Modeler; it is not
related to the NAMM trade show.

## Assumptions for version 1

- The plug-in requires a stereo input bus and a stereo output bus.
- Left and right inputs remain independent throughout the first version:
  - input left feeds model A;
  - input right feeds model B.
- Gig Performer is responsible for duplicating or otherwise preparing a mono
  guitar source into the two input channels when dual-mono operation is
  required.
- Both model slots use independent NAM DSP instances, including independent
  resampling state.
- If one slot is empty, that output channel is silent. This is safer than
  passing dry guitar on one side.
- Model A and model B remain hard-panned. No pan or blend control is included.
- The first version excludes IR loading, EQ, noise gate, model slimming,
  standalone app, AAX, AUv3, and capture/training. These can be reconsidered
  after the dual-model plug-in is stable.
- “Independent mono” use means a host may route each input and output channel
  separately.
- A later input-mode switch is planned:
  - **Stereo:** left to model A and right to model B;
  - **Mono:** one mono source duplicated into both models.
  The exact mono source rule (left input, right input, or stereo sum) will be
  chosen before that switch is implemented.

## Upstream baseline

The workspace is cloned recursively from:

- plug-in: <https://github.com/sdatkinson/NeuralAmpModelerPlugin>
- DSP core: <https://github.com/sdatkinson/NeuralAmpModelerCore>

Baseline checked on June 19, 2026:

- NeuralAmpModelerPlugin `0.7.15`, commit
  `96337e9ab6e3beb619459779bbb5c47e1b04d8c4`;
- NeuralAmpModelerCore `0.5.3`, submodule commit
  `9c7b185de346fe0725dea537bcee4bc38b5bb6d6`.

The plug-in repository already contains the correct iPlug2 project and pins the
core, Eigen, and AudioDSPTools dependencies as submodules. Maintaining those
submodule pins is simpler and less risky than creating a new plug-in framework.

## Proposed processing graph

```text
stereo host input
       |                 |
 input left          input right
       |                 |
  input gain          input gain
       |                 |
 NAM model A        NAM model B
       |                 |
    A gain             B gain
       |                 |
 left output        right output
       \                 /
          output gain
```

No allocation, model loading, file I/O, or locks may occur in the audio
processing callback. Each model is loaded into a staging pointer off the active
audio path and swapped into its slot at a block boundary, following the current
upstream design.

## Reviewable increments

### 1. Establish a buildable upstream baseline

- Install full Xcode and accept its license.
- Download iPlug2's pinned VST3 SDK and prebuilt graphics libraries.
- Build the unmodified upstream VST3 and AU targets.
- Validate that both bundles are produced in the user's audio plug-in folders.

Exit condition: the unmodified upstream source builds locally.

### 2. Add dual-model DSP routing without redesigning the UI

- Replace the single model/staged-model/path state with two explicit slots.
- Allocate two mono input buffers and two mono model output buffers.
- Feed input channel 0 to NAM A and input channel 1 to NAM B.
- Write A to host output 0 and B to host output 1.
- Add independent A/B output-gain parameters.
- Report the maximum latency of the two slots.
- Silence an unloaded slot.

Exit condition: deterministic left-to-A/right-to-B stereo routing works with
two test models, with no changes to branding or visual design.

Progress:

- Completed: testable left/right routing seam, two internal channels, slot
  isolation, silent unloaded slots, two-slot ownership/staging/reset/removal,
  maximum branch latency reporting, separate A/B model browsers and paths,
  versioned DualNAM state with legacy slot-A migration, unique DualNAM VST3/AUv2
  bundle identity, VST3/AUv2 builds, and AU validation.
- Controlled Gig Performer testing completed successfully: independent capture
  selection, changing, clearing, save/close/reopen restoration, shared input
  control, EQ, and noise gate all worked as intended.
- Remaining before production use: add independent gains, compensate unequal
  branch latency, validate VST3 in a host/pluginval, and move model retirement
  off the audio thread.

### 3. Add the minimal two-slot UI and state

- Add separate model A and B file browsers.
- Add A gain and B gain controls.
- Serialize both paths and all four gains.
- Use a new DualNAM state header and version; do not claim compatibility with
  NeuralAmpModeler presets.

Exit condition: save/reopen a DAW session and restore both model paths and
gains.

Progress:

- Completed: separate model A/B browsers, independent load/clear handling,
  persisted A/B paths, a versioned DualNAM state header, and legacy upstream
  NAM state migration into slot A. Independent pre-model A/B input gains were
  appended as parameter IDs 13 and 14 with a -20 dB to +20 dB range and 0 dB
  defaults; schema version 2 persists them while schema version 1 restores them
  at their defaults. Independent output gains were appended as parameter IDs
  15 and 16 with a -40 dB to +40 dB range and 0 dB defaults. Schema version 3
  persists them; older shared Output values migrate into both branches. The
  original Output parameter remains at stable ID 5 and now serves as Global
  Output. The editor is now 1200x540, with a 140-pixel global strip above two
  identical 600x400 channel panels. Model loading, branch input gain, branch
  output gain, and mono input/output meters are functional on both panels. Each
  input meter reads its branch after Input A/B; each output meter reads its
  final host channel after Output A/B and Global Output. Independent EQ switches and
  Bass/Middle/Treble parameters were appended as IDs 17 through 24. Each branch
  owns a separate mono tone-stack DSP instance. Schema version 4 persists the
  eight EQ parameters; older shared EQ values migrate into both branches.
  Legacy Bass/Middle/Treble/ToneStack parameter IDs remain reserved for host
  compatibility but no longer drive active DSP. The editor is now 1200x540:
  a 140-pixel shared strip above the channel panels contains Global Input, the
  one shared stereo noise-gate threshold/toggle, and Global Output. Global
  Output uses stable parameter ID 5 and is applied after Output A/B. Schema
  version 5 activates that master output while migrating schemas 1 through 4
  without applying their old shared Output value twice. Channel A retains the
  inherited shared IR, while the Channel B IR selector remains disabled.
  The editor has an opaque full-size base and separately drawn strip
  background. Global labels are separate controls, and the gate switch has a
  dedicated non-overlapping hit rectangle.
- Remaining: another manual host save/reopen verification for the new controls.
  Replace placeholders one behavior at a time as independent IR paths are
  implemented.

### 4. Rename and package as DualNAM

- Rename plug-in display name, bundle name, C++ class, identifiers, plist
  references, Xcode targets, resources, and output bundles.
- Use new manufacturer and plug-in identifiers rather than upstream IDs.
- Build only VST3 and AU for the first local release.
- Add license and third-party notices.

Exit condition: DualNAM can coexist with the official NAM plug-in.

Progress:

- Completed for the supported VST3/AUv2 targets: display name, output bundle
  names, version `0.1.0`, manufacturer, four-character component IDs, bundle
  identifiers, generated plist metadata, and validation commands.
- Intentionally deferred: source class, Xcode target, project, and resource
  filename renames. They are internal and do not affect host coexistence.
- Local migration note: the first build after changing the target product name
  may remove a previously built `NeuralAmpModeler` bundle as stale Xcode output.
  Reinstall the official NAM plug-in if it is needed alongside DualNAM.

### 5. Validate the local release

- Test 44.1, 48, and 96 kHz.
- Test common buffer sizes from 32 through 1024 samples.
- Confirm the left input reaches only model A and the left output.
- Confirm the right input reaches only model B and the right output.
- Confirm Gig Performer can duplicate one mono source to both plug-in inputs
  for dual-mono operation.
- Confirm A appears only on left and B only on right.
- Confirm each gain control is automatable and state restores correctly.
- Run `auval` on the AU and `pluginval` on both formats.
- Measure CPU with two representative models.

## Required downloads

1. Full Xcode from the Mac App Store. Command Line Tools alone are insufficient
   because this project builds through an Xcode workspace/project.
2. The VST3 SDK pinned by this version of iPlug2:

   ```sh
   cd iPlug2/Dependencies/IPlug
   ./download-iplug-sdks.sh
   ```

3. iPlug2's pinned prebuilt macOS libraries:

   ```sh
   cd iPlug2/Dependencies
   ./download-prebuilt-libs.sh
   ```

Optional validation download after the first successful build:

- pluginval: <https://github.com/Tracktion/pluginval>

No separate JUCE installation is required. This upstream plug-in uses iPlug2.

## Local build outline

After installing Xcode:

```sh
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
sudo xcodebuild -license accept
./scripts/check-macos-toolchain.sh
./scripts/verify-baseline-macos.sh
```

The verified baseline script builds the VST3 and AU schemes directly from the
macOS Xcode project. The existing distribution script builds additional
formats and assumes release packaging tools, so it is not the local-development
command.

Expected development output locations are:

- `~/Library/Audio/Plug-Ins/VST3`
- `~/Library/Audio/Plug-Ins/Components`

## Risks

- Running two NAM instances roughly doubles model inference cost.
- Different model sample rates create independent resampler latency. The host
  sees one plug-in latency, so the lower-latency branch may require explicit
  delay compensation to keep left and right time-aligned.
- Upstream's current class and Xcode project names are repeated throughout the
  project. Renaming before the DSP works would create a large, hard-to-review
  diff.
- Public distribution requires unique identifiers, signing, notarization, and
  a careful license/attribution review. A local unsigned development build does
  not require completing that release process.
