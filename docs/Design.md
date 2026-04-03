# Decl_Audio - Design Document

---

## 0. Quick Orientation (start here if lost)

This audio engine does one thing: **keeps gameplay code out of audio decisions**.

Game code says *what is happening in the world*. The engine decides *what should be playing*.

**The three things that exist:**

```
CompiledBank      immutable blueprints - loaded once, never touched at runtime
AssetBank         immutable decoded audio - loaded once, read freely
ProgramInstance[] live running things   - created/destroyed by audio thread on command
```

**The full pipeline:**

```
[Host Thread]
  "entity X is grounded, speed = 4.2"
        ↓  command queue
[Control Thread]
  updates WorldState (entities, tags, values)
  runs BehaviorResolver -> decides which instances should exist
  mints InstanceIds, sends commands to audio thread
        ↓  command queue (thin, timestamped)
[Audio Thread]
  owns ProgramInstance[]
  applies commands (create, set param, set position, stop)
  each block: fills output buffer via container execution
        ↓
[Output]
```

**Two handoffs:**
- Host -> Control: game submits world facts
- Control -> Audio: thin commands, timestamped

**Who owns what:**
- Control owns: *why* and *what* (matching, instance decisions, param values)
- Audio owns: *execution* (playheads, container transitions, mixing)
- Entities never cross the thread boundary. Control resolves entity->behavior mapping and sends the result, not the inputs.

**Distribution:**
The engine is distributed as a compiled shared library (.dll / .so). The public interface is a C API defined in a single header. C++ wrappers over that API are provided for convenience but are not the contract.

**Architecture:**
```cpp
Engine
  │
  ├── host->control queue        (lock-free)
  │
  ├── Control                   (thread + WorldState + Resolver)
  │     │
  │     └── control->audio queue (lock-free)
  │
  └── Audio                     (thread + ProgramInstances + Mixer)
```


---

## 1. Purpose

DeclarativeSoundEngine moves moment-to-moment audio decisions out of gameplay code and into a dedicated audio system.

Game code publishes facts about the world:
- which entities exist, and which tags they currently have
- which float values they currently expose
- which one-shot events occurred this frame

The audio engine decides:
- which behaviors match
- which sounds should start, stop, or continue
- which parameters apply
- how those sounds are mixed and spatialized

The core idea is not "data-file-driven audio." The real idea is:

> gameplay code declares state, and the audio engine resolves intent.

---

## 2. Goals

- Keep gameplay-side audio integration small and predictable.
- Make audio behavior data-driven and inspectable.
- Make runtime behavior deterministic and testable.
- Keep the real-time audio path safe: no locks, no allocations, no filesystem access, no parsing, no logging.
- Support reloading and tooling without contaminating the mixer.
- Be embeddable as a library, not a singleton demo.

---

## 3. Non-Goals For MVP

- inheritance / overrides in behavior files
- fully general expression language
- dynamic graph mutation at runtime
- overlapping containers within a program (one active container at a time)
- advanced bus scripting
- streaming large assets
- fancy DSP chains
- editor UI

If a feature makes the runtime model ambiguous or unsafe, it does not belong in MVP.

---

## 4. Core Principles

**Separate authoring from runtime.** json source is never interpreted at runtime. It is compiled into blueprints before use.

**Compiled data is immutable.** `CompiledBank` and `AssetBank` are read-only after load. Both threads can read them freely without synchronization.

**The audio thread executes, it does not decide.** It runs compiled programs against immutable data. It never evaluates match conditions, inspects entity state, or makes behavioral decisions.

**Template / instance split at every level.** Compiled data is the blueprint. Instance data is the runtime state. These are always separate structs.

**Determinism beats cleverness.** The runtime should be explainable at any point: why a behavior matched, why an instance was created, what a container is doing right now.

---

## 5. System Layers

### Authoring / Compiler
- Parses json authoring files
- Builds an authoring IR with source locations and unresolved names
- Validates schema and semantics
- Resolves references, interns names into dense IDs
- Lowers behaviors, programs, containers, and conditions into `CompiledBank`

This layer can allocate, parse, and log freely. For MVP it runs inside `LoadBehaviors()`. An offline build step is optional.

### Control Thread
- Receives commands from the host
- Maintains `WorldState` (entities, tags, values)
- Runs `BehaviorResolver` against `CompiledBank`
- Decides which `ProgramInstances` should exist
- Sends thin timestamped commands to the audio thread

### Audio Thread
- Owns all `ProgramInstances`
- Applies incoming commands at correct sample offsets
- Each block: iterates instances, fills output buffer via container execution
- Handles all playback-driven transitions (container exhausted -> advance cursor)
- Spatializes and mixes to output
- Must be real-time safe at all times

