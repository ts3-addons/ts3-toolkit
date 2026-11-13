# TS3Toolkit

[简体中文](README.zh-CN.md) | English

A native Windows x64 plugin for TeamSpeak 3. TS3Toolkit currently adds a personal
channel entrance sound: when you enter a channel, a selected sound is mixed into
your microphone stream so everyone in the channel hears it. No server component or
client-side installation is required for other users.

## Features

- Mix a selected sound into the custom TeamSpeak capture device.
- Keep microphone passthrough active when the feature fails or is idle.
- Configure enable state, volume, delay, cooldown, and a sound range.
- Decode WAV, MP3, and FLAC input and convert it to 48 kHz mono 16-bit PCM.
- Preview a selected range from the settings dialog.
- Build a double-click-installable `.ts3_plugin` package.

## Install

1. Build the package with the commands below.
2. In TeamSpeak 3, open the plugin manager and install
   `build/package/ts3toolkit-0.1.0.ts3_plugin`.
3. Restart TeamSpeak when prompted.
4. Open TS3Toolkit settings and choose a sound file, then set TeamSpeak's input
   device to `TS3Toolkit Virtual Capture`.

## Build and test

Requirements: Windows x64, Visual Studio/MSVC with the Windows SDK, CMake, and a
generator supported by CMake.

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --build build --config Release --target ts3toolkit_package
```

The package is written to `build/package/`.

## Architecture

The code is split into layers with dependencies pointing inward:

- `core` — PCM types, mixing, math, and bounded buffers.
- `audio` — microphone capture, decoding, resampling, and playback.
- `config` — versioned settings storage and validation.
- `entrance` — trigger, delay, cooldown, and playback state.
- `plugin` — TeamSpeak 3 entry points, events, and the custom capture device.
- `platform` — Windows-specific helpers.

Repository layout:

```text
include/ts3toolkit/  public headers
src/                 implementation by layer
tests/               unit tests
docs/                behavior and architecture contracts
packaging/           .ts3_plugin packaging scripts
third_party/         vendored SDK and miniaudio
```

## Documentation

- [Architecture](docs/architecture.md) — layer boundaries and data flow.
- [Entrance sound contract](docs/entrance-sound.md) — trigger, audio, and settings rules.

## License

This project is licensed under the [MIT License](LICENSE). Copyright 2026 TheSkyC.
