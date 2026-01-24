# ElevenLabs VST Plugin - Technical Specification

## Project Overview

A DAW plugin (VST3/AU/AAX) that enables users to generate music samples directly within their digital audio workstation using the ElevenLabs Music API. The plugin accepts text prompts and generates audio that is seamlessly integrated into the user's project timeline.

### Vision
Empower music producers to rapidly iterate on ideas using AI-generated music, treating the plugin as a creative instrument within their existing DAW workflow.

---

## Technical Stack

### Core Framework
- **Plugin Framework**: JUCE (latest version)
- **Target Formats**: VST3, AU (macOS), AAX
- **Language**: C++17 or later
- **Build System**: CMake with JUCE integration

### Dependencies
- **HTTP Client**: cpp-httplib (header-only, HTTPS support)
- **Audio Processing**: JUCE built-in audio utilities
- **Threading**: JUCE AsyncUpdater pattern
- **JSON Parsing**: JUCE built-in JSON or nlohmann/json

### Platform Targets
- **Primary**: macOS + Windows (simultaneous support)
- **macOS**: AU + VST3
- **Windows**: VST3
- **Linux**: Not in MVP scope

### Target DAWs for Testing
- Ableton Live (priority 1)
- Reaper (priority 2)

---

## Architecture & Components

### High-Level Architecture
```
┌─────────────────────────────────────────────┐
│         Plugin Processor (Audio Thread)      │
│  - Plays cached audio                        │
│  - Minimal CPU when idle                     │
│  - Receives updates from background thread   │
└─────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────┐
│         Plugin Editor (UI Thread)            │
│  - Modal dialog interface                    │
│  - Prompt input + controls                   │
│  - Preview playback                          │
│  - Error notifications                       │
└─────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────┐
│        API Client Layer                      │
│  - ElevenLabs Music API integration          │
│  - Background thread execution               │
│  - AsyncUpdater for UI callbacks             │
│  - Retry logic with exponential backoff      │
└─────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────┐
│        Audio Cache Manager                   │
│  - Per-project folder storage                │
│  - File path management                      │
│  - Memory-efficient loading (stream from disk)│
│  - History tracking                          │
└─────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────┐
│        State Serialization Layer             │
│  - Save/load plugin state                    │
│  - Audio file path references                │
│  - Persistent prompt history                 │
│  - API key storage (preferences file)        │
└─────────────────────────────────────────────┘
```

### Component Details

#### 1. Plugin Processor
- **Responsibility**: Real-time audio playback
- **Key Requirements**:
  - Play cached audio with near-zero CPU overhead when idle
  - Implement hybrid async model: play placeholder initially, hot-swap when generation completes
  - Handle buffer fills efficiently
  - Thread-safe communication with background generation thread

#### 2. Plugin Editor (UI)
- **Type**: Modal dialog interface
- **Layout**: Single view with all controls visible
- **Controls** (MVP):
  - Text prompt input (multi-line text area)
  - Duration control (seconds or bars)
  - Genre/style selector (dropdown or tags)
  - Generate button (with loading state)
  - Preview transport (play/pause last generation)
  - Status bar (generation progress, errors)

#### 3. API Client Layer
- **Endpoint**: ElevenLabs Music API
- **Features**:
  - HTTPS POST requests with prompt + parameters
  - Background thread execution (not audio thread)
  - AsyncUpdater for thread-safe UI callbacks
  - Exponential backoff retry logic
  - Rate limit handling with auto-queue and countdown timer
  - Network error detection and graceful degradation

#### 4. Audio Cache Manager
- **Storage Location**: Per-project folder structure
  ```
  [DAW Project Folder]/
    ├── [Project Name].als/.rpp/etc.
    └── ElevenLabsCache/
        ├── generation_001.wav
        ├── generation_002.wav
        ├── history.json
        └── ...
  ```
- **File Format**: 48kHz, 24-bit WAV files
- **Features**:
  - Memory-efficient: stream from disk, load to RAM only when playing
  - Persistent history per project (prompt + params + audio file reference)
  - Automatic cleanup of orphaned files

#### 5. State Serialization Layer
- **Plugin State** (saved in DAW project):
  - Current audio file path reference
  - Current prompt and parameters
  - Generation history (prompt + params + file path)
  - User preferences (last used genre, duration settings)