---

## 6. Threading Model

### Host Thread
Owned by the game. Submits world facts, inspects debug output. Never touches internal engine state directly.

### Control Thread
Owned by the engine. Drains host commands, mutates `WorldState`, runs resolver, sends playback commands.

### Audio Thread
Owned by the audio backend callback. Owns `ProgramInstance[]`. Executes compiled programs. Mixes output.

**For MVP:** control remains host-driven via explicit `Update()` calls. Internal control-thread ownership is a conceptual boundary, not a requirement to spin up a separate worker thread yet.

---

## 7. The Command Interface (Control -> Audio)

The command set is intentionally thin. Control does all the semantic work before sending anything.

```
CreateInstance(instanceId, programId, position, volume)
SetVolume(instanceId, value)
SetPosition(instanceId, vec3)
SetListenerPosition(vec3)
RequestStop(instanceId)
```

That's essentially it.

**On param updates:** when entity X's values change, control looks up which instances are bound to entity X and sends targeted commands to each. Entity structs never cross the boundary - only their effects do, as targeted commands to specific instances.

**On source kinds:** control may spawn either fire-and-forget instances or entity-bound instances. Fire-and-forget instances carry their initial snapshot on `CreateInstance` and receive no further updates. Entity-bound instances keep a control-side binding so `SetVolume` / `SetPosition` can be forwarded while the match remains active. The audio thread still only sees instance ids and commands; it never sees entities.

**On default instance params:** every instance always has a runtime `volume` and `position`. They default to `1.0` and `Vec3{}`. Authored container `volume` remains a compiled blueprint gain baked into the program. Runtime forwarding updates the instance-level values on top of that.

**On listener state:** listener state is global engine state, not entity state. The host submits it through `SetListenerPosition()`. Control forwards it to audio through the same command path as all other audio updates, so listener changes are block-accurate at MVP timing just like source position and volume changes.

**On reserved runtime values:** `volume` and `position` are reserved entity-side runtime values. They do not need to be declared in authored `parameters`, and they are not valid match-condition inputs. `SetValue(engine, entity, "volume", x)` updates instance volume. `SetPosition(engine, entity, x, y, z)` updates instance position.

**On spawn vs change:** control applies the current `volume` and `position` values when the instance is created if the entity already has them. After that, control only sends `SetVolume` / `SetPosition` when those runtime values actually change.

**On timing:** for MVP, commands are block-accurate only. Params that drift slowly (health, speed, distance) are applied at the next block boundary. Sample-offset timestamps are a later addition once the real backend is in place.

**On stage transitions:** the audio thread handles all playback-driven transitions autonomously (container exhausted -> advance to next). Control only sends world-driven transitions (`RequestStop` when a match condition is lost). No return signalling needed.

**On stale commands:** if control sends `SetVolume`, `SetPosition`, or `RequestStop` for an instance that has already retired on the audio side, the command is dropped. Finished instances clean themselves up on the audio side.

---

## 8. Data Model

Four distinct kinds of data, with very different lifetimes:

### AuthoringDocument - parsed but unresolved
Created immediately after json parse. This is compiler-only data, never touched by runtime threads.

```cpp
struct AuthoringCondition {
    std::string parameter;
    ComparisonOp op;
    float literal;
};

struct AuthoringContainer {
    AuthoringContainerType type;     // OneShot, Loop, Random, Sequence
    std::vector<std::string> assets; // paths or asset ids, unresolved
    std::vector<AuthoringContainer> children;
    float volume;
    int32_t loopCount;
};

struct AuthoringBehavior {
    std::string id;
    std::vector<std::string> matchTags;
    std::vector<AuthoringCondition> matchConditions;
    std::vector<AuthoringContainer> program;
    std::vector<std::string> parameters; // optional named float params for match conditions
    AuthoringSpatializationSettings spatialization; // programwide, defaults to none when absent
};
```

### CompiledBank - immutable blueprints
Created during `LoadBehaviors()`. Never mutated. Reload creates a new bank.

