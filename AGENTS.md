# DualNAM agent instructions

These instructions apply to the entire repository. They combine the project's
working agreement with an XP/TDD/CI workflow for AI-assisted development.

## Product contract

DualNAM is a macOS VST3/AUv2 effect based on the official Neural Amp Modeler
plug-in and core.

- Purpose: process two independently routed guitar channels through two
  independent NAM models in one stereo plug-in.
- Primary user: the project owner running DualNAM inside Gig Performer on
  macOS.
- Supported environment: Apple Silicon macOS with the repository-pinned NAM
  core, iPlug2, Eigen, and VST3 SDK versions.
- Current maturity: development bootstrap; no DualNAM DSP release exists yet.
- Critical properties: real-time safety, deterministic stereo isolation,
  host/session compatibility, model-state integrity, and coexistence with the
  official NAM plug-in.
- Input is stereo.
- Input left feeds NAM model A.
- Input right feeds NAM model B.
- Model A outputs only to the left channel.
- Model B outputs only to the right channel.
- Model loading and gain controls are independent.
- Gig Performer may duplicate a mono source to both inputs externally.
- A future mode switch may select stereo or mono-duplicated input, but do not
  implement or assume its mono-source rule until it is explicitly designed.
- Version 1 excludes capture/training, IR, EQ, noise gate, AAX, AUv3, and the
  standalone app unless the project plan is changed explicitly.

Read `docs/IMPLEMENTATION_PLAN.md` before behavior or architecture changes.
The product owner is the source of truth for intended behavior. The
implementation plan is the current written specification. Executable tests are
the source of truth for behavior already implemented.

## Role of the agent

Treat AI-assisted development as full-time pair programming, not autonomous
code generation.

- Inspect the actual code and tests. Do not trust issue, prompt, or PR summaries
  as proof of behavior.
- State assumptions when requirements are ambiguous.
- Ask before broad refactors, architecture changes, migrations, dependency
  upgrades, identifier changes, publishing, or irreversible operations.
- The human decides product behavior, tradeoffs, merges, releases, and
  exceptions to these rules.
- Never describe generated code as correct merely because it compiles.
- Never claim certainty beyond the available code, test, build, and runtime
  evidence.

## Standard workflow

1. Read the relevant code, tests, documentation, configuration, submodule
   versions, and recent history.
2. Restate the goal, constraints, assumptions, and observable acceptance
   criteria.
3. For non-trivial work, provide a short plan before editing.
4. Create the smallest failing test for the next behavior.
5. Implement the minimum change that makes it pass.
6. Refactor while tests remain green.
7. Run narrow checks first, then the complete available verification path.
8. Review the actual diff against the original goal and identified risks.
9. Update affected documentation and workflow instructions.
10. Report changed, verified, not verified, risks, and follow-ups.

## Work in small vertical increments

- Implement one observable behavior or one refactoring objective at a time.
- Keep behavior changes, refactoring, dependency changes, and unrelated cleanup
  in separate increments when practical.
- Prefer the simplest implementation that passes the current tests and meets
  the product contract.
- Avoid speculative abstractions and features for possible future use.
- Keep the repository runnable after every completed increment.
- Make commits small, coherent, and safe to revert. Do not commit unless asked.
- Do not hide incomplete work behind silent fallback behavior, broad exception
  handling, or vague TODO comments.
- Never prefix branch names with `codex/`.
- Name branches as `<type-of-work>/<short-description>`, for example
  `gui/vector-global-strip` or `fix/model-slot-state`.

## TDD: red, green, refactor

Use test-driven development for new behavior and defect fixes.

1. Red: add the smallest test that expresses the required behavior and confirm
   it fails for the expected reason.
2. Green: write the minimum production code needed to pass that test.
3. Refactor: remove duplication and improve names and structure while the tests
   remain green.

Additional rules:

- Every bug fix requires a regression test that fails before the fix.
- Every new branch, routing rule, state transition, or parameter behavior
  requires focused test coverage.
- Do not weaken, delete, skip, or rewrite a valid test merely to make a change
  pass.
- If a behavior cannot yet be automated, explain why, document a reproducible
  manual test, and identify the seam needed to automate it later.
- Tests must be fast, independent, repeatable, self-validating, and timely.
- Prefer deterministic generated sample buffers and purpose-built fake models
  over external audio hardware in automated tests.
- Keep host/plugin integration tests separate from fast DSP unit tests.

## DualNAM test priorities

The safety net must cover at least:

- left input reaches model A only;
- right input reaches model B only;
- A reaches left output only and B reaches right output only;
- unloaded slots are silent;
- independent A/B gains have the expected linear/dB behavior;
- shared input/output gain behavior is deterministic;
- loading or clearing one slot does not alter the other;
- state serialization restores both model paths and gains;
- processing is stable across block boundaries and variable block sizes;
- sample-rate conversion and reported latency are correct;
- branches remain time-aligned when model/resampler latencies differ;
- no allocation, file I/O, model construction, blocking lock, exception, or
  logging occurs in the audio callback.