- **API Key Storage**:
  - Stored in plugin preferences file (not in DAW project)
  - Location: `~/Library/Application Support/ElevenLabsVST/` (macOS)
  - Location: `%APPDATA%/ElevenLabsVST/` (Windows)
  - Format: Plain text JSON (user supplies their own key)

---

## Core Features (MVP)

### 1. Text-to-Music Generation
- User opens modal dialog
- Enters text prompt describing desired music
- Selects duration (in seconds)
- Chooses genre/style (dropdown with common options: Electronic, Rock, Jazz, Classical, Hip-Hop, Ambient, Cinematic, etc.)
- Clicks "Generate"
- Plugin displays loading state with progress indicator
- On completion, audio replaces current plugin content
- User can preview generated audio using mini transport

### 2. Persistent History
- All generations saved in project folder
- History view shows:
  - Timestamp
  - Prompt text
  - Duration and genre used
  - Clickable to reload that generation
- History persists across DAW sessions

### 3. Async Generation with Hot-Swap
- Plugin plays silence/placeholder initially
- API call happens in background thread
- When audio arrives, plugin hot-swaps audio buffer
- DAW timeline continues playing smoothly
- No audio dropouts or glitches

### 4. Error Handling
- Visual error notifications in plugin GUI
- Fallback to last successful generation on error
- Retry logic with exponential backoff for transient failures
- Rate limit handling: auto-queue with countdown timer
- Offline mode: graceful degradation (playback only, no new generations)

### 5. API Authentication
- Settings panel for API key input
- Users create ElevenLabs account and provide their own key
- Key stored in plugin preferences (not in project file)
- Validation on entry with test request
- Clear instructions linking to ElevenLabs API documentation

---

## UI/UX Design

### Modal Dialog Specifications
- **Trigger**: Click button in plugin GUI or keyboard shortcut
- **Behavior**:
  - Opens centered over DAW window
  - Modal (blocks interaction with plugin behind it)
  - Can preview last generation while modal is open (mini transport)
  - Closes on "Generate" or "Cancel"

### UI Layout
```
┌─────────────────────────────────────────────────────┐
│  Generate Music with ElevenLabs                  [X]│
├─────────────────────────────────────────────────────┤
│                                                      │
│  Prompt:                                             │
│  ┌────────────────────────────────────────────────┐ │
│  │ Uplifting electronic music with bright synths  │ │
│  │ and energetic drums                            │ │
│  │                                                │ │
│  └────────────────────────────────────────────────┘ │
│                                                      │
│  Duration: [10 seconds ▼]    Genre: [Electronic ▼] │
│                                                      │
│  ┌────────────────────────────────────────────────┐ │
│  │ Preview Last Generation:   [►] Play             │ │
│  └────────────────────────────────────────────────┘ │
│                                                      │
│  Status: Ready                                       │
│                                                      │
│          [Cancel]              [Generate]            │
│                                                      │
└─────────────────────────────────────────────────────┘
```

### Main Plugin GUI (Minimal)
- Display current generation info (prompt, duration, genre)
- Button to open modal dialog: "Generate New"
- Simple waveform display of loaded audio
- Status indicator (idle, generating, error)
- Settings gear icon for API key configuration

### Loading States
- Modal shows spinner/progress bar during generation
- Status text: "Generating... (15s elapsed)"
- Cancel button remains active to abort request
- Disable "Generate" button during active generation

### Error States
- Red banner at top of modal with error message
- Examples:
  - "Network error. Retrying in 5s..."
  - "Rate limit reached. Retrying in 2m 30s..."
  - "API key invalid. Please check settings."
  - "Generation failed. Using last successful audio."
- Actionable buttons where appropriate (e.g., "Open Settings", "Retry Now")

---

## API Integration

