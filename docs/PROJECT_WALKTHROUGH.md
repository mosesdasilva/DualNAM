# DualNAM project walkthrough

This document explains what was built, why it was built that way, and how the
C++ code relates to familiar audio concepts. It is intended as a learning guide
for the project owner, not merely as API documentation.

## 1. What DualNAM is

DualNAM is a stereo macOS audio effect based on the official Neural Amp Modeler
plug-in and NAM DSP core.

Its defining signal path is:

```text
host left input  -> shared input gain -> NAM model A -> shared processing -> left output
host right input -> shared input gain -> NAM model B -> shared processing -> right output
```

The current implementation also has an independent input trim immediately
before each model:

```text
left  -> shared input -> Input A -> NAM A
right -> shared input -> Input B -> NAM B
```

`Input A` and `Input B` each range from -20 dB to +20 dB and default to 0 dB.

The two NAM captures are independent:

- slot A loads one `.nam` file;
- slot B loads another `.nam` file;
- left never feeds model B;
- right never feeds model A;
- model A writes only to the left output;
- model B writes only to the right output;
- an empty slot produces silence on its output channel.

Gig Performer can duplicate one mono guitar input to both plug-in inputs. That
makes both models receive the same performance while DualNAM still operates as
a true stereo, two-lane processor.

The current plug-in also retains inherited upstream controls such as shared
input/output gain, EQ, noise gate, IR processing, calibration, and model
slimming. The controlled Gig Performer test confirmed that loading, unloading,
changing captures, saving, closing, reopening, EQ, noise gate, and shared input
control all work.

## 2. What came from upstream and what we changed

The project was not written from an empty folder. It began as a fork of:

- `sdatkinson/NeuralAmpModelerPlugin`;
- `sdatkinson/NeuralAmpModelerCore`.

The upstream project already supplied:

- the real NAM neural-network inference engine;
- `.nam` file parsing;
- sample-rate conversion around a NAM model;
- the iPlug2 VST3/AUv2 plug-in framework;
- the original user interface;
- parameter handling;
- EQ, noise gate, IR, metering, and calibration behavior;
- Xcode projects and macOS bundle generation.

The DualNAM work changed the plug-in from one active NAM lane into two
independent lanes. It added:

- deterministic stereo routing;
- two model ownership slots;
- independent A/B loading and clearing;
- two persisted model paths;
- a versioned DualNAM state format;
- compatibility when reading older single-model NAM state;
- separate A/B model browsers;
- unique host-facing DualNAM names and identifiers;
- focused routing, model-lifecycle, state, and identity tests;
- repeatable VST3/AUv2 build and validation scripts.

This is an important architectural distinction: DualNAM reuses the proven NAM
engine instead of attempting to reproduce neural-network inference.

## 3. A short C-to-C++ translation guide

Much of C++ still looks like C: functions, loops, arrays, pointers, conditions,
and structures all remain. The main additions used in DualNAM are types that
help express ownership and reusable behavior more safely.

### Namespaces

```cpp
namespace dualnam
{
  // DualNAM-owned names live here.
}
```

A namespace is similar to putting a prefix on related C names. Instead of a
global function such as:

```c
void dualnam_process_stereo(...);
```

C++ can use:

```cpp
dualnam::ProcessStereoModels(...);
```

The `::` operator means “look inside this namespace, class, or enum.”

### Classes

A class combines data and functions that operate on that data.

```cpp
class GainModel
{
public:
  explicit GainModel(double gain)
  : mGain(gain)
  {
  }

  void process(double** inputs, double** outputs, int numFrames)
  {
    for (int frame = 0; frame < numFrames; ++frame)
      outputs[0][frame] = mGain * inputs[0][frame];
  }

private:
  double mGain;
};
```

In C, this could be represented as a structure plus separate functions:

```c
typedef struct
{
  double gain;
} GainModel;

void gain_model_process(
  GainModel* model,
  double** inputs,
  double** outputs,
  int num_frames)
{
  for (int frame = 0; frame < num_frames; ++frame)
    outputs[0][frame] = model->gain * inputs[0][frame];
}
```

The C++ class keeps the gain and its processing operation together.

### Constructors