```cpp
struct CompiledCondition {
    ParameterId parameterId;
    ComparisonOp op;
    float literal;
};

struct CompiledBehavior {
    BehaviorId id;
    ProgramId  programId;
    uint32_t   firstTag;
    uint32_t   tagCount;
    uint32_t   firstCondition;
    uint32_t   conditionCount;
};

struct CompiledContainer {
    ContainerType type;      // OneShot, Loop, Random
    float         volume;
    uint32_t      firstAsset;
    uint32_t      assetCount;
    int32_t       loopCount;
    // envelope params (attack, sustain, release), etc.
};
 
struct CompiledProgram {
    ProgramId id;
    uint32_t  firstContainer;
    uint32_t  containerCount;
    CompiledSpatializationSettings spatialization;
};

struct CompiledBank {
    std::vector<CompiledBehavior>  behaviors;
    std::vector<CompiledProgram>   programs;
    std::vector<CompiledContainer> containers;

    std::vector<TagId>             behaviorTags;
    std::vector<CompiledCondition> conditions;
    std::vector<AssetId>           containerAssets;

    std::vector<std::string>       assetPaths;   // AssetId -> path
    // plus string -> id tables for behavior/program/tag/parameter/asset lookup
};
```

### AssetBank - immutable decoded audio
All assets preloaded at load time. Immutable after decode. Both threads can read freely.

```cpp
struct DecodedBuffer {
    float*   samples;
    uint64_t frameCount;
    uint32_t channelCount;
    uint32_t sampleRate;
};
```

### ProgramInstance - live runtime state (audio thread only)
Created when `CreateInstance` command arrives. Destroyed when program finishes.

```cpp
struct ProgramInstance {
    InstanceId                       instanceId;
    const CompiledProgram*           compiled;    // ptr into CompiledBank, never null
    int                              cursor;      // index of active container
    float                            volume;
    Vec3                             position;
    bool                             stopRequested;
    ActiveContainerState             current;

    int getSamples(float* buf, int framesRequested);
};
```

`ProgramInstance` owns one active container state at a time. Compiled programs are linear, so the runtime does not need heap-owned polymorphic instances for every compiled container. When the cursor advances, the audio thread reinitializes `current` from the next `CompiledContainer`.

---

## 9. Container Model

Each container in a program has a compiled blueprint plus a small value-state struct. Only the currently active container needs runtime state.

```cpp
struct OneShotState {
    uint64_t samplePosition;
};

struct LoopState {
    uint64_t samplePosition;
    int32_t remainingLoops;
};

struct RandomState {
    uint32_t pickedAssetSlot;
    uint64_t samplePosition;
    bool     hasPick;
};

using ActiveContainerState = std::variant<OneShotState, LoopState, RandomState>;
```

The active container state has one job: fill as many frames as it can and report how many it wrote. It does not know about block size beyond the current request, the wider program structure, or what comes next.

**Loop stop rule:** `RequestStop` never cuts the currently playing pass short. It only prevents future wraps. If a loop container is entered after stop has already been requested, it is still allowed one pass and then exhausts. In effect, a stopped loop behaves like `loopCount = 1` from its current or next entry point.

---

## 10. The Buffer Fill Loop

Seamless transitions between containers happen inside a single block:

```cpp
int ProgramInstance::getSamples(float* buf, int framesRequested) {
    int written = 0;
    while (written < framesRequested) {
        int w = renderCurrentContainer(buf + written, framesRequested - written);
        written += w;
        if (written < framesRequested) {
            // container exhausted mid-block - advance cursor
            cursor++;
            if (cursor >= compiled->containerCount) break;  // program finished
            current = makeContainerState(compiledContainerAt(cursor),
                                         derivedSeed(rootSeed, instanceId, compiled->id, cursor));
        }
    }
    return written;
}
```

Because one container hands off to the next within the same block, intro->loop and loop->outro seams are sample-accurate with zero gap. Attack/release envelopes at zero give hard cuts; non-zero gives crossfades.

---

## 11. Audio Thread Update Loop

```cpp
// each callback block:
applyPendingCommands();

for (auto& inst : instances) {
    float localBuf[blockSize];
    int written = inst.getSamples(localBuf, blockSize);

    spatialize(localBuf, written, inst.position);
    mixToOutput(localBuf, written, inst.volume);

    if (written < blockSize) {
        finished.push_back(inst.instanceId);  // program exhausted
    }
}
removeFinished();
```

Real-time safe: no allocations, no locks, no I/O. Reads only from owned instance state and immutable bank/asset data.

---

## 12. Instantiation Flow

When `CreateInstance(instanceId, programId, ...)` arrives on the audio thread:

```cpp
const CompiledProgram* compiled = compiledBank.get(programId);

ProgramInstance inst;
inst.instanceId = instanceId;
inst.compiled   = compiled;
inst.cursor     = 0;
inst.volume     = cmd.volume;
inst.position   = cmd.position;
inst.stopRequested = false;
inst.current = makeContainerState(compiledBank.containers[compiled->firstContainer + 0],
                                  derivedSeed(rootSeed, inst.instanceId, compiled->id, 0));

instances.push_back(inst);
```

