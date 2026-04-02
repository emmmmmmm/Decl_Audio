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

---

## 7. The Command Interface (Control -> Audio)

The command set is intentionally thin. Control does all the semantic work before sending anything.

```
CreateInstance(instanceId, programId, position, volume)
SetVolume(instanceId, value)
SetPosition(instanceId, vec3)
RequestStop(instanceId)
```

That's essentially it.

**On param updates:** when entity X's values change, control looks up which instances are bound to entity X and sends targeted commands to each. Entity structs never cross the boundary - only their effects do, as targeted commands to specific instances.

**On timing:** params that drift slowly (health, speed, distance) are fine at block accuracy (~5ms). Hard events (gunshot, footstep) should carry a sample timestamp so the audio thread can apply them at the exact offset within the block.

**On stage transitions:** the audio thread handles all playback-driven transitions autonomously (container exhausted -> advance to next). Control only sends world-driven transitions (`RequestStop` when a match condition is lost). No return signalling needed.

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
    std::vector<ContainerInstance*>  containers;  // one per compiled container

    ContainerInstance* current() { return containers[cursor]; }
    int getSamples(float* buf, int framesRequested);
};
```

---

## 9. Container Model

Each container in a program is a subclass of `ContainerInstance`. The compiled data is the blueprint; the instance holds all runtime state.

```cpp
struct ContainerInstance {           // base class
    const CompiledContainer* compiled;
    // envelope state, etc.

    // returns framesWritten
    // if framesWritten < framesRequested, container is exhausted
    virtual int getSamples(float* buf, int framesRequested) = 0;
};

struct OneShotInstance : ContainerInstance {
    uint64_t samplePosition;
    // copies from asset, advances position
    // returns < framesRequested when end of asset is reached
};

struct LoopInstance : ContainerInstance {
    uint64_t samplePosition;
    bool     stopRequested;
    // wraps at asset end - never exhausts unless stopRequested
};

struct RandomInstance : ContainerInstance {
    AssetId  pickedAssetId;   // picked once on first getSamples()
    uint64_t samplePosition;
    // after pick, behaves like OneShot
};
```

Container subclasses have one job each: fill as many frames as they can, return how many they wrote. They do not know about block size, program structure, or what comes next.

---

## 10. The Buffer Fill Loop

Seamless transitions between containers happen inside a single block:

```cpp
int ProgramInstance::getSamples(float* buf, int framesRequested) {
    int written = 0;
    while (written < framesRequested) {
        int w = current()->getSamples(buf + written, framesRequested - written);
        written += w;
        if (written < framesRequested) {
            // container exhausted mid-block - advance cursor
            cursor++;
            if (cursor >= containers.size()) break;  // program finished
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
applyPendingCommands();
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

for (uint32_t i = 0; i < compiled->containerCount; ++i) {
    const CompiledContainer& cc = compiledBank.containers[compiled->firstContainer + i];
    inst.containers.push_back(makeContainerInstance(cc));
}

instances.push_back(inst);
```

`makeContainerInstance()` is a factory that creates the right subclass based on `ContainerType`. Compiled data is never copied - only pointed to.

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

**Deactivation**: control detects match lost, sends `RequestStop`. The active container (typically a `LoopInstance`) sets `stopRequested`, exhausts on its next update, and the program retires naturally.

---

## 14. Authoring Model

**For MVP: json.**

Why: mature parsers, human-readable (comments, trailing commas), easy schema validation, no invented language before the runtime is proven.

Long-term: a custom `.audio` DSL is allowed if authoring ergonomics become the bottleneck. It must compile to the same IR. It is a frontend only, never interpreted at runtime.

**MVP supported behavior fields:**
`id`, `matchTags`, `matchConditions`, `program`, `parameters`

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

Streaming is a later addition with its own state machine.

---

## 17. Immutability Rules

- `CompiledBank` is immutable after load. Reload creates a new one.
- `AssetBank` buffers are immutable after decode. Both threads read freely.
- `ProgramInstances` are owned exclusively by the audio thread.
- `WorldState` is owned exclusively by the control thread.
- Commands flow one way: host -> control -> audio. Nothing flows back as shared mutable state.

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

* [ ] asset discovery from compiled bank manifest
* [ ] decode to DecodedBuffer (via miniaudio or stb_vorbis)
* [ ] id->buffer lookup
* [ ] missing asset diagnostics

* [ ] Testable: load a bank, verify all assets decoded, print asset manifest

**Phase 4 - Audio side: playback**

* [ ] ContainerInstance base + OneShotInstance, LoopInstance, RandomInstance
* [ ] ProgramInstance with cursor + fill loop
* [ ] CreateInstance / RequestStop / SetVolume / SetPosition command consumer
* [ ] stub backend that just calls the fill loop and discards output

* [ ] Testable: manually send CreateInstance from a test harness, verify getSamples() produces correct output (compare against known buffer), verify RequestStop retires cleanly

**Phase 5 - Resolver: matching**

* [ ] BehaviorResolver - match tags + conditions against CompiledBank
* [ ] emit CreateInstance on new match
* [ ] emit RequestStop on lost match
* [ ] control->audio command queue wired up

* [ ] Testable: set tags on an entity, verify correct ProgramInstance is created on audio side. remove tag, verify RequestStop is sent and instance retires

**Phase 6 - Audible output**

* [ ] miniaudio backend wired up
* [ ] audio thread callback calls fill loop, mixes, spatializes
* [ ] stereo pan for MVP spatialization

* [ ] Testable: sandbox CLI - load behaviors, set a tag, hear a sound. remove tag, hear it stop.

**Phase 7 - Resolver: param forwarding**

* [ ] SetValue on entity -> SetVolume / SetPosition forwarded to bound instances
* [ ] value->param mapping in authored behaviors

* [ ] Testable: change entity position value, verify audio instance position updates

**Phase 8 - Debugging + CLI**

* [ ] runtime debug dump (active instances, current container, asset playing)
* [ ] match explanation (why did/didn't behavior B match entity X)
* [ ] full sandbox CLI with interactive commands

* [ ] Testable: interactive session, inspect everything

**Phase 9 - Hardening**

* [ ] unit tests for compiler, resolver, container types
* [ ] integration test: full round-trip from json -> audible output
* [ ] reload test: swap bank while instances are playing
* [ ] profiling pass

---

## 23. Open Decisions

- Should the control loop be host-driven (tick on game update), internal-thread-driven, or support both?
- Should entity IDs be numeric only in the public API, or support string names at the API boundary?
- What level of spatialization for MVP: stereo pan only, or basic 3D attenuation + listener transform?

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