This code:

```cpp
GainModel modelA(2.0);
```

creates a `GainModel` and calls its constructor with a gain of `2.0`.

The constructor initializer:

```cpp
: mGain(gain)
```

places the incoming `gain` value into the member variable `mGain`.

### Templates

The routing helper is a template:

```cpp
template <typename Sample, typename Model>
void ProcessStereoModels(...);
```

A template is comparable to a type-safe code generator. The same routing logic
can be compiled for:

- `double` samples and a fake test model;
- the plug-in's real sample type and `ResamplingNAM`;
- another compatible model type in the future.

Unlike a C macro, the compiler still checks the types and function calls.

### `std::array`

```cpp
std::array<std::unique_ptr<Model>, 2> mLive;
```

This is a fixed-size array of two elements. It is similar to:

```c
Model* live[2];
```

but it knows its own size and works with other C++ library features.

### `std::unique_ptr`

```cpp
std::unique_ptr<ResamplingNAM> model;
```

A `std::unique_ptr` means one object owns the model. When that owner releases
or replaces it, C++ destroys the model automatically.

The closest manual C pattern is:

```c
ResamplingNAM* model = create_model();
/* use model */
destroy_model(model);
model = NULL;
```

The C++ form reduces leaks and double frees because ownership is explicit.

### `std::move`

```cpp
mStaged[index] = std::move(model);
```

This transfers ownership. After the move, the destination owns the model and
the source no longer does.

Conceptually:

```text
before: local variable owns model
after:  staged slot owns model
```

It does not copy the large neural-network model.

### `nullptr`

```cpp
if (models[channel] == nullptr)
```

`nullptr` is the C++ null-pointer value. It serves the same conceptual purpose
as `NULL`, but has a dedicated pointer-safe type.

### Enumerations with scope

```cpp
enum class ModelSlot : std::size_t
{
  A = 0,
  B = 1
};
```

This creates two named slot values:

```cpp
dualnam::ModelSlot::A
dualnam::ModelSlot::B
```

The names make code clearer than passing raw integers `0` and `1`.

### References

```cpp
WDL_String& slotPath =
  slot == dualnam::ModelSlot::A ? mNAMPathA : mNAMPathB;
```

The `&` here declares a reference. `slotPath` becomes another name for either
`mNAMPathA` or `mNAMPathB`. Assigning to `slotPath` changes the selected real
member.

This avoids duplicating the complete load function for A and B.

## 4. Audio buffers and pointer shapes

Hosts generally pass audio as channel pointers:

```cpp
sample** inputs
sample** outputs
```

This can be visualized as:

```text
inputs
  |
  +-- inputs[0] -> left samples:  L0 L1 L2 L3 ...
  |
  +-- inputs[1] -> right samples: R0 R1 R2 R3 ...
```

`inputs[0][20]` means:

- choose input channel 0;
- read sample frame 20.

The same structure applies to outputs.

A NAM capture is mono, so each NAM expects an array containing one channel
pointer. DualNAM creates a one-channel view for each stereo lane:

```cpp
Sample* modelInput[1]{inputs[channel]};
Sample* modelOutput[1]{outputs[channel]};
models[channel]->process(modelInput, modelOutput, numFrames);
```

No samples are copied by these pointer assignments. The code passes each model
a pointer directly to the correct channel buffer.

## 5. The deterministic stereo-routing seam

The smallest DualNAM-owned DSP unit is:

```cpp
template <typename Sample, typename Model>
void ProcessStereoModels(
  Sample** inputs,
  Sample** outputs,
  const int numFrames,
  Model* const (&models)[kStereoChannels])
{
  for (std::size_t channel = 0; channel < kStereoChannels; ++channel)
  {
    if (models[channel] == nullptr)
    {
      std::fill_n(outputs[channel], numFrames, static_cast<Sample>(0));
      continue;
    }

    Sample* modelInput[1]{inputs[channel]};
    Sample* modelOutput[1]{outputs[channel]};
    models[channel]->process(modelInput, modelOutput, numFrames);
  }
}
```

Read it as an audio patch:

1. Visit channel 0, then channel 1.
2. If that channel has no loaded model, write digital silence.
3. Make a mono input connection to the same-numbered channel.
4. Make a mono output connection to the same-numbered channel.
5. Ask that channel's NAM model to process the block.

The array mapping is:

```cpp
ResamplingNAM* models[2]{
  mModelSlots.Live(dualnam::ModelSlot::A),
  mModelSlots.Live(dualnam::ModelSlot::B)
};
```

Therefore:

```text
channel 0 -> slot A -> channel 0
channel 1 -> slot B -> channel 1
```

There is no pan calculation. The hard-left/hard-right behavior comes from never
crossing channel indices.

### Why unloaded slots are silent

An empty model slot could theoretically pass dry guitar, but that would make a
missing model sound like a valid processed lane. DualNAM instead writes zeros:

```cpp
std::fill_n(outputs[channel], numFrames, static_cast<Sample>(0));
```

This is a deterministic safe failure mode.

## 6. The complete real-time processing path

iPlug2 calls:

```cpp
void NeuralAmpModeler::ProcessBlock(
  sample** inputs,
  sample** outputs,
  int nFrames)
```

once for each audio block.

The current high-level order is:

```text
host inputs
  -> prepare internal buffers
  -> shared input gain
  -> apply pending model changes
  -> noise-gate trigger
  -> independent Input A / Input B gain
  -> NAM A and NAM B
  -> noise-gate gain
  -> independent EQ A / EQ B tone stacks
  -> inherited IR stage, when active
  -> DC-blocking high-pass filter
  -> independent Output A / Output B gain
  -> host outputs
  -> meters
```

The key DualNAM call is:

```cpp
dualnam::ProcessStereoModels(
  triggerOutput,
  mOutputPointers,
  nFrames,
  models);
```

The branch gains are applied into preallocated model-input buffers:

```cpp
dualnam::ApplyStereoInputGains(
  triggerOutput,
  modelInputPointers,
  nFrames,
  mModelInputGains);
```

This placement means the shared Input control and gate detector still see the
common input stage, while `Input A` and `Input B` independently determine how
hard each capture is driven.

### Input copy and shared gain

```cpp
for (size_t channel = 0; channel < nChansOut; ++channel)
{
  const bool inputConnected = channel < nChansIn;
  for (size_t s = 0; s < nFrames; s++)
    mInputArray[channel][s] =
      inputConnected ? mInputGain * inputs[channel][s] : 0.0;
}
```

Each channel is copied independently into its internal lane. Both receive the
same shared gain multiplier, but their sample content is never summed or
duplicated internally.

If Gig Performer supplies:

```text
left  = guitar
right = guitar
```

both models hear the guitar.

If Gig Performer supplies:

```text
left  = guitar 1
right = guitar 2
```

each model hears a different source.

### Output copy and shared gain

```cpp
for (size_t channel = 0; channel < nChansOut; ++channel)
{
  for (auto s = 0; s < nFrames; s++)
    outputs[channel][s] = mOutputGain * inputs[channel][s];
}
```

Again, the gain is shared, but the channels stay separate.

## 7. Two-slot model ownership

The original upstream plug-in had one live model and one staged model. DualNAM
needed two of each:

```cpp
std::array<std::unique_ptr<Model>, 2> mLive;
std::array<std::unique_ptr<Model>, 2> mStaged;
```

The live model is currently processing audio. The staged model is completely
loaded and prepared, waiting to become live.

An audio analogy is a two-deck system:

```text
live deck:    connected to the audience
staging deck: prepared off to the side
changeover:   switch decks at a block boundary
```

### Loading a model

The browser eventually calls:

```cpp
_StageModel(dualnam::ModelSlot::A, fileName);
```

or:

```cpp
_StageModel(dualnam::ModelSlot::B, fileName);
```

The loader:

1. reads the `.nam` file;
2. constructs the NAM DSP object;
3. rejects models that are not mono-in/mono-out;
4. wraps the NAM model in the sample-rate conversion layer;
5. resets and prepares it for the host sample rate and block size;
6. applies the current Slim value when supported;
7. transfers ownership into the selected staged slot;
8. remembers the selected path;
9. tells the correct browser that loading succeeded.

The central ownership transfer is:

