# Multimedia and persistence foundation (0.21.0)

## Scope

Trusted native desktop fixtures only. No Xbox ABI, guest audio commands, game controller service, external executable loading or game execution. The existing GPU fixture remains separate from playback and input. No keyboard/mouse, streaming audio, asynchronous file I/O, general file handles or combined interactive application in this release.

## PCM and audio

Portable signed PCM16 interleaved stereo at 48 kHz. Two equal-sized streams mix using Q15 gains (0..32768), 64-bit intermediates and saturation. The one-second fixture combines faded 440 Hz and 660 Hz tones at low amplitude. On Windows, XAudio2 creates mastering and source voices, submits a single buffer, waits up to six seconds for consumption, checks sample-frame count and destroys the voices before releasing backing storage. This verifies engine progress, not speaker audibility. No audio endpoint produces an explicit failure. Audio/input hardware checks are intentionally excluded from automated CTest.

## Input

XInput scans slots 0..3, polls the first connected device for 1..30 seconds, prints changed packets, normalizes radial stick magnitude and trigger thresholds. No controller returns code 2; no simulated hardware pass. Optional rumble is low intensity for 150 ms, with a stop attempt during normal and exceptional exit. Disconnect or device error may prevent stop confirmation. No background keyboard capture or persistent input logging.

## Storage

`app::Runtime` accepts one optional host-selected snapshot path. `Persist{name}` stores the bytes of an existing memory-backed key to this path; `Restore{name}` loads it under the requested key subject to existing storage limits. Key names never become paths. Snapshot format: eight magic/version bytes (`RVSAVE`, 1, 0), four little-endian payload-length bytes, four little-endian CRC32 bytes, followed by up to 1 MiB of data. Unrelated and corrupt existing files are rejected rather than overwritten. Writes stage in a newly created directory alongside the destination and replace it using local filesystem rename or MoveFileExW. Single writer, trusted local directory, no adversarial path-race protection, no encryption/authentication or guaranteed power-loss durability. Parent directory must exist. Runtime shutdown clears temporary data, not the explicit snapshot.

## Public references

- https://learn.microsoft.com/en-us/windows/win32/xaudio2/how-to--play-a-sound-with-xaudio2
- https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2sourcevoice-getstate
- https://learn.microsoft.com/en-us/windows/win32/xinput/getting-started-with-xinput
- https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw

No external implementation source bundled.

## v0.21.1 audio hotfix

XAudio2 SamplesPlayed resets at an end-of-stream boundary. Completion is now confirmed with OnStreamEnd and zero queued buffers, with voice errors and timeout retained. The reported submitted frame count is not a post-stream cursor. Human confirmation is needed for audibility. See [voice state](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/ns-xaudio2-xaudio2_voice_state) and [OnStreamEnd](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2voicecallback-onstreamend).
