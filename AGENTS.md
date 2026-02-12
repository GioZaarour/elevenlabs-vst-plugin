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
- **Instance UUID**: Each plugin instance is assigned a `juce::Uuid` (lazy-generated on first save or generation). Used to scope sample history per DAW project.
- **Thread-safe buffer access for UI**: `copyAudioBufferTo()` briefly locks to copy the shared_ptr, then copies audio data without the lock. `bufferVersion` (atomic int, incremented on every buffer swap) lets the editor detect changes without polling the pointer.

### `src/PluginEditor.h/cpp`
- JUCE GUI with custom components; inherits `DragAndDropContainer` for drag-to-DAW support
- **Resizable window**: min 400x350, default 500x450, max 1200x900; all layout uses proportional scaling (`scale = min(w/500, h/450)`)
- `WaveformDisplay` - renders audio waveform with playback position indicator; has its own 60fps Timer for loading animation (gradient sweep); handles mouse events for click-to-scrub and hold-to-drag (300ms threshold). Stores its own `audioBufferCopy` (safe copy) instead of a raw pointer into the processor.
- `GenerationDialog` - modal for prompt/genre/duration input
- `SettingsDialog` - API key, history scope toggle ("This Project" / "All Projects"), and Clear Cache button
- **History dropdown** (`ComboBox`) above waveform: shows past generations as "Genre (Duration) - Date", sorted newest-first. Scoped to current project by default (filtered by instance UUID), or all projects via settings.
- **Missing audio recovery**: When a cached file no longer exists, the Generate button becomes "Regenerate" and re-uses the original prompt/genre/duration.
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
- **Per-project history**: Each `HistoryEntry` stores a `projectUuid` (set at generation time). `getHistoryForProject(uuid)` returns only entries for that project.
- **Concurrent-safe history file**: Uses `juce::InterProcessLock` for history.json reads/writes (multiple plugin instances may run in different DAWs). Writes use atomic temp-file-then-rename.

### `src/StateSerializer.h/cpp`
- Global config: `~/Library/Application Support/ElevenLabsVST/config.json`
- Stores: API key, last genre, last duration, `showAllSamples` (history scope toggle)
- Per-instance state for DAW project save/restore, now includes `instanceUuid` for project-scoped history

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
       if (lock.isLocked()) {
           currentBuffer = pending;
           bufferVersion.fetch_add(1);  // Signal UI that buffer changed
       }
   }
   ```

4. **UI Buffer Access**: The editor uses `copyAudioBufferTo()` + `bufferVersion` to safely get audio data for waveform rendering. WaveformDisplay stores its own `audioBufferCopy` to avoid dangling pointers.

## Per-Project Identity Model

Each plugin instance has a stable UUID (`instanceUuid`) that persists across DAW sessions:
- Generated lazily on first `getStateInformation()` or `startGeneration()`
- Stored in DAW project state via `StateSerializer::PluginState`
- Stamped on every `HistoryEntry.projectUuid` at generation time
- Used by the history dropdown to show only samples created by this project ("This Project" scope, the default)
- Users can switch to "All Projects" scope in Settings to see everything

This means each DAW project's plugin instance sees its own history by default, while shared cache storage is used under the hood.

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

3. **State Persistence**: Cached audio path stored in DAW project; if cache cleared, audio lost (UI now detects this and offers one-click "Regenerate")

4. **History File Contention**: Multiple DAW instances share `history.json`. The `InterProcessLock` prevents corruption but operations should be kept brief.

5. **Drag-to-DAW**: Uses `performExternalDragDropOfFiles` with a 300ms hold threshold. Click = scrub, hold = drag. Requires `PluginEditor` to inherit `DragAndDropContainer`.

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