`makeContainerState()` creates the active value-state for the current container based on `ContainerType`. Compiled data is never copied - only referenced.

---

## 13. Matching and Behavior Model

**Persistent state**: tags and values that stay valid until changed.
```
entity.player  /  movement.grounded  /  speed = 4.3  /  surface = gravel
```

**Transient events**: submitted for one update tick, then expire.
```
footstep.left  /  weapon.fire  /  ui.confirm
```

**Activation**: a behavior becomes active when required tags match and conditions evaluate true. Control mints an `InstanceId` and sends `CreateInstance`.

**Activation is edge-triggered:** a behavior starts when it becomes matched, not on every tick that it remains matched. If a non-looping program is used under a persistent match and finishes while the match still holds, it does not automatically respawn. If a behavior should stay alive while matched, author a loop in the program.

**Deactivation**: control detects match lost, sends `RequestStop`. The current pass continues, future loop wraps are disabled, and the program retires naturally once its remaining containers are exhausted.

---

## 14. Authoring Model

**For MVP: json.**

Why: mature parsers, human-readable (comments, trailing commas), easy schema validation, no invented language before the runtime is proven.

Long-term: a custom `.audio` DSL is allowed if authoring ergonomics become the bottleneck. It must compile to the same IR. It is a frontend only, never interpreted at runtime.

**MVP supported behavior fields:**
`id`, `matchTags`, `matchConditions`, `program`, `parameters`, `spatialization`

**Phase 7 runtime value model:**

Rules:
- `parameters` remains optional and float-only. It exists for named gameplay values that are used in match conditions.
- `volume` and `position` are implicit reserved runtime values, not authored parameter declarations.
- Authored container `volume` is still just a static blueprint gain.
- Runtime entity `volume` overrides the instance gain multiplier.
- Runtime entity `position` overrides the instance position.
- `SetValue()` writes named float values into world state. If the key is `volume`, it updates the reserved runtime volume instead of a generic match parameter.
- `SetPosition(entityId, x, y, z)` writes the reserved runtime position.
- Reserved runtime values are not valid in authored `parameters` or `matchConditions`.

**Phase 7.5 spatialization model:**

Rules:
- `spatialization` is an optional programwide behavior field.
- If `spatialization` is absent, the program is effectively in `none` mode: positionless and rangeless.
- If `spatialization` is present, the program is spatialized in `pan` mode.
- Spatialization is programwide for MVP. A single program cannot mix spatialized and non-spatialized containers.
- `spatialization` requires exactly `minDistance`, `maxDistance`, and `attenuation`.
- The only valid `attenuation` value in MVP is `linear`.
- Compile-time validation rejects partial `spatialization`, unknown spatialization fields, `minDistance < 0`, and `maxDistance <= minDistance`.
- Listener position is a dedicated engine state updated via `SetListenerPosition(x, y, z)`.
- Pan uses `clamp(relative.x / length(relative), -1, 1)` with `0` when distance is zero.
- Range uses full 3D distance `length(source - listener)`.

**MVP container types:**
`oneshot`, `loop`, `random`, `sequence`

`sequence` is authoring-only in MVP. The compiler flattens it into ordered `CompiledContainer` entries inside `CompiledProgram`. The runtime only executes flat program order.

**Rule:** compile-time errors are strongly preferred. Unknown types, missing assets, type mismatches - reject at load with actionable diagnostics.

---

## 15. Expressions and Conditions

For MVP: numeric literals, named float parameter lookup, and basic comparisons.

Implementation: start with compiled comparison structs (`parameterId`, `op`, `literal`) evaluated against typed runtime value stores. If arithmetic becomes necessary later, extend this to a tiny bytecode or compact expression tree. No regex in the runtime.

---

## 16. Asset Strategy

For MVP: preload everything.
- All referenced assets discovered at compile/load time
- All assets decoded and ready before runtime starts
- Missing assets fail load with explicit diagnostics

**Internal mix format:** the runtime renders stereo interleaved float at 48 kHz. Asset channel count is an input property, not the runtime bus format. Mono assets are duplicated to L/R at read time. Stereo assets remain stereo.

**Spatialization rule for MVP:** mono assets are treated as point sources and spatialized with equal-power pan plus range attenuation. Stereo assets keep their authored width; spatialization applies per-channel balance and attenuation without folding them down to mono. Mixed mono/stereo assets inside a `random` container are legal and are spatialized according to the picked asset's decoded channel layout.

**Audio device config:** device settings are fixed at engine creation time. `EngineConfig` owns an `AudioConfig` payload with default values that can be overridden before `CreateEngine()`. Runtime reconfiguration is out of scope for MVP and, if added later, must be handled as an explicit device recreation path rather than a mutable setter.

