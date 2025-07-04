## Decl Audio

**Decl** is a modular, tag-driven audio engine designed to *decouple* sound logic from game logic. Instead of embedding playback code in gameplay scripts, Decl listens to the world state-tags, values, events - and reacts accordingly. Sound becomes a system that describes behavior, not one that gets micromanaged.

This means minimal audio logic in game code: just keep the sound engine updated with what's happening in the world. Decl handles the rest.

---

### Core Idea: Audio as Reaction to State

In a typical integration, your game loop sends state updates like:

```cpp
AudioManager_SetTag(entityId, "entity.player");
AudioManager_SetFloatValue(entityId, "speed", 4.3f);
AudioManager_SetTransientTag(entityId, "foot.contact.left");
```

The `.audio` files define behavior reactively:

```yaml
# Base ambient weather event
t- id: weather_base.audio
  matchTags:
    - ambience.weather
  soundNode:
    layer:
      sounds:
        - base_ambience.wav
  volume: 1.0

# Rain variant
- id: weather_rain.audio
  inherit: weather_base.audio
  matchTags:
    - ambience.weather.rain
  overrides:
    soundNode:
      layer:
        sounds:
          - base_ambience.wav
          - rain_loop.wav
  volume: "+0.2"

# Sunshine variant with condition
- id: weather_sunshine.audio
  inherit: weather_base.audio
  matchTags:
    - ambience.weather.sunshine
  matchConditions: ["temperature > 20"]
  overrides:
    soundNode:
      layer:
        sounds:
          - base_ambience.wav
          - birds_chirping.wav
  volume: "-0.5"

# Footstep event
- id: footstep
  matchTags:
    - entity.player
    - foot.contact.*
  parameters:
    volume: "1.0"
  onStart:
    random:
      sounds:
        - player_step_gravel_01.wav 
        - player_step_gravel_02.wav
  # onActive:
  #   layer:
  #     sound: step_gravel_loop.wav
  #     volume: 0.5
  #     loop: 1

# Velocity-driven states
- id: idle.audio
  matchTags: ["entity.player"]
  matchConditions: ["velocity < 0.1"]
  sound: idle.wav

- id: walk.audio
  matchTags: ["entity.player"]
  matchConditions: ["velocity >= 0.1", "velocity < 3.0"]
  sound: walk.wav

- id: run.audio
  matchTags: ["entity.player"]
  matchConditions: ["velocity >= 3.0"]
  sound: run.wav
```

Decl supports:

* **Wildcards** in tags (e.g. `foot.contact.*` matches `foot.contact.left`, `foot.contact.right`, etc.)
* **Event states**:

  * `onStart`: plays when triggered
  * `onActive`: loops while active
  * `onStop`: plays on stop

No hardcoded decisions in game code. Just describe state—Decl figures out the rest.

---

### 🔍 Features

* **Declarative `.audio` event format**
  Describe playback logic in structured YAML-like text files—no scripting required for common sound behaviors.

* **Logic nodes**
  Includes `play`, `random`, `layer`, `sequence`, `loop`, and more—chainable and nestable.

* **Expressions**
  Full support for mathematical expressions, conditionals, and logic inside parameter values.

* **Inheritance**
  Base events can be extended or selectively overridden by variants.

* **Entity/Tag-based routing**
  Sound behavior is driven by tags and world state—minimal coupling to gameplay code.

* **Minimal API footprint**
  Push values and tags—Decl resolves all logic and playback internally.

* **Spatialization-ready**
  Position and listener data routed via entity tags and metadata—no engine-specific calls needed.

* **Sample rate checking**
  Audio buffers compare their decoded sample rate with the device's rate when loaded. If they differ, a warning is logged and the buffer is resampled so playback speed remains correct.


### API Functions

The engine exposes a compact C interface. After creating the manager you mostly
push tags and values to drive playback:

````c
void AudioManager_Create(AudioConfig* cfg);
void AudioManager_Destroy();
void AudioManager_LoadBehaviorsFromFile(const char* behaviorPath,
                                        const char* assetPath);

void AudioManager_SetTag(const char* entityId, const char* tag);
void AudioManager_SetTransientTag(const char* entityId, const char* tag);
void AudioManager_ClearTag(const char* entityId, const char* tag);

void AudioManager_SetFloatValue(const char* entityId, const char* key, float value);
void AudioManager_SetStringValue(const char* entityId, const char* key,
                                 const char* value);
void AudioManager_SetVectorValue(const char* entityId, const char* key,
                                 float x, float y, float z);
void AudioManager_ClearValue(const char* entityId, const char* key);
void AudioManager_ClearEntity(const char* entityId);

void AudioManager_LogSetMinimumLevel(LogCategory category, LogLevel level);
void AudioManager_SetLogCallback(LogCallbackFn cb);
bool AudioManager_PollLog(int* outCat, int* outLvl, char* outMsg, int maxLen);

````

---

More examples, integration guides, and tooling coming soon.

## Selecting an audio backend

The engine supports multiple backends. By default the CLI tool uses the
`Miniaudio` backend which requires audio hardware. You can override this
behavior using the `DECLSOUND_BACKEND` environment variable:

```
export DECLSOUND_BACKEND=stub   # run without audio hardware
```

Valid values are `miniaudio`, `unity`, and `stub`.


# Build instructions
mkdir build && cd build
cmake ..
cmake --build . --config Debug
