# Decl_Audio

A declarative audio engine for games. Game code declares world facts - entity tags, float values, transient events. The engine decides what plays.

```
SetTag(engine, "player",  "movement.grounded");
SetTag(engine, "player",  "movement.walking");
SetValue(engine, "player", "speed", 4.2f);
Update(engine);
// -> footstep behavior matched, instance spawned, audio thread plays it
```

---

## How it works

Behaviors live in JSON. Each behavior says: *when these tags are present and these conditions hold, run this program.* The engine matches world state against those behaviors and manages instance lifetimes automatically.

```json
{
  "behaviors": [
    {
      "id": "movement.footsteps",
      "matchTags": ["movement.grounded", "movement.walking"],
      "parameters": ["speed"],
      "matchConditions": [{ "parameter": "speed", "op": ">=", "value": 0.1 }],
      "program": [
        { "type": "random", "assets": ["audio/step_a.wav", "audio/step_b.wav"], "volume": 0.9 }
      ]
    }
  ]
}
```

A match starts the behavior. The match going away sends a stop - the current pass finishes cleanly before the instance retires.

---

## Authoring

### Container types

| Type       | Description                                                                       |
| ---------- | --------------------------------------------------------------------------------- |
| `oneshot`  | Play once and advance                                                             |
| `loop`     | Repeat (`loopCount: -1` = infinite)                                               |
| `random`   | Pick one asset from `assets` on each entry                                        |
| `sequence` | Play children in order (compiler flattens to linear)                              |
| `select`   | Choose one child on entry, run it for the match lifetime                          |
| `blend`    | Run both children simultaneously, mix by `parameter` (0 -> child A, 1 -> child B) |

### Match conditions

Conditions are ANDed. Supported operators: `<`, `<=`, `==`, `>=`, `>`, `!=`.

```json
"matchConditions": [
  { "parameter": "health", "op": "<", "value": 0.25 }
]
```

Parameters used in conditions must be declared in `"parameters"`.

### Transient tags

Tags submitted via `SetTransientTag` are active for exactly one `Update()` pass, then cleared automatically. Use these for events like footsteps or weapon fires.

### Spatialization

Add a `"spatialization"` block to a behavior for panned, distance-attenuated playback. Listener position is set separately.

```json
"spatialization": {
  "minDistance": 1.0,
  "maxDistance": 20.0,
  "attenuation": "linear"
}
```

Set source position via `SetPosition(engine, entityId, x, y, z)` and listener via `SetListenerPosition(engine, x, y, z)`.

### Blend example

```json
{
  "id": "engine.rumble",
  "matchTags": ["vehicle.engine_on"],
  "parameters": ["rpm"],
  "program": [
    {
      "type": "blend",
      "parameter": "rpm",
      "children": [
        { "type": "loop", "asset": "audio/engine_idle.wav", "loopCount": -1, "volume": 1.0 },
        { "type": "loop", "asset": "audio/engine_high.wav", "loopCount": -1, "volume": 1.0 }
      ]
    }
  ]
}
```

`rpm = 0` -> idle only. `rpm = 1` -> high only. Values in between mix both.

---

## C API

```c
// Engine lifecycle
EngineConfig cfg = GetDefaultConfig();
DeclAudioEngine* engine = nullptr;
CreateEngine(&cfg, &engine);
LoadBehaviors(engine, "path/to/behaviors.json");

// Per-frame
SetTag(engine, "player", "movement.walking");
SetValue(engine, "player", "speed", 4.2f);
SetPosition(engine, "player", x, y, z);
Update(engine);   // drains commands, runs resolver, sends audio commands

// Cleanup
DestroyEntity(engine, "player");
DestroyEngine(engine);
```

The engine ships as a compiled shared library (DLL/.so) with this C header as the sole public contract.

### EngineConfig

| Field                  | Description                                                          |
| ---------------------- | -------------------------------------------------------------------- |
| `sample_rate`          | Device sample rate (default: 48000)                                  |
| `output_channel_count` | Output channels (default: 2)                                         |
| `callback_frame_count` | Frames per audio callback block                                      |
| `max_instances`        | Hard voice ceiling - exceeding it terminates loudly                  |
| `backend`              | `DECL_AUDIO_BACKEND_PLATFORM_DEFAULT` or `DECL_AUDIO_BACKEND_SILENT` |

---

## Build

**Visual Studio 2022** - open `Decl_Audio.sln`, target `x64 Debug` or `x64 Release`.

Output: `x64/Debug/Decl_Audio/Decl_Audio.dll`

**g++ (alternative):**
```bash
g++ -std=c++20 -Iinclude -Isrc/platform/win32 src/**/*.cpp tests/*.cpp -o decl_audio_tests
```

---

<!-- ## Architecture

```
JSON -> Compiler -> CompiledBank + AssetBank   (loaded once, immutable)
                        ↓
[Host Thread]   SetTag / SetValue / Update()
        ↓  lock-free queue
[Control Thread]  WorldState -> BehaviorResolver -> mints/kills instances
        ↓  lock-free queue (timestamped commands)
[Audio Thread]  ProgramInstance[] -> container execution -> mix -> output
```

Three threads, two queues, no shared mutable state. The audio thread owns its instances exclusively and only reads from immutable bank data.
 -->