**Backend integration:** keep third-party audio libraries behind engine-owned seams. Decoding and playback backend setup should be centralized in dedicated wrapper/implementation translation units so the runtime, resolver, and public API do not depend directly on miniaudio-specific types or macros.

Streaming is a later addition with its own state machine.

---

## 17. Immutability Rules

- `CompiledBank` is immutable after load. Reload creates a new one.
- `AssetBank` buffers are immutable after decode. Both threads read freely.
- `ProgramInstances` are owned exclusively by the audio thread.
- `WorldState` is owned exclusively by the control thread.
- Commands flow one way: host -> control -> audio. Nothing flows back as shared mutable state.

**Deterministic random:** random container picks are derived from an engine root seed plus stable per-instance inputs (`instanceId`, `programId`, container index). Do not use a shared mutable RNG stream as the source of truth; unrelated random draws must not perturb existing playback results.

**RT-safe storage:** avoid heap-owned per-container polymorphic runtime objects. Programs are linear and only one container is active at a time, so `ProgramInstance` stores the current container state by value and rebuilds it when the cursor advances. Active instance storage is reserved up front; capacity exhaustion is a fail-loudly error.

---

## 18. Debugging and Introspection

The engine must be able to answer:
- which behaviors match this entity right now, and why
- which instances are active and in which container
- which asset each instance is currently playing
- which tags/values were considered during matching

Minimum tooling: CLI sandbox app, validator tool, runtime debug dump, trace log for starts/stops/rejections.

---

## 19. Repository Layout

```
include/DeclarativeSoundEngine/   public C API and thin C++ wrappers
src/api/                          public API implementation
src/runtime/                      WorldState, BehaviorResolver
src/playback/                     ProgramInstance, ContainerInstance subclasses
src/mixer/                        audio thread loop, spatialization, mixing
src/compiler/                     parser, validator, compiler, CompiledBank
src/assets/                       AssetBank, decoding
src/backends/                     miniaudio, stub, 
apps/SandboxCLI/                  interactive test app
apps/Validator/                   behavior validation tool
tests/                            unit and integration tests
```

---

## 20. MVP Definition

- Create an engine instance
- Load json, compile into `CompiledBank`, preload all assets into `AssetBank`
- Submit tags and float values for entities
- Submit transient events
- Resolve behaviors -> send `CreateInstance` commands
- Audio thread instantiates `ProgramInstance` objects from compiled programs, executes containers, mixes output
- Seamless container transitions within a block
- `RequestStop` triggers loop exit and program retirement
- Inspect active instances through debug tooling

---

## 21. First Milestone

Intentionally small:
1. Load one json behavior file, compile it, load one audio asset.
2. Submit one tag for one entity, match the behavior.
3. Send `CreateInstance` to audio thread.
4. Audio thread creates a `ProgramInstance`, runs a `LoopContainerInstance`, produces audible output.
5. Remove the tag, send `RequestStop`, audio thread retires the instance cleanly.
6. Inspect what happened from a debug CLI.

If this is clean to implement, the foundation is correct. If it feels hard, the architecture is still wrong.

---

## 22. Roadmap

**Phase 0 - Scaffolding**

* [x] repo layout, build system, CI stub
* [x] empty Engine struct, public C API shell (just the header, no impl)
* [x] lock-free queue implementation (this is load-bearing, get it right early)

* [x] Testable: project compiles, queue passes unit tests (single producer / single consumer, no drops, no races)


**Phase 1 - Compiler + CompiledBank**

* [x] json parser (thirdparty json)
* [x] parse json into `AuthoringDocument`
* [x] schema validation
* [x] lower to `CompiledBehavior` + `CompiledProgram` + `CompiledContainer`
* [x] `CompiledBank` with id->behavior/program lookup and symbol tables
* [x] LoadBehaviors() entry point

* [x] Testable: Validator CLI - load a json file, print compiled bank contents or emit diagnostics

**Phase 2 - Control side: world state**

* [x] EntityState (tags + values)
* [x] WorldState (flat entity map)
* [x] host->control command queue + drain loop
* [x] SetTag, RemoveTag, SetValue, DestroyEntity commands implemented

* [x] Testable: submit commands from a test harness, inspect WorldState after drain, verify entity state is correct

**Phase 3 - AssetBank**

* [x] asset discovery from compiled bank manifest
* [x] decode to DecodedBuffer (via miniaudio)
* [x] id->buffer lookup
* [x] missing asset diagnostics

* [x] Testable: load a bank, verify all assets decoded, print asset manifest

