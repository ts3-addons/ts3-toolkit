# Architecture

This document defines the layer boundaries, data flow, threading model, and
error handling rules for TS3Toolkit. It is the contract that implementations
must follow.

## Goals

- Keep the TeamSpeak 3 SDK and platform APIs at the edges so the core logic can
  be understood and tested in isolation.
- Make the microphone path always work: feature failures must never break the
  user's ability to speak.
- Keep the audio path deterministic and bounded in latency and memory.

## Layers and dependency rule

Dependencies point inward only. A layer may include headers from layers below
it and must never depend on a layer above it.

```
plugin        TeamSpeak 3 entry points, exported functions, event callbacks
  |
entrance      feature controller: state machine, scheduling, cooldown
  |
audio         miniaudio adapter: capture, decode, resample
config        versioned persistent settings and validation
  |
core          SDK-free primitives: PCM types, ring buffer, mixer, math
```

- `core` depends on nothing in this project and includes no TS3 or platform
  headers beyond the standard library.
- `audio` and `config` are adapters. They translate between core types and the
  external library or storage format.
- `plugin` is the only layer that includes TS3 plugin SDK headers.
- `platform` holds small platform-specific helpers used across layers; it must
  not contain feature logic.

## Audio data flow

```
hardware microphone
      |  (miniaudio capture, native format)
      v
  resample/convert to 48 kHz mono PCM
      |
      +----> mixer: add entrance sound frame, clamp to 16-bit range
                        ^
entrance sound file -> decode -> resample -> queued PCM
      |
      v
processCustomCaptureData (10 ms frames, 480 samples at 48 kHz)
      |
      v
TeamSpeak preprocessor, Opus encoder, network
```

The custom device is registered for capture only at 48 kHz mono. The pump always
produces frames at a fixed cadence; when no entrance sound is queued, the
microphone passes through unchanged.

When local monitoring is enabled, the pump also sends the volume-adjusted
overlay samples to a bounded single-producer/single-consumer buffer. A separate
playback device drains that buffer to the system default output. Microphone
samples are never routed to the monitor. If playback cannot start, capture and
transmission continue without monitoring.

## Threading model

- **Client callback thread**: runs `ts3plugin_*` event functions. Callbacks only
  validate inputs and hand work to the controller; they never block on I/O or
  audio.
- **Audio pump thread**: owns the custom device timing. Each 10 ms iteration it
  pulls a microphone frame, mixes any queued sound, and calls
  `processCustomCaptureData`.
- **Capture/decoder work**: microphone capture runs through miniaudio's own
  worker thread feeding a ring buffer; sound file decoding happens before a
  sound is scheduled, never on the pump thread.

Shared state crosses threads through bounded buffers and mutexes. The pump
thread never allocates memory or waits on unbounded locks.

## Error handling

- Fail closed: invalid configuration, missing files, or decoder errors disable
  the affected feature and leave the microphone path untouched.
- Every external API return value is checked and mapped to a bounded internal
  status; errors are reported once with a short, bounded log message.
- Resource lifetimes are explicit: every started device, allocated buffer, or
  registered custom device has a matching stop/free/unregister on shutdown.

## Resource bounds

- Audio format: 16-bit signed PCM, 48 kHz mono on the custom device.
- Entrance sounds are length-limited and fully decoded into bounded buffers;
  oversized or malformed inputs are rejected at the configuration boundary.
- A cooldown window prevents repeated triggers from queueing overlapping sounds.
