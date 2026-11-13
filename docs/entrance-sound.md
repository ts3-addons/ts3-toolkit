# Entrance sound

This document is the behavior contract for the personal entrance sound feature.
Implementation, tests, and user-facing text must follow the rules defined here.

## Summary

When the local user enters a channel, the plugin plays a user-selected sound by
mixing it into the custom capture stream. Every other client in the channel
hears it as part of the user's voice. No server-side component is involved.

## Trigger semantics

The feature triggers from `ts3plugin_onClientMoveEvent` when all of the following
hold:

1. The moved client is the local client (the event's client ID equals the local
   client ID returned by `getClientID`).
2. The destination channel is valid (non-zero channel ID). Disconnects and
   leave events never trigger.
3. The feature is enabled and a sound file is configured.
4. The controller is not in the cooldown state.

Both self-initiated moves and moves performed by another client (for example an
administrator moving the user) count as entering a channel.

## Exclusions

- Movement of other clients is ignored.
- Events that do not change the channel are ignored.
- Triggers arriving during cooldown are dropped.
- If the configured file is missing, unreadable, or cannot be decoded, no sound
  is played and the microphone continues to pass through unchanged.

## State machine

```
Idle --(valid enter event)--> Pending --> Playing --(sound exhausted)--> Cooldown
 ^                                                             |
 +-----------------------(cooldown elapsed)--------------------+
```

- **Idle**: waiting for a valid enter event.
- **Pending**: waiting for the configured delay; repeated entries are ignored.
  Leaving or disconnecting cancels the pending sound.
- **Playing**: decoded sound frames are being mixed into the pump output.
- **Cooldown**: new enter events are ignored until the cooldown window elapses.

The microphone path is active in every state; the state machine only controls the
sound overlay.

## Configuration contract

The settings are persisted through the storage layer with a versioned schema.
The current schema version is `2`. Version 1 documents migrate with zero delay.

| Key | Type | Range / constraint | Default |
|---|---|---|---|
| `enabled` | boolean | `true` / `false` | `true` |
| `sound_file` | string | filesystem path; empty means no sound configured | empty |
| `volume` | float | `0.0` to `1.0` | `0.8` |
| `cooldown_seconds` | integer | `0` to `300` | `5` |
| `local_monitor` | boolean | `true` / `false` | `false` |
| `delay_ms` | integer | `0` to `30000` milliseconds | `0` |

Rules:

- Values are validated and normalized at the storage boundary. Out-of-range
  values are rejected and the previous valid value is retained rather than
  clamped silently.
- `sound_file` is resolved against the plugin's data directory when relative.
- Paths outside the permitted data directory or paths that fail decoding are
  rejected when the setting is saved.
- The meaning of a released key is never redefined; future changes add keys or
  migrate the schema.

## Audio constraints

- The custom device and the mixed output are 48 kHz mono, 16-bit signed PCM.
- The source file is decoded and, when needed, resample/converted to that
  format before playback.
- Sound duration is capped at 30 seconds; longer files are rejected.
- The sound is scaled by `volume` before mixing, and the mixed frame is clamped
  to the 16-bit range to prevent clipping.
- Supported containers/codecs follow the miniaudio decoder capability set:
  WAV, MP3, and FLAC.

## Local monitor

When `local_monitor` is enabled, the plugin also plays the sound through the
local playback device at the same time so the user can confirm it fired. This is
a local-only convenience and does not affect the capture stream.

## Failure modes

| Failure | Behavior |
|---|---|
| Sound file missing or unreadable | No overlay; microphone passes through; one bounded error log |
| Decoder error or unsupported format | Setting rejected on save, or overlay skipped if it fails later |
| Pump timing error | Custom device is stopped safely; the user is not blocked from reconnecting |
| Repeated enter events | Cooldown suppresses overlapping playback |
