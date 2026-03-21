## Decl Audio

**Decl** is a tag-driven audio engine intended to keep sound behavior out of gameplay code. Game code publishes tags, values, and events; the audio engine reacts to that state declaratively.

### Core Idea

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

* **Wildcards** in tags, such as `foot.contact.*`
* **Event states**: `onStart`, `onActive`, `onStop`

### Features

* **Declarative `.audio` event format**
  Describe playback logic in structured YAML-like text files without scripting for common sound behaviors.

* **Logic nodes**
  Includes `play`, `random`, `layer`, `sequence`, `loop`, and more.

* **Expressions**
  Full support for mathematical expressions, conditionals, and logic inside parameter values.

* **Inheritance**
  Base events can be extended or selectively overridden by variants.

* **Entity/Tag-based routing**
  Sound behavior is driven by tags and world state.

* **Minimal API footprint**
  Push values and tags and let Decl resolve playback internally.

* **Spatialization-ready**
  Position and listener data are routed via entity tags and metadata.

* **Sample rate checking**
  Audio buffers compare their decoded sample rate with the device's rate when loaded. If they differ, a warning is logged and the buffer is resampled so playback speed remains correct.


### Public API

The engine exposes a compact C interface:

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

## Selecting an audio backend

The engine supports multiple backends. The CLI defaults to `miniaudio`, which requires audio hardware.

Valid values are `miniaudio`, `unity`, and `stub`.

## Repository Layout

```text
include/DeclarativeSoundEngine/   public headers
src/core/                         engine runtime and parsing
src/backends/                     backend implementations
src/support/                      support headers and third-party helpers
src/platform/win32/               Visual Studio / Win32 glue
apps/AudioTestCLI/                example CLI application
```

## Build

```sh
cmake -S . -B build
cmake --build build --config Debug
```

If you are using Visual Studio on Windows, prefer presets so the compiler is re-detected into a fresh build directory:

```powershell
cmake --preset windows-vs2022-debug
cmake --build --preset build-windows-debug
```

The build expects either:

* `../miniaudio/miniaudio.h`
* `../yaml-cpp-master/`

If `yaml-cpp` is not present locally, CMake will fetch it.

## CLI

`AudioTestCLI` is a small interactive test app:

```sh
./AudioTestCLI --assets /path/to/assets --behaviors /path/to/behaviors --backend stub
```

Supported commands in the prompt:

* `help`
* `tag <entity> <tag>`
* `clear <entity> <tag>`
* `transient <entity> <tag>`
* `float <entity> <key> <value>`
* `string <entity> <key> <value...>`
* `pos <entity> <x> <y> <z>`
* `quat <entity> <a> <b> <c> <d>`
* `transform <entity> <x> <y> <z> <a> <b> <c> <d>`
* `dump`
* `exit`