For audio assertions, use explicit tolerances and explain why the tolerance is
appropriate. Do not compare floating-point buffers with unexplained magic
numbers.

## Continuous integration and feedback

- There must be one documented command for the complete local verification
  path. Until DualNAM-specific tests and plug-in validation are added, do not
  claim the project has complete CI.
- Every proposed change should pass the same checks CI runs.
- CI should run on pushes and pull requests and fail on:
  - compilation errors;
  - test failures;
  - formatting violations;
  - static-analysis warnings selected by the project;
  - invalid AU/VST3 bundles or failed plug-in validation.
- Keep release automation separate from CI. CI validates changes; releases are
  explicit, versioned, and tag-triggered.
- Cache dependencies/build products where safe, but never reuse stale generated
  artifacts as test evidence.
- Build release artifacts once per architecture and reuse those exact artifacts
  for validation and packaging.

Current bootstrap check:

```sh
./scripts/check-macos-toolchain.sh
```

Focused stereo-routing test:

```sh
./scripts/test-routing.sh
```

Verified upstream baseline build and validation:

```sh
./scripts/verify-baseline-macos.sh
```

The script builds the `macOS-VST3` and `macOS-AUv2` schemes from
`NeuralAmpModeler/projects/NeuralAmpModeler-macOS.xcodeproj`, verifies both
universal bundles, and runs Apple's AU validator.

A passing suite reduces risk; it does not prove correctness. If a required
check cannot run, state the exact blocker and the behavior left unverified.

## Visual inspection ownership

- The product owner performs all visual inspection of the plug-in GUI.
- The agent must not open, inspect, capture, screenshot, or judge the rendered
  plug-in interface unless the product owner explicitly requests an exception.
- For GUI changes, the agent may verify source-level layout assertions,
  compilation, bundle integrity, plug-in validation, and non-visual behavior.
- Report visual appearance, alignment, spacing, typography, color, and usability
  as not verified until the product owner provides their manual assessment.
- Do not treat a successful build or automated layout check as evidence that a
  GUI looks correct.

## GUI asset workflow

Use an SVG-first workflow for DualNAM front-end work.

- The product owner designs visual GUI components in a vector design tool and
  exports them as SVG assets.
- Store approved SVG assets in `NeuralAmpModeler/resources/img/` with
  descriptive `DualNAM-*` filenames.
- Prefer rendering those SVG assets directly in iPlug2 controls instead of
  recreating panels, knobs, switches, meters, selector bars, icons, or other
  artwork with hand-coded C++ drawing primitives.
- Use C++ for behavior and integration: positioning, parameter binding, dynamic
  text labels and values, knob rotation, switch state, meter level repetition,
  file-browser behavior, enable/disable state, and layout grouping.
- Keep reusable DualNAM-owned GUI behavior in project controls such as labeled
  SVG knobs, SVG switches, selector controls, and dynamic SVG meters.
- When a component should change visually, update or replace the SVG asset
  first; change C++ drawing code only when runtime behavior cannot be expressed
  by positioning, transforming, or repeating existing SVG assets.
- Normalize exported SVGs only when needed for predictable runtime scaling,
  cropping, or coordinate handling, and document the reason in the change
  report.
- Keep source-level GUI assertions updated when adding assets or changing layout
  contracts, but leave final visual approval to the product owner.

## Code design for agent readability

- Give each function one job and each module one responsibility.
- Prefer short functions. Treat 4-20 lines as a useful target, not a reason to
  fragment cohesive DSP math.
- Keep source files under roughly 500 lines where practical; split by
  responsibility before files become difficult to inspect.
- Use specific, searchable names. Avoid vague names such as `data`, `handler`,
  `manager`, `temp`, or `process` without domain context.
- Use explicit C++ types and ownership. Prefer RAII and `std::unique_ptr` for
  exclusive ownership.
- Avoid global mutable state, hidden ownership, and implicit cross-module
  dependencies.
- Inject replaceable dependencies through constructors or parameters.
- Wrap third-party behavior behind the smallest project-owned seam needed for
  testing; do not duplicate or broadly rewrite upstream NAM/iPlug2 code.
- Remove real duplication. Do not create abstractions merely because two short
  blocks look similar.
- Prefer early returns and shallow control flow. Avoid nesting beyond two
  levels where a clearer decomposition is possible.
- Preserve upstream conventions in vendored/submodule code. Put DualNAM-owned
  behavior in project-owned files when practical.
- Use the project's standard formatter and static-analysis configuration. Do
  not introduce a competing style system.

## Real-time audio constraints

- The audio callback must be deterministic and real-time safe.
- Preallocate buffers during initialization/reset, never during processing.
- Stage model loading outside the active audio path and swap prepared state only
  at a safe block boundary.
- Do not use blocking mutexes, filesystem access, console output, heap
  allocation, or unbounded work in `ProcessBlock`.
- Make channel counts, sample rates, block sizes, and latency assumptions
  explicit.
- On invalid runtime state, prefer a deterministic safe output such as silence
  over undefined behavior.