```cpp
mModelSlots.Stage(slot, std::move(temp));
```

### Preserving a working model when loading fails

Before loading, the code saves the old path:

```cpp
WDL_String previousNAMPath = slotPath;
```

If parsing or construction throws an error:

```cpp
mModelSlots.CancelStaged(slot);
slotPath = previousNAMPath;
```

The failed candidate is discarded and the previous valid path remains. This is
important because malformed external `.nam` files should not destroy a working
configuration.

### Activating staged models

The loader prepares the model, but the model becomes live through:

```cpp
const auto modelChanges = mModelSlots.ApplyPending();
```

`ApplyPending()` checks each slot independently. A staged model moves into the
matching live slot:

```cpp
mLive[index] = std::move(mStaged[index]);
```

Changing A does not alter B, and changing B does not alter A.

### Clearing a model

The UI does not directly delete the model. It requests removal:

```cpp
mModelSlots.RequestRemove(dualnam::ModelSlot::A);
```

The pending request is applied during processing, and only that slot is reset.

### Latency reporting

Two captures can require different resampling latency. The plug-in currently
reports the larger value:

```cpp
int MaximumLatency() const
{
  int maximum = 0;
  for (const auto& model : mLive)
    if (model != nullptr)
      maximum = std::max(maximum, model->GetLatency());
  return maximum;
}
```

This tells the host the plug-in's worst-case latency. It does not yet delay the
faster internal branch to align it with the slower branch. Internal branch
alignment remains a future production-hardening task.

## 8. The mirrored two-channel editor

The editor width was doubled from 600 to 1200 pixels while retaining the
original 400-pixel height. A single `attachChannelPanel` lambda receives one
600-pixel panel rectangle and creates the same geometry for either channel:

```cpp
attachChannelPanel(leftPanel, "CHANNEL A", ModelSlot::A, kModelAInputLevel, ...);
attachChannelPanel(rightPanel, "CHANNEL B", ModelSlot::B, kModelBInputLevel, ...);
```

Using one layout function is important: changing a control position once keeps
both panels aligned. The first knob in each panel replaces the original shared
Input knob and is connected to that branch's functional `Input A` or `Input B`
parameter. The last knob is similarly connected to `Output A` or `Output B`.

The two model browsers are also functional and independently tagged:

```cpp
new NAMFileBrowserControl(
  modelArea,
  clearModelMessage,
  defaultModelString,
  "nam",
  makeLoadModelCompletionHandler(modelSlot),
  ...);
```

Both use the same generic loader logic. The slot argument determines:

- which staging slot receives the model;
- which path is updated;
- which browser receives success or failure messages;
- which live model is eventually replaced.

This is an example of avoiding duplication without building a large
abstraction.

The inherited noise gate, IR, and slimming controls are not yet independent DSP
paths. Channel A keeps those existing shared controls functional so no
established capability is lost. Matching Channel B controls are visible but
disabled. This makes their intended locations explicit without
misrepresenting them as implemented. The Channel B IR browser follows the same
rule: it is a visual placeholder until a second IR object and state path exist.

### Independent EQ

Each branch owns a separate `BasicNamToneStack`:

```cpp
std::unique_ptr<AbstractToneStack> mToneStacks[2];
```

The routing helper passes only one mono channel to each instance:

```cpp
dualnam::RouteStereoEffects(
  postModelChannels,
  toneStackOutputs,
  numFrames,
  toneStacks,
  toneStackEnabled);
```

EQ A cannot read or modify channel B, and EQ B cannot read or modify channel A.
Each side has an independent switch plus Bass, Middle, and Treble values from
0 to 10, defaulting to 5.

The original shared `ToneStack`, `Bass`, `Middle`, and `Treble` parameter IDs
remain allocated for existing host automation compatibility, but they no
longer control active DSP. State schema 4 stores the eight new EQ parameters.
When an older state is opened, its shared EQ settings are copied to both
branches.

### Independent output gains

The final handoff to the host applies one linear gain per channel:

```cpp
dualnam::ApplyStereoOutputGains(
  processedChannels,
  hostOutputs,
  numFrames,
  mModelOutputGains,
  connectedOutputChannels);
```

