# AGENTS.md - ElevenLabs VST Plugin

## Project Overview

This is a DAW plugin (VST3/AU) built with JUCE that generates music via the ElevenLabs Music API. Users enter a text prompt and the plugin generates audio that can be played back within their DAW.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     PluginEditor (UI)                        │
│  - WaveformDisplay, GenerationDialog, SettingsDialog        │
│  - Runs on JUCE Message Thread                              │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│                   PluginProcessor (Core)                     │
│  - Audio processing, state management                        │
│  - Coordinates all components                                │
└───┬──────────────┬──────────────┬──────────────┬────────────┘
    │              │              │              │
    ▼              ▼              ▼              ▼
┌────────┐  ┌──────────┐  ┌────────────┐  ┌─────────────────┐
│ State  │  │   API    │  │   Cache    │  │  Audio Thread   │
│Serialzr│  │  Client  │  │  Manager   │  │  (lock-free)    │
└────────┘  └──────────┘  └────────────┘  └─────────────────┘
```

## Key Components

### `src/PluginProcessor.h/cpp`
- Main audio processor class
- **Thread model**: Uses atomic pointer swap for buffer hot-swapping (no locks on audio thread)
- Manages playback state (play/stop/loop/position)
- Handles DAW state serialization (`getStateInformation`/`setStateInformation`)

### `src/PluginEditor.h/cpp`
- JUCE GUI with custom components
- `WaveformDisplay` - renders audio waveform with playback position
- `GenerationDialog` - modal for prompt/genre/duration input
- `SettingsDialog` - API key configuration
- Colors defined in `Colors` struct

### `src/ApiClient.h/cpp`
- Background thread HTTP client using cpp-httplib
- **Endpoint**: `POST https://api.elevenlabs.io/v1/music`
- **Auth**: `xi-api-key` header
- Features: exponential backoff retry (1s→2s→4s→8s), rate limit handling, cancellation
- Results delivered via `MessageManager::callAsync()`

### `src/AudioCacheManager.h/cpp`
- Converts MP3 (API response) → WAV (48kHz/24-bit)
- Manages cache directory and history.json
- Handles sample rate conversion with `LagrangeInterpolator`

### `src/StateSerializer.h/cpp`
- Global config: `~/Library/Application Support/ElevenLabsVST/config.json`
- Stores: API key, last genre, last duration
- Per-instance state for DAW project save/restore

## Thread Safety Rules

1. **Audio Thread** (`processBlock`):
   - NO allocations
   - NO blocking locks (use `SpinLock::ScopedTryLockType`)
   - Only atomic operations and buffer copies

2. **Background Thread** (ApiClient):
   - Use `MessageManager::callAsync()` to deliver results to message thread

3. **Buffer Hot-Swap Pattern**:
   ```cpp
   auto* pending = pendingBuffer.exchange(nullptr);
   if (pending) {
       SpinLock::ScopedTryLockType lock(bufferLock);
       if (lock.isLocked()) currentBuffer = pending;
   }
   ```

## Build Instructions

```bash
# Prerequisites (macOS)
brew install openssl cmake

# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build (installs to ~/Library/Audio/Plug-Ins/)
cmake --build build -j8
```

**Output locations:**
- VST3: `~/Library/Audio/Plug-Ins/VST3/ElevenLabs VST.vst3`
- AU: `~/Library/Audio/Plug-Ins/Components/ElevenLabs VST.component`
- Standalone: `build/ElevenLabsVST_artefacts/Release/Standalone/`

## OpenSSL Bundling

The build automatically bundles OpenSSL libraries into the plugin bundle using a post-build script. This is required because:
- DAWs like Ableton run in sandboxed environments
- Homebrew paths (`/opt/homebrew/...`) are not accessible to sandboxed apps
- Libraries are copied to `Contents/Frameworks/` and paths fixed with `install_name_tool`

## Plugin Configuration

```cmake
IS_SYNTH TRUE                    # Appears as instrument
NEEDS_MIDI_INPUT TRUE            # Required for synth in DAWs
VST3_CATEGORIES "Instrument" "Generator"
```

The plugin must be loaded on a **MIDI track** in DAWs (not audio track).

## Validation

```bash
# Test with pluginval
pluginval --validate "~/Library/Audio/Plug-Ins/VST3/ElevenLabs VST.vst3"
```

## API Integration

**Request:**
```json
POST https://api.elevenlabs.io/v1/music
Headers: { "xi-api-key": "<key>", "Content-Type": "application/json" }
Body: { "prompt": "...", "duration_seconds": 30 }
```

**Response:** Binary MP3 audio data

## Known Issues / Gotchas

1. **DAW Loading Issues**: If plugin fails to load, check:
   - OpenSSL libraries bundled in `Contents/Frameworks/`
   - Code signature valid (`codesign --verify`)
   - Loaded on MIDI track (not audio track)

2. **Sample Rate Mismatch**: AudioCacheManager resamples to match DAW sample rate at load time

3. **State Persistence**: Cached audio path stored in DAW project; if cache cleared, audio lost

## Dependencies

- **JUCE 7.x** - Git submodule at `/JUCE`
- **cpp-httplib** - Header-only at `/include/httplib.h`
- **OpenSSL** - System (Homebrew), bundled at build time

## File Locations

| Data | Path |
|------|------|
| Global config | `~/Library/Application Support/ElevenLabsVST/config.json` |
| Audio cache | `~/Library/Application Support/ElevenLabsVST/Cache/` |
| Cache history | `~/Library/Application Support/ElevenLabsVST/Cache/history.json` |
