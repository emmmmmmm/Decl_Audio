## Roadmap

This roadmap separates committed milestones from backlog ideas.

The foundation through Phase 8 is in place. Phase 9 is still active hardening work. Phase 10 is the next committed feature milestone. Everything after that is intentionally kept looser until the execution model and hardening work settle.

**Completed Foundation**

**Phase 0 - Scaffolding**
- [x] repo layout, build/test flow, queue foundation

**Phase 1 - Compiler + CompiledBank**
- [x] authoring parse/validation
- [x] lower to immutable compiled data
- [x] `LoadBehaviors()`

**Phase 2 - Control-side world state**
- [x] entity/tag/value state
- [x] host->control command handling

**Phase 3 - AssetBank**
- [x] asset discovery
- [x] preload/decode
- [x] missing asset diagnostics

**Phase 4 - Audio-side playback**
- [x] oneshot / loop / random playback
- [x] instance lifecycle on audio side
- [x] stub backend execution

**Phase 5 - Resolver**
- [x] match tags + conditions
- [x] create on match, stop on unmatch

**Phase 6 - Audible output**
- [x] miniaudio backend
- [x] direct output mixing
- [x] startup audio config

**Phase 7 - Runtime forwarding**
- [x] reserved runtime values `volume` / `position`
- [x] control-side forwarding to bound instances

**Phase 7.5 - Spatialization**
- [x] programwide spatialization settings
- [x] listener position
- [x] pan + distance attenuation

**Phase 8 - Debugging + CLI**
- [x] debug snapshot
- [x] interactive sandbox CLI
- [-] ring buffer trace log

**Phase 9 - Stabilization and Contracts**

This is still active. It is not glamorous work, but it should be finished before the runtime gets structurally more complex.

- [x] RT safety audit of the callback path
- [x] document and test fail-loudly behavior at instance-capacity exhaustion
- [x] document and test the `Update()` cadence contract
- [ ] profiling pass at realistic instance counts
- [-] fold the remaining trace logging work in here if it still feels useful after the audit

Audit note:
- RT-safe as-is: `MiniaudioBackend::DataCallback()` forwards directly to `AudioRuntime::Render()`, which zeroes the output buffer, drains a fixed-capacity SPSC `RingBuffer`, and renders from preallocated runtime state plus immutable bank/asset data. No locks, filesystem, parsing, or logging appear on that callback path.
- Safe but potentially expensive: the callback drains the entire pending audio-command queue at block start, `FindInstanceIndex()` is linear for per-instance updates, output and scratch buffers are cleared every block, and spatialization does trig work once per active instance per block.
- Follow-up risks: bursty `Update()` traffic can stack several linear scans into one callback, the RT story depends on `AudioCommand` staying heap-free and the runtime staying preallocated, and there is still no profiling envelope for worst-case block cost.

Capacity note:
- Current MVP policy is fail-loudly, not virtualization: if `CreateInstance` would exceed `AudioRuntime::max_instances`, the audio thread terminates immediately instead of silently dropping, stealing, or virtualizing a voice.
- The tests cover this with a subprocess death case so the behavior is locked down without weakening the runtime path with recovery code.

**Testable:** no RT-safety findings, capacity failure is explicit, missed-`Update()` behavior is documented and covered by tests, and the profiler gives a believable cost envelope.

**Phase 10 - Runtime Execution Tree**

This is where nested containers belong. It is the next real feature milestone, not design-doc bulk.

- [ ] lock semantics for tree nodes before the refactor starts
- [ ] `select` chooses once on entry and does not switch branches mid-run
- [ ] `blend` keeps both child subtrees advancing and mixes from the current runtime value
- [ ] keep `volume` and `position` as dedicated runtime commands
- [ ] compile programs as node trees instead of flat container lists
- [ ] replace `cursor + current` with a root execution context per `ProgramInstance`
- [ ] add program-local runtime parameter slots for nodes that need live values
- [ ] port existing leaf behavior (`oneshot`, `loop`, `random`) onto the new executor with no behavior change
- [ ] ship `select`
- [ ] ship `blend`
- [ ] extend debug/introspection so nested state is inspectable

**Testable:** all existing flat-program tests still pass on the tree executor; nested `select` chooses on entry only; nested `blend` stays in sync and reacts to runtime parameter updates block-accurately.

**Candidate Milestones After Phase 10**

These are plausible next milestones, but they should stay flexible until Phase 10 lands.

**Reloading + Bank Lifecycle**
- `ReloadBehaviors(path)` as a hard audio reset with world-state preservation
- re-resolve current world state after reload
- if additive bank management still feels necessary after reload ships, fold `LoadAdditionalBank()` / `UnloadBank()` into the same area instead of treating them as a separate giant phase

**Voice Budget / Virtualization**
- fixed `maxInstances` contract in `EngineConfig`
- choose reject vs steal policy
- cheap skip-mix / out-of-range behavior only if profiling proves it matters

**Platform Bring-up**
- Linux CI + binary + backend verification
- macOS CI + binary + backend verification

**Parking Lot**

These are real topics, but they are not committed milestones yet.

- multichannel output policy beyond stereo
- multi-listener support
- streaming
- dedicated control-thread ownership instead of host-driven `Update()`
- richer DSP / bus features

If one of these gets promoted, it should do so because a concrete use case forces it, not because it looked reasonable in a long numbered list.