### ElevenLabs Music API
- **Endpoint**: `/v1/music-generation` (or as documented by ElevenLabs)
- **Method**: POST
- **Authentication**: Bearer token (user's API key)
- **Request Body** (MVP):
  ```json
  {
    "prompt": "string",
    "duration_seconds": 10,
    "style": "electronic"
  }
  ```
- **Response**: Audio file (MP3 or WAV)
- **Expected Latency**: 15-60 seconds depending on duration

### Request Flow
1. User clicks "Generate" in modal
2. Validate inputs (prompt not empty, duration valid)
3. Show loading state
4. Create background thread (not audio thread)
5. Make HTTPS POST request via cpp-httplib
6. Poll or wait for response (with timeout: 120s)
7. On success:
   - Download audio data
   - Save to project cache folder as WAV
   - Convert to 48kHz/24-bit if necessary
   - Trigger AsyncUpdater to notify audio processor
   - Audio processor hot-swaps buffer on next render call
   - Update UI with success state
8. On failure:
   - Trigger AsyncUpdater with error
   - Show error in UI
   - Execute retry logic if applicable
   - Fall back to last successful generation for playback

### Error Handling Specifics
- **Network Timeout**: Retry with exponential backoff (1s, 2s, 4s, 8s, max 3 retries)
- **API Errors** (4xx/5xx):
  - 401 Unauthorized: Show "Invalid API key" error, prompt settings
  - 429 Rate Limited: Parse `Retry-After` header, show countdown timer, auto-retry
  - 500 Server Error: Retry with backoff
  - Other: Show error message, allow manual retry
- **Offline**: Detect network unavailable, show "Offline" status, disable generation

### Rate Limiting Strategy
- Parse `X-RateLimit-Remaining` and `Retry-After` headers
- When limit hit:
  - Queue the request
  - Show countdown timer in UI: "Rate limit reached. Retrying in 2m 30s"
  - Auto-submit when timer expires
  - Allow user to cancel queued request

---

## Audio Handling

### Format Specifications
- **API Output**: Likely 44.1kHz or 48kHz, 16-bit or 24-bit (depends on ElevenLabs)
- **Plugin Internal**: Convert everything to 48kHz, 24-bit WAV
- **Storage**: WAV files in project cache folder
- **Playback**: Let DAW handle final sample rate conversion to project settings

### Hot-Swap Implementation
```cpp
// Pseudo-code
class AudioProcessor {
    std::atomic<AudioBuffer*> currentBuffer{nullptr};
    AudioBuffer placeholderBuffer; // silence or noise

    void processBlock(AudioBuffer& buffer) {
        AudioBuffer* buf = currentBuffer.load();
        if (buf == nullptr) {
            // Play placeholder until generation completes
            buffer.copyFrom(placeholderBuffer);
        } else {
            // Play generated audio
            buffer.copyFrom(*buf);
        }
    }

    void onGenerationComplete(AudioBuffer* newBuffer) {
        // Called from background thread via AsyncUpdater
        currentBuffer.store(newBuffer);
    }
};
```

### Memory Management
- Stream audio from disk for preview (low memory mode)
- Load into RAM only when playback starts in DAW
- Unload after 30s of silence to free memory
- Maximum cached audio in RAM: 2 minutes @ 48kHz/24-bit (~33 MB)

### Audio Processing Pipeline
1. API returns audio (format unknown)
2. Parse audio format (using JUCE AudioFormatReader)
3. Resample to 48kHz if necessary (JUCE ResamplingAudioSource)
4. Convert to 24-bit if necessary
5. Save as WAV in cache folder
6. Load into AudioBuffer for playback
7. On playback request: copy to plugin's audio output

---

## State Management & Persistence

### Plugin State (Saved in DAW Project)
```cpp
struct PluginState {
    String currentAudioFilePath;
    String currentPrompt;
    int currentDurationSeconds;
    String currentGenre;

    Array<GenerationHistoryItem> history;
};

struct GenerationHistoryItem {
    String prompt;
    int duration;
    String genre;
    String audioFilePath; // relative to project folder
    Time timestamp;
};
```

### Serialization Format
- JUCE `ValueTree` or JSON
- Saved via `getStateInformation()` / `setStateInformation()`
- Compact format to minimize project file bloat

### Loading Strategy
- On project load:
  1. Read plugin state from DAW
  2. Resolve audio file paths (relative to project folder)
  3. Check if files exist
  4. If exists: load audio buffer (lazy load on playback)
  5. If missing: show warning in UI, audio is silent until regenerated

### API Key Storage
- **Location**:
  - macOS: `~/Library/Application Support/ElevenLabsVST/config.json`
  - Windows: `%APPDATA%/ElevenLabsVST/config.json`
- **Format**:
  ```json
  {
    "api_key": "sk_...",
    "last_genre": "Electronic",
    "last_duration": 10
  }
  ```
- **Security**: Plain text (user is responsible for key security)
- **Future Enhancement**: Use system keychain for better security

---

## Performance Requirements

### CPU Usage
- **Idle (not generating)**: < 0.1% CPU
  - Achieved by:
    - No polling or timers when idle
    - Simple buffer copy in audio thread
    - No GUI repaints unless state changes

- **Generating**: < 5% CPU
  - Background thread does HTTP work
  - Audio thread remains unaffected

### Memory Usage
- **Baseline**: < 50 MB (plugin + JUCE framework)
- **Per Generation**: ~16 MB per minute of 48kHz/24-bit audio
- **Max Cached in RAM**: 100 MB (auto-unload old audio)

### UI Responsiveness
- Modal open/close: < 100ms
- Button clicks: < 50ms to show feedback
- No blocking on UI thread (all API calls on background thread)

### Audio Latency
- Playback latency: Match DAW buffer size (no additional latency)
- Hot-swap latency: < 1 audio buffer (e.g., 10ms @ 512 samples)

---

## Platform Support & Distribution

### Build Targets
- macOS: 10.13 High Sierra or later
- Windows: Windows 10 or later
- Architectures:
  - macOS: x86_64 + ARM64 (Universal Binary)
  - Windows: x64

### Installation
- **Format**: Installer package
  - macOS: PKG installer
  - Windows: MSI installer (or NSIS)
- **Installation Paths** (standard plugin locations):
  - macOS VST3: `~/Library/Audio/Plug-Ins/VST3/`
  - macOS AU: `~/Library/Audio/Plug-Ins/Components/`
  - Windows VST3: `C:\Program Files\Common Files\VST3\`

### Distribution
- Direct download from website
- GitHub Releases
- Installer handles copying binaries and setting permissions
- Code signing:
  - macOS: Developer ID Application certificate (required for Gatekeeper)
  - Windows: Authenticode signing (optional but recommended)

---

## Testing Strategy

### Manual Testing
- **DAW Compatibility Matrix**:
  | DAW | macOS AU | macOS VST3 | Windows VST3 |
  |-----|----------|------------|--------------|
  | Ableton Live | ✓ | ✓ | ✓ |
  | Reaper | ✓ | ✓ | ✓ |

- **Test Cases**:
  1. Install plugin, launch DAW, load plugin
  2. Enter API key, validate
  3. Generate audio with various prompts
  4. Save project, close DAW, reopen - verify audio loads
  5. Test error cases: invalid key, network offline, rate limit
  6. Test history: generate multiple times, switch between them
  7. Test playback: play generated audio in timeline
  8. Test CPU usage: monitor when idle and generating
  9. Test across OS versions: macOS 12-14, Windows 10-11

### Automated Testing
- **Unit Tests**:
  - API client logic (mock HTTP responses)
  - Audio cache manager (file I/O)
  - State serialization (save/load)
- **Plugin Validation**:
  - Use `pluginval` tool (VST3/AU validator)
  - Run on both macOS and Windows
  - Must pass all tests before release

### Continuous Integration
- GitHub Actions or similar
- Build matrix: [macOS x86_64, macOS ARM64, Windows x64]
- Run unit tests on each commit
- Run pluginval on pull requests
- Automated installer generation

---

## Documentation Requirements

### Developer README
- **Setup Instructions**:
  1. Clone repository
  2. Install dependencies: JUCE, CMake, cpp-httplib
  3. Build with CMake: `cmake -B build && cmake --build build`
  4. Open in IDE: Xcode (macOS), Visual Studio (Windows)

- **Build Commands**:
  ```bash
  # Configure
  cmake -B build -DCMAKE_BUILD_TYPE=Release

  # Build
  cmake --build build --config Release

  # Run tests
  ctest --test-dir build

  # Install (copies to plugin folders)
  cmake --install build
  ```

- **Project Structure**:
  ```
  elevenlabs-vst-plugin/
  ├── CMakeLists.txt
  ├── README.md
  ├── SPEC.md (this file)
  ├── src/
  │   ├── PluginProcessor.h/cpp
  │   ├── PluginEditor.h/cpp
  │   ├── ApiClient.h/cpp
  │   ├── AudioCacheManager.h/cpp
  │   └── StateSerializer.h/cpp
  ├── include/
  │   └── (headers if needed)
  ├── tests/
  │   └── (unit tests)
  ├── resources/
  │   └── (UI assets, icons)
  └── installers/
      ├── macos/
      └── windows/
  ```

### Troubleshooting Documentation
- **Common Issues**:
  1. "Plugin not appearing in DAW"
     - Solution: Check installation path, rescan plugins in DAW
  2. "Invalid API key" error
     - Solution: Verify key at elevenlabs.io, check for typos
  3. "Generation failed" errors
     - Solution: Check network connection, verify API quota
  4. "Audio not loading after reopening project"
     - Solution: Ensure project folder and cache folder are in same location
  5. High CPU usage
     - Solution: Check for stuck background threads, restart DAW

- **DAW-Specific Issues**:
  - Ableton Live: [specific quirks]
  - Reaper: [specific quirks]

---

## Future Enhancements (Post-MVP)

### Version 1.1
- Prompt strength/guidance scale parameter
- Model selection (if ElevenLabs offers multiple models)
- Export audio to arbitrary location (outside DAW)
- Drag-and-drop prompts from history to timeline

### Version 1.2
- Preset system: curated prompts for common styles
- Batch generation: queue multiple prompts
- Variation generator: create variations of existing generation
- MIDI parameter control: map MIDI CC to duration/style

### Version 2.0
- Voice-to-voice conversion support (if API allows)
- Real-time parameter modulation during playback
- Multi-layer mode: stack multiple generations
- Global prompt library (sync across projects)
- Collaborative sharing: export/import prompts with audio

### Integration Enhancements
- VST3 parameter automation (duration, style as automatable params)
- AAX support for Pro Tools users
- Linux version (if demand exists)
- Standalone app version (no DAW required)

---

## Security & Privacy Considerations

### API Key Security
- Never log or transmit API key except to ElevenLabs servers
- Warn users not to commit config files to version control
- Consider encryption in future version (system keychain)

### Network Security
- All API communication over HTTPS
- Verify SSL certificates
- Timeout connections after 120s to prevent hangs

### User Privacy
- No telemetry or analytics in MVP
- No data collected or sent except to ElevenLabs API
- Prompts and audio stay local on user's machine

---

## Success Metrics

### MVP Launch Goals
- Successfully install and run in Ableton Live and Reaper
- Generate audio from prompts with < 5% failure rate
- Average generation time < 60s
- Zero DAW crashes attributable to plugin
- Positive initial user feedback (if beta testing)

### Performance Benchmarks
- CPU: < 0.1% idle, < 5% generating
- Memory: < 100 MB total
- UI: < 100ms modal open time
- Audio: zero dropouts or glitches during hot-swap

---

## Timeline Considerations

This specification avoids timeline estimates per project guidelines. However, key milestones include:

1. **Foundation**: Project setup, CMake config, basic JUCE plugin scaffold
2. **API Integration**: Implement API client, test with real ElevenLabs API
3. **Audio Pipeline**: Cache manager, hot-swap logic, format conversion
4. **UI Development**: Modal dialog, controls, preview transport
5. **State Management**: Serialization, history, persistence
6. **Error Handling**: All error cases, retry logic, offline mode
7. **Testing**: Manual DAW testing, pluginval, unit tests
8. **Polish**: Performance optimization, UI refinement
9. **Distribution**: Installers, code signing, documentation
10. **Release**: Beta testing, final QA, launch

---

## Open Questions & Decisions Needed

1. **Exact ElevenLabs Music API endpoint**: Verify documentation and endpoint structure
2. **API response format**: Confirm if audio is returned directly or via URL
3. **Audio format from API**: Determine native format (MP3, WAV, sample rate, bit depth)
4. **Rate limits**: Understand ElevenLabs' specific rate limiting policies
5. **Pricing model for users**: Ensure users understand they pay ElevenLabs directly
6. **Genre/style taxonomy**: Finalize list of supported genres for dropdown
7. **Duration limits**: Check API constraints on min/max generation length
8. **Beta testing group**: Identify initial testers with Ableton/Reaper access

---

## Conclusion

This specification defines a focused, achievable MVP for an ElevenLabs Music Generation VST plugin. The architecture prioritizes:

- **Simplicity**: Clear separation of concerns, minimal dependencies
- **Performance**: Near-zero CPU when idle, efficient async generation
- **Reliability**: Robust error handling, graceful degradation
- **User Experience**: Seamless DAW integration, intuitive modal interface
- **Maintainability**: Clean codebase structure, comprehensive testing

The MVP scope (prompt + duration + genre only) ensures rapid development while delivering core value. Future enhancements provide a clear roadmap for growth based on user feedback.