**Phase 4 - Audio side: playback**

* [x] ContainerInstance base + OneShotInstance, LoopInstance, RandomInstance
* [x] ProgramInstance with cursor + fill loop
* [x] CreateInstance / RequestStop / SetVolume / SetPosition command consumer
* [x] stub backend that just calls the fill loop and discards output

* [x] Testable: manually send CreateInstance from a test harness, verify getSamples() produces correct output (compare against known buffer), verify RequestStop retires cleanly

**Phase 5 - Resolver: matching**

* [x] BehaviorResolver - match tags + conditions against CompiledBank
* [x] emit CreateInstance on new match
* [x] emit RequestStop on lost match
* [x] control->audio command queue wired up

* [x] Testable: set tags on an entity, verify correct ProgramInstance is created on audio side. remove tag, verify RequestStop is sent and instance retires

**Phase 6 - Audible output**

* [x] miniaudio backend wired up
* [x] miniaudio implementation centralized behind engine-owned backend/decoder seams
* [x] `EngineConfig` owns defaultable startup-only `AudioConfig` (sample rate, sample format, output channels, callback frames, backend preference)
* [x] audio thread callback calls fill loop and mixes direct output

* [x] Testable: sandbox CLI - load behaviors, set a tag, hear a sound. remove tag, hear it stop.

**Phase 7 - Resolver: param forwarding**

* [x] SetValue on entity -> SetVolume / SetPosition forwarded to bound instances
* [x] reserved runtime values `volume` / `position` forwarded without authored bindings

* [x] Testable: change entity value, verify audio instance updates
* [x] Testable: add test in sandboxCLI that incorporates a parameter change (volume)

**Phase 7.5 - Spatialization**

* [x] authored programs declare spatialization explicitly through optional programwide `spatialization`
* [x] spatialization settings live in dedicated authored/compiled structs so range, attenuation model, and later falloff-curve options have a stable home
* [x] listener position can be set with a new command `SetListenerPosition()`, forwarded to the audio thread
* [x] audio thread owns listener state
* [x] audio is spatialized relative to listener position
* [x] pan-only spatialization for mono sources
* [x] range-based attenuation for spatialized sources
* [x] stereo sources keep authored width while spatialization applies balance + attenuation

* [x] Testable: change entity position value and hear output adapt relative position. same for moving listener.

**Phase 8 - Debugging + CLI**

* [ ] `GetDebugSnapshot()` - serialize current engine state to readable struct or JSON dump
  * active instances (instanceId, programId, current container type, asset playing)
  * entity->behavior matches and why (tags considered, conditions evaluated)
  * current param values per entity
* [ ] ring buffer trace log (CreateInstance / RequestStop / match / unmatch events, timestamped)
* [ ] full sandbox CLI with interactive commands: `entity`, `tag`, `untag`, `setval`, `setpos`, `setlistener`, `dump`, 

* [ ] Testable: interactive session that exercises every command type, verify dump output is accurate and readable

**Phase 9 - Hardening**

*RT safety + operational edge cases (unit test suite already exists)*

* [ ] lock/allocation scan of audio thread path (static analysis + manual trace of every callback codepath)
* [ ] instance capacity exhaustion: verify fail-loudly behavior, document the contract
* [ ] missed/late `Update()` handling: document the contract explicitly
  * transient events submitted but not yet drained wait for next Update() - this is correct, document it
  * burst catch-up after missed calls: command queue handles it, verify under test
  * if Update() is never called again: transient events never fire, persistent state freezes - document as host responsibility
* [ ] profiling pass: block timing, worst-case per-instance cost, mixer overhead at realistic instance counts

* [ ] Testable: RT safety audit produces no findings; capacity exhaustion path verified; missed-Update scenarios documented and tested

**Phase 10 - Multichannel output**

*Parking lot - decision pending*

* [ ] decide: does the engine accommodate system output channel count, or always output stereo and let the host/platform upmix?
  * "fold to stereo + let system handle it" is honest and probably correct for an embeddable library
  * accommodating arbitrary channel counts requires channel-layout-aware spatialization (pan logic is stereo-specific right now)
  * no authored `AudioConfig` channel override is likely the right call - read system config, adapt or fold
* [ ] if multichannel: introduce channel layout abstraction (stereo, 5.1, 7.1 - enum, not arbitrary)
* [ ] if multichannel: spatialization must become channel-layout-aware (not just [-1,1] pan)
* [ ] if fold: document the policy and verify stereo output is correct for all spatialization modes

*Note: deprioritize until a concrete use case demands it. Most games route through their own mixer before hitting the speaker layout.*

