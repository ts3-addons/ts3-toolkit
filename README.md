# TS3Toolkit

A native client plugin for TeamSpeak 3 that adds a modular set of quality-of-life
features. The first feature is **personal entrance sound**: when the local user
enters a channel, a custom sound is injected into the voice stream so everyone in
the channel hears it, without any server-side changes and without other clients
installing anything.

## Status

Early development. The entrance sound feature is the current focus; the
repository is structured so further features can be added as independent modules.

## How it works

The plugin registers a custom TeamSpeak 3 capture device. A dedicated audio
pump mixes the hardware microphone with a decoded entrance sound and pushes the
combined 48 kHz mono PCM into the client through the official plugin API. The
client then encodes and transmits it as ordinary voice.

See the documents under `docs/` for the detailed design and contracts.

## Architecture

The codebase separates platform-independent business logic from thin adapters:

- `core` — SDK-independent logic: PCM primitives, mixing, state machines,
  configuration model.
- `audio` — miniaudio-based adapter for microphone capture and sound file
  decoding/resampling.
- `config` — versioned, validated persistent settings.
- `entrance` — entrance sound controller and scheduling.
- `plugin` — TeamSpeak 3 entry points, custom device, event callbacks.
- `platform` — platform-specific helpers.

## Layout

```
include/ts3toolkit/   public headers, grouped by layer
src/                  implementations, grouped by layer
third_party/          vendored TS3 plugin SDK headers and miniaudio
tests/                unit tests
docs/                 design and behavior documents
packaging/            .ts3_plugin packaging template
cmake/                CMake helper modules
```

## Build requirements

The native build targets Windows x64 and requires:

- A C++17 compiler (MSVC toolchain)
- Windows SDK
- CMake and Ninja (or the Visual Studio generator)

The build instructions are added together with the CMake project in a later
milestone.

## Documentation

- [docs/architecture.md](docs/architecture.md) — layer boundaries and data flow.
- [docs/entrance-sound.md](docs/entrance-sound.md) — entrance sound contracts.