`Output A` affects only the left output and `Output B` affects only the right
output. Both range from -40 dB to +40 dB and default to unity at 0 dB.

The original parameter ID 5 named `Output` remains allocated because removing
it would shift or invalidate existing host automation. It is no longer read by
the active DSP and no longer has a knob in the main editor. When schema 1 or 2
state is loaded, its saved value initializes both new output gains so an older
session begins with the same overall level:

```cpp
Output A = legacy Output;
Output B = legacy Output;
```

### Independent meters

Each channel panel has two live mono meters. They do not share stereo meter
data or channel offsets:

```cpp
mInputSenderA.ProcessBlock(inputA, frames, kCtrlTagInputMeterA);
mInputSenderB.ProcessBlock(inputB, frames, kCtrlTagInputMeterB);
mOutputSenderA.ProcessBlock(outputA, frames, kCtrlTagOutputMeterA);
mOutputSenderB.ProcessBlock(outputB, frames, kCtrlTagOutputMeterB);
```

The input meters receive the branch buffers after the independent Input A/B
gain stage. The output meters receive the host output buffers after independent
Output A/B gain. Therefore changing one branch gain produces visual feedback
only on that branch's corresponding meter.

## 9. Saving and restoring host state

A plug-in session normally saves parameters and references to external files.
DualNAM does not embed the neural-network files in the host session. It stores
their paths and reloads them.

### The DualNAM state identity

```cpp
inline constexpr char kHeader[] = "###DualNAM###";
inline constexpr char kLegacyNAMHeader[] =
  "###NeuralAmpModeler###";
inline constexpr char kSchemaVersion[] = "4";
```

The header answers “what kind of state is this?” The schema version answers
“which layout does this state use?”

Schema 2 added `Input A/B`, schema 3 added `Output A/B`, and schema 4 adds
independent EQ A/B settings. Earlier schemas remain readable and migrate shared
values into both branches where appropriate.

### Serialization

```cpp
chunk.PutStr(dualnam::state::kHeader);
chunk.PutStr(dualnam::state::kSchemaVersion);
chunk.PutStr(mNAMPathA.Get());
chunk.PutStr(mNAMPathB.Get());
chunk.PutStr(mIRPath.Get());
return SerializeParams(chunk);
```

The stored order is:

```text
DualNAM header
schema version
model A path
model B path
IR path
all plug-in parameters
```

The order matters. The reader must consume fields in the same order in which
the writer produced them.

### Restoration

When a host restores state, DualNAM:

1. reads the header;
2. chooses the DualNAM or legacy reader;
3. reads model paths and parameter values;
4. restores parameters;
5. stages model A when its path is present;
6. stages model B when its path is present;
7. restores the inherited IR when its path is present.

The controlled Gig Performer test confirmed this workflow across save, close,
reopen, and different capture selections.

### Legacy single-model compatibility

Older upstream NAM state contains only one model path. DualNAM maps it to A:

```cpp
static ModelPaths FromLegacy(std::string legacyPath)
{
  return {std::move(legacyPath), {}};
}
```

This means:

```text
old NAM path -> DualNAM slot A
slot B       -> empty
```

The old state is read, but new state is written with the DualNAM header.

## 10. Unique plug-in identity

Hosts do not identify a plug-in only by its filename. They use format-specific
IDs and bundle metadata. Reusing upstream NAM identifiers could make DualNAM
replace, collide with, or be mistaken for official NAM.

The host-facing identity is now:

```cpp
#define PLUG_NAME "DualNAM"
#define PLUG_MFR "Moses Da Silva"
#define PLUG_VERSION_STR "0.1.0"
#define PLUG_UNIQUE_ID 'DuNM'
#define PLUG_MFR_ID 'MoDa'
#define BUNDLE_NAME "DualNAM"
#define BUNDLE_MFR "mosesdasilva"
```

The installed bundles are:

```text
~/Library/Audio/Plug-Ins/VST3/DualNAM.vst3
~/Library/Audio/Plug-Ins/Components/DualNAM.component
```

Their bundle identifiers are:

```text
com.mosesdasilva.vst3.DualNAM
com.mosesdasilva.audiounit.DualNAM
```