**Phase 11 - Virtualization**

*Keep it practical - no priority system yet, just the useful subset*

* [ ] hard voice ceiling: `maxInstances` in `EngineConfig`, exceeded at runtime -> steal or reject (policy TBD)
  * steal policy: steal instance with lowest effective volume (attenuation already gives this number)
  * ? or: reject new instance, keep existing (simpler, possibly wrong perceptually)
  * ? or: steal oldest
* [ ] skip-mix culling: instances with effective output volume below threshold skip accumulate step but still advance playhead
  * threshold: ? (needs tuning - maybe authored per-behavior, maybe global config)
  * cost saving: buffer copy still happens, mix accumulate is skipped - modest but free
* [ ] out-of-range oneshot cancellation: control policy - if entity is beyond `maxDistance` at CreateInstance time, don't create
  * alternatively: send RequestStop if entity moves beyond maxDistance while matched oneshot is active
  * ? which is more useful in practice
* [ ] out-of-range loop: call getSamples(), zero the output before accumulate - keeps playhead in sync for when source re-enters range
  * ? is playhead coherence actually worth the buffer copy cost for loops? probably yes for music/ambient layers
* [ ] ? voice stealing is probably the bigger practical win - prioritize over OOR policies if scope needs trimming

* [ ] Testable: exceed voice ceiling, verify steal/reject behavior; entity moves OOR, verify oneshot cancelled; OOR loop re-enters range, verify no seam

*Note: authored priority system deferred - add once real use cases reveal what priority actually means in practice*

**Phase 12 - Hot Reloading**

*Hard wipe + world state preservation. No behavior identity mapping across reloads.*

* [ ] `ReloadBehaviors(path)` - compiler re-runs, produces new CompiledBank + new AssetBank
* [ ] audio side: hard reset - flush instance list directly (no graceful RequestStop drain, timing loss is acceptable for a tooling workflow)
* [ ] control side: flush entity->instanceId binding table
* [ ] swap banks atomically (control stops reading old bank before audio thread does)
* [ ] re-resolve current WorldState against new bank -> emit fresh CreateInstance commands
* [ ] world state survives: tags, values, entity set all preserved across reload
* [ ] document the contract: ReloadBehaviors() is a hard audio state reset. World state survives. Instance timing does not.

*Note: identity mapping across reloads deliberately out of scope. New params, new assets, new behaviors all change IDs. Preserving world state is already a big usability win.*

* [ ] ? asset reuse across reloads: if path+hash match, reuse DecodedBuffer from old AssetBank (optimization, not required for correctness)

* [ ] Testable: load bank A, start instances, reload bank B, verify audio hard-resets, verify world state re-resolves correctly against new bank

**Phase 12.5 - Bank loading / unloading**

*Additive bank management - load additional banks at runtime without full reload*

* [ ] `LoadAdditionalBank(path)` - compile and merge into running engine without stopping playback
  * ? merge strategy: new behaviors added to resolver, conflicts (duplicate behavior ids) -> error or override?
  * ? does this create a second CompiledBank, or merge into one? separate banks with resolver iterating all feels cleaner
* [ ] `UnloadBank(bankId)` - remove a bank's behaviors from the resolver
  * instances currently playing from that bank: let them drain (RequestStop) or hard-stop?
  * ? reference tracking: audio thread needs to know which bank a ProgramInstance came from
* [ ] asset management: unloading a bank must not free assets still referenced by active instances

*Note: this is probably more immediately useful than streaming for most use cases (level loading, DLC, dynamic content). Streaming and bank management are complementary but independent features.*

* [ ] ? Testable: load base bank, load addional bank, verify new behaviors resolve, unload additional bank, verify behaviors removed and instances drain

**Phase 13 - BlendContainer**

*A/B parameter-driven blend. Multi-blend deferred until base concept proven.*

* [ ] new container type: `blend` (authored + compiled)
  * authored fields: `assets` (exactly 2 for now), `parameter` (name of the blend control param), `range` ([minVal, maxVal] mapped to [0,1] blend ratio)
  * compiled: assetA, assetB, parameterId, rangeMin, rangeMax
* [ ] `BlendState` runtime struct:
  * samplePositionA, samplePositionB (both always advance)
  * current blend ratio [0,1]
  * output: `lerp(sampleA, sampleB, ratio)` per frame
* [ ] new command: `SetParam(instanceId, parameterId, value)`
  * control watches entity values, forwards to bound instances when blend parameter changes
  * audio thread applies to BlendState of matching instance
  * ? scope question: is SetParam only for blend, or the start of a general per-instance parameter mechanism?
    * if general: sets up expression-driven volume, pitch, etc. later - worth deciding scope now even if only blend uses it for now
    * if blend-only: simpler, can be generalized later - but API shape might need to change
