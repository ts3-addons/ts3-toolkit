# miniaudio (vendored)

Single-header audio playback and capture library used for hardware microphone
capture, sound file decoding, and sample format/rate conversion.

- Source: https://github.com/mackron/miniaudio
- Pinned version: `0.11.26`
- Upstream file: `miniaudio.h` (unmodified)

## Usage

The implementation is instantiated in exactly one translation unit by defining
`MINIAUDIO_IMPLEMENTATION` before including the header. All other translation
units include the header for declarations only.

## License

Choice of public domain or MIT-0 (MIT No Attribution), as stated in the header
and by the upstream author David Reid.