The Audio Unit identity is:

```text
type:         aufx
subtype:      DuNM
manufacturer: MoDa
```

The internal C++ class and Xcode target still use upstream names such as
`NeuralAmpModeler`. That is acceptable because hosts see the external bundle
metadata, not the source filename. Avoiding a broad internal rename kept the
functional change reviewable.

## 11. Test-driven development used in this project

Each new behavior was first represented by a focused executable test.

### Routing test

The test uses small generated buffers:

```cpp
std::array<double, 4> leftInput{1.0, 2.0, 3.0, 4.0};
std::array<double, 4> rightInput{10.0, 20.0, 30.0, 40.0};
```

Model A multiplies by `2.0`; model B multiplies by `-0.5`:

```cpp
GainModel modelA(2.0);
GainModel modelB(-0.5);
```

The assertions prove:

```text
left output  = 2.0  * left input
right output = -0.5 * right input
```

Because the input patterns and gains differ, an accidental channel swap or
crossfeed would fail visibly.

The unloaded-slot test initializes the right output with nonzero values and
then verifies the routing helper overwrites it with zero. This proves silence
is actively produced rather than assumed.

### Model-slot test

Fake models have an identity and latency:

```cpp
class FakeModel
{
public:
  explicit FakeModel(int identity, int latency = 0);
  int Identity() const;
  int GetLatency() const;
};
```

The tests prove:

- A and B activate independently;
- clearing A leaves B alive;
- the reported latency is the maximum live-branch latency.

### State test

The state tests prove:

- DualNAM writes its own header;
- the DualNAM and upstream headers are different;
- legacy state maps only to A;
- A and B paths remain independent.

### Identity test

The shell test verifies stable host-facing contracts:

```sh
grep -q '^#define PLUG_NAME "DualNAM"$' config.h
grep -q "^#define PLUG_UNIQUE_ID 'DuNM'$" config.h
grep -q "^#define PLUG_MFR_ID 'MoDa'$" config.h
```

This protects against accidentally reverting to upstream NAM identity.

## 12. Build and validation path

Focused code tests:

```sh
./scripts/test-routing.sh
./scripts/test-plugin-identity.sh
```

Complete local macOS build and AU validation:

```sh
./scripts/verify-baseline-macos.sh
```

Despite its historical filename, this script now validates the current DualNAM
development build. It:

1. checks required tools and dependencies;
2. runs the identity contract;
3. builds universal VST3 and AUv2 bundles;
4. verifies executable files exist;
5. validates both property lists;
6. verifies bundle IDs and AU component IDs;
7. confirms both `x86_64` and `arm64` architectures;
8. runs Apple's complete `auval` suite.

Apple validation passed for:

```text
Audio Unit name: DualNAM
manufacturer:    Moses Da Silva
type/subtype:    aufx / DuNM
manufacturer ID: MoDa
version:         0.1.0
```

## 13. What the controlled Gig Performer test proved

The manual host test added evidence that unit tests and `auval` cannot provide.
It confirmed:

- the plug-in appears and opens in Gig Performer;
- two different `.nam` captures can be selected;
- A and B can be changed independently;
- each slot can be unloaded and loaded;
- the plug-in continues processing correctly;
- the rackspace/session saves;
- Gig Performer can close;
- the saved session reopens;
- both model selections restore;
- inherited EQ works;
- inherited noise gate works;
- shared input behavior works;
- the resulting workflow satisfies the original practical requirement.

This is now the stable functional checkpoint.

## 14. Git history of the DualNAM work

The project was developed in small commits:

```text
014d5f3 Add stereo model routing foundation
e9ec01c Add dual model slot lifecycle
aff38e2 Add independent model loading and state
b93544e Assign unique DualNAM plugin identity
```

Each commit represents a distinct concept:

### Stereo routing foundation

Created the testable two-channel routing seam and proved left/right isolation
and silent unloaded slots.

### Dual model slot lifecycle

Added ownership for two live and two staged model instances, independent
activation/removal, and maximum latency reporting.

### Independent loading and state

Added separate browsers, paths, loading, clearing, a DualNAM state schema, and
legacy state migration.

### Unique plug-in identity