* [ ] both playheads always advance regardless of blend ratio (keeps B in sync even at ratio=0)
* [ ] blend parameter is not a match condition input - it is a runtime modulator only

* [ ] Testable: author a blend behavior, set entity value, verify output crossfades correctly; verify both playheads stay in sync at ratio extremes

**Phase 14 - Linux**

*Mostly build/CI work - miniaudio already handles ALSA/PipeWire*

* [ ] CI matrix: Ubuntu, GCC + Clang
* [ ] verify ALSA backend; test PipeWire if feasible
* [ ] .so artifact
* [ ] SandboxCLI binary for Linux
* [ ] fix whatever breaks (likely include paths, compiler warnings treated as errors, minor portability issues)

**Phase 15 - OSX**

*Same story - CoreAudio via miniaudio*

* [ ] CI: macOS, Clang
* [ ] CoreAudio backend verified
* [ ] ? Universal binary (arm64 + x86_64) if distributing prebuilts
* [ ] .dylib artifact
* [ ] fix whatever breaks

**Phase 16 - Multi-listener**

*Small API change with some non-trivial policy decisions*

* [ ] `SetListenerPosition(listenerId, x, y, z)` - listener 0 is implicit (no-id call routes to listener 0)
* [ ] `maxListeners` in `EngineConfig` (default 1) - fixed at engine creation, not dynamic
* [ ] audio thread owns listener state as indexed array, not a single vec3
* [ ] spatialization policy: nearest-listener assignment per instance
  * pick the listener that minimizes distance, spatialize from that perspective
  * avoids N-render problem, perceptually fine for non-VR cases
  * ? should assignment be re-evaluated every block, or only on position change? (every block is safer)
* [ ] non-spatialized sources: play once at authored volume, no duplication regardless of listener count
* [ ] ? more than N listeners: reject at config time or silently clamp?

* [ ] Testable: two listeners, spatialized source, verify nearest-listener assignment; non-spatialized source, verify no duplication

**Phase 17 - Streaming**

*Large feature - separate state machine, breaks preload-everything model*

* [ ] ? to be discussed when bank loading/unloading is proven and the asset model is stable
* [ ] streaming requires: background IO thread, per-instance ring buffer, audio thread reads from ring buffer instead of DecodedBuffer
* [ ] interaction with bank unloading: streaming instance holds a file reference, not a decoded buffer - unload becomes more complex
* [ ] ? priority: is streaming more urgent than any of the above? probably not until asset sizes actually become a problem

---

## 23. Open Decisions

- Which `AudioConfig` fields are required in the public API for MVP beyond sample rate, sample format, output channels, callback frames, and backend preference?

- Multichannel output policy: accommodate system channel count with channel-layout-aware spatialization, or always fold to stereo and let host/platform handle upmixing? (Phase 10)

- Voice stealing vs. reject policy at voice ceiling: steal lowest-volume instance, or reject the new one? (Phase 11)

- OOR oneshot cancellation: reject at CreateInstance time, or send RequestStop when entity moves OOR? (Phase 11)

- `SetParam` scope: blend-only command, or start of a general per-instance parameter mechanism? Decide before Phase 13 ships since it affects the API shape. (Phase 13)

- Multi-listener spatialization assignment: re-evaluate nearest listener every block, or only on position change? (Phase 16)

- Bank merge strategy for additional bank loading: separate CompiledBank objects with resolver iterating all, or merge into one? Conflict handling on duplicate behavior ids? (Phase 12.5)

- When spatialization expands beyond pan/range, do we want one listener only for MVP+1, or should multi-listener support be planned into the command shape immediately? (answered: listener 0 default, maxListeners in config, fixed at creation)

---

## 24. Summary

Gameplay declares state. The audio engine resolves intent. The audio thread executes compiled programs.

```
CompiledBank          blueprint, immutable, shared read-only
AssetBank             audio data, immutable, shared read-only
ProgramInstance[]     live state, owned exclusively by audio thread

Control:  matching + instance lifecycle decisions
          ↓ CreateInstance / SetVolume / SetPosition / RequestStop
Audio:    executes CompiledProgram via ContainerInstance subclasses
          fills blocks, handles transitions, spatializes, mixes
```

Control is the brain. Audio is a small deterministic executor. The compiler is where the real complexity lives - and it runs once, offline, with no RT constraints.

If those boundaries hold, the engine is debuggable, reloadable, and RT-safe by construction.
If they blur, it collapses.