- Measure CPU and latency with two representative NAM models before calling a
  DSP increment complete.

## Compatibility and persisted state

- Assume DAW sessions, Gig Performer rackspaces, automation mappings, parameter
  indices, serialized paths, and plug-in identifiers become public contracts
  once a DualNAM build is used.
- Identify every persisted-state or host-facing contract touched by a change.
- Preserve parameter ordering and stable identifiers unless an intentional
  migration is approved.
- New state formats require an explicit version, backward-read strategy where
  practical, failure behavior, and rollback plan.
- Never silently discard, reinterpret, or overwrite model paths or parameter
  values.
- Test save/reload and upgrade behavior when serialization changes.
- Breaking compatibility requires explicit human approval and documentation.

## Comments and documentation

- Preserve useful existing comments during refactoring.
- Comments explain why, constraints, provenance, or non-obvious DSP decisions;
  they do not narrate obvious syntax.
- Reference an upstream issue, commit, or specification when code exists for a
  particular compatibility constraint.
- Public interfaces require concise documentation of intent and usage.
- Update `docs/IMPLEMENTATION_PLAN.md`, README/build instructions, and validation
  commands when behavior or workflow changes.
- User-facing documentation starts with the problem solved and how to use it,
  not with a list of frameworks and dependencies.

## Errors and diagnostics

- Error messages include the offending value/path and the expected condition.
- Keep diagnostics off the real-time thread.
- Do not catch an error only to ignore it.
- Distinguish user/model-file errors from programmer invariants.
- Never expose secrets, signing credentials, or local private paths in tracked
  configuration. Track example configuration and ignore real credentials.

## Security and external input

- Treat `.nam` files, restored state, host-provided channel layouts, file paths,
  and parameter values as untrusted input.
- Validate channel counts, model topology, paths, sizes, sample rates, block
  sizes, and numeric ranges at system boundaries.
- Malformed or unsupported models must fail safely without crashing the host or
  replacing a previously valid slot.
- Error messages may include the offending local path for the user, but
  diagnostics and published artifacts must not leak unrelated private paths.
- Use established libraries for parsing and platform security. Do not invent
  cryptography, signing, or sandbox mechanisms.

## Dependencies and upstream code

- Verify the checked-out version and actual API before coding against NAM core,
  iPlug2, Eigen, the VST3 SDK, or Apple frameworks.
- Prefer upstream source, primary documentation, and pinned interfaces over
  memory or examples from other versions.
- Keep submodules pinned. Do not update, replace, or fork dependencies without
  explicit approval and a compatibility/build rationale.
- A new dependency must justify its maintenance, binary-size, performance,
  licensing, security, and distribution cost.
- Record important upstream constraints and license obligations in project
  documentation.

## Refactoring and review

- Refactor continuously after tests are green; do not accumulate cleanup into a
  large rewrite.
- Before handoff, inspect the actual diff for:
  - dead code;
  - duplicated logic;
  - vague names;
  - unexplained constants;
  - ownership or lifetime risks;
  - real-time safety violations;
  - missing tests;
  - stale documentation.
- Review behavior independently from the author's summary.
- Do not mix broad formatting churn with functional changes.
- For large diffs, review and validate by subsystem rather than trusting one
  broad test run or prose summary.

## Releases

- Favor small, reversible releases over large batches.
- A release requires green CI, versioned release notes, documented installation,
  and checksums for distributed artifacts.
- Maintain a changelog when the first distributable DualNAM version is created.
- macOS public distribution requires unique plug-in identifiers, signing, and
  notarization. Do not reuse the upstream NAM identity.
- Never tag, publish, sign, notarize, or deploy without explicit approval.

## Definition of done

A change is done only when:

- its observable acceptance criteria are satisfied;
- the implementation is scoped and understandable;
- the driving test failed first for the expected reason, when TDD applies;
- relevant tests, formatting, static analysis, builds, and integration checks
  pass;
- real-time safety, compatibility, state, security, performance, licensing,
  and host impact were considered where relevant;
- affected documentation is current;
- the final diff contains no accidental scope expansion;
- open risks and unverified assumptions are explicit.

Never claim success when required validation was skipped or blocked.

## Required completion report

End each task with:

### Changed

- Behavior and files changed.
- The red test that drove the change, or why TDD did not apply.

### Verified

- Automated and manual commands/checks run, with results.

### Not verified

- Checks not run and the exact reason, or `None`.

### Risks and follow-ups

- Remaining risks, decisions, compatibility concerns, or `None`.

## Method references

These rules are adapted for DualNAM from Fabio Akita's guidance on clean code
for agents, XP/TDD/continuous integration, iterative production software, and
minimum open-source project hygiene:

- <https://akitaonrails.com/2026/04/20/clean-code-para-agentes-de-ia/>
- <https://akitaonrails.com/2026/03/01/software-nunca-esta-pronto-4-projetos-a-vida-pos-deploy-e-por-que-one-shot-prompt-e-mito/>
- <https://akitaonrails.com/2026/05/30/boas-praticas-projetos-codigo-aberto-llm-o-minimo/>