Changed the external product name, bundle names, component IDs, bundle IDs,
version, generated metadata, and validation expectations.

## 15. Important real-time audio concepts

### The audio callback has a deadline

`ProcessBlock()` must finish before the audio interface needs the next block.
Missing the deadline produces clicks, dropouts, or host instability.

Therefore the callback should avoid:

- disk access;
- constructing a neural network;
- blocking locks;
- console logging;
- unpredictable work;
- heap allocation and object destruction where possible.

### Staging separates preparation from use

Loading and preparing a `.nam` capture is expensive. The model is constructed
before becoming live, then ownership is switched at a block boundary.

This is analogous to preparing a new effects rack before switching the audio
patch to it.

### Current known real-time limitation

The current `ApplyPending()` implementation can destroy the previous
`std::unique_ptr` when replacing or removing a live model:

```cpp
mLive[index].reset();
```

or:

```cpp
mLive[index] = std::move(mStaged[index]);
```

Because `_ApplyDSPStaging()` currently runs from `ProcessBlock()`, destruction
of the previous neural model can occur on the audio thread. It worked in the
controlled test, but it remains a production-hardening issue because model
destruction may release heap memory and perform unbounded cleanup.

A future increment should transfer retired models to a non-audio-thread cleanup
queue.

## 16. Current behavior versus future work

### Confirmed working

- VST3 and AUv2;
- stereo input and stereo output;
- left to model A to left;
- right to model B to right;
- two independent `.nam` files;
- independent loading, changing, and clearing;
- silent empty slots;
- saved and restored A/B paths;
- legacy single-model state mapped to A;
- shared input and output gain;
- independent pre-model Input A and Input B gain parameters;
- independent post-processing Output A and Output B gain parameters;
- independent input and output meters for Channel A and Channel B;
- independent EQ switches and Bass/Middle/Treble controls for both branches;
- mirrored 1200x400 Channel A/Channel B editor;
- functional model browser and branch input control on both panels;
- disabled Channel B placeholders for future independent processing;
- inherited EQ and noise gate;
- inherited IR functionality;
- unique DualNAM host identity;
- universal Intel/Apple Silicon bundles;
- Apple AU validation;
- controlled Gig Performer workflow.

### Not yet implemented

- selectable stereo/mono-duplicated input mode;
- internal delay compensation when A/B branch latencies differ;
- off-audio-thread retired-model destruction;
- release signing and notarization;
- formal release packaging.

### Not yet fully validated

- VST3 `pluginval`;
- CPU measurements with two representative captures;
- systematic sample-rate matrix at 44.1, 48, and 96 kHz in Gig Performer;
- systematic buffer-size matrix from 32 through 1024 samples;
- long-duration stress testing during repeated model changes.

## 17. How to explain the project to another person

A concise technical explanation is:

> I forked the official Neural Amp Modeler plug-in and kept its real NAM DSP
> engine and iPlug2 host integration. I changed the plug-in architecture from
> one mono NAM model to two independently owned mono NAM models inside a stereo
> effect. The left host channel is routed only through model A and returned
> only on the left; the right channel does the same through model B. I added
> separate model browsers, safe staged loading, independent clearing, dual-path
> session restoration, legacy state handling, and unique VST3/AUv2 identities
> so DualNAM is a separate plug-in. I added deterministic unit tests, universal
> macOS builds, Apple AU validation, and completed a save/reopen test in Gig
> Performer with two different captures.

A shorter musician-facing explanation is:

> DualNAM puts two NAM captures in one stereo plug-in. The left input uses
> capture A and stays left; the right input uses capture B and stays right. In
> Gig Performer I can duplicate one guitar into both inputs, load two different
> captures, and save the entire setup in one rackspace.

## 18. Recommended next development order

The next functional increment should add independent A/B gains because that is
the final major control from the original product contract.

After that:

1. move retired-model destruction off the audio thread;
2. add A/B branch delay compensation;
3. run `pluginval`;
4. measure CPU and latency with representative captures;
5. test the sample-rate and buffer-size matrix;
6. create a signed/notarized release process only when public distribution is
   intended.

The existing stable checkpoint should remain easy to return to while those
changes are developed.
