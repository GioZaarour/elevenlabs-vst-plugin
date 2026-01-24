cmake_minimum_required(VERSION 3.22)

project(ElevenLabsVST VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add JUCE
add_subdirectory(JUCE)

# Find OpenSSL for HTTPS support
find_package(OpenSSL REQUIRED)

# Plugin target
juce_add_plugin(ElevenLabsVST
    COMPANY_NAME "Cambria"
    PRODUCT_NAME "ElevenLabs VST"
    BUNDLE_ID "com.cambria.elevenlabsvst"
    PLUGIN_MANUFACTURER_CODE Camb
    PLUGIN_CODE Elvn

    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE

    VST3_CATEGORIES "Instrument" "Generator"

    FORMATS VST3 AU Standalone

    COPY_PLUGIN_AFTER_BUILD TRUE
)

# Source files
target_sources(ElevenLabsVST PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/StateSerializer.cpp
    src/ApiClient.cpp
    src/AudioCacheManager.cpp
)

# Include directories
target_include_directories(ElevenLabsVST PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Compile definitions for cpp-httplib
target_compile_definitions(ElevenLabsVST PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# JUCE modules
target_link_libraries(ElevenLabsVST PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    OpenSSL::SSL
    OpenSSL::Crypto
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Platform-specific settings
if(APPLE)
    target_link_libraries(ElevenLabsVST PRIVATE
        "-framework CoreFoundation"
        "-framework Security"
    )
endif()

if(WIN32)
    target_link_libraries(ElevenLabsVST PRIVATE
        ws2_32
        crypt32
    )
endif()

# macOS: Bundle OpenSSL libraries and fix paths for distribution
if(APPLE)
    # Get OpenSSL library paths
    get_target_property(OPENSSL_SSL_LIB OpenSSL::SSL IMPORTED_LOCATION)
    get_target_property(OPENSSL_CRYPTO_LIB OpenSSL::Crypto IMPORTED_LOCATION)

    # Create a post-build script to bundle OpenSSL
    set(BUNDLE_SCRIPT "${CMAKE_BINARY_DIR}/bundle_openssl.sh")
    file(WRITE ${BUNDLE_SCRIPT} "#!/bin/bash
set -e

bundle_openssl() {
    local BUNDLE=\"\$1\"
    local BINARY=\"\$BUNDLE/Contents/MacOS/ElevenLabs VST\"

    if [ ! -f \"\$BINARY\" ]; then
        return
    fi

    mkdir -p \"\$BUNDLE/Contents/Frameworks\"

    # Copy OpenSSL libraries
    cp \"${OPENSSL_SSL_LIB}\" \"\$BUNDLE/Contents/Frameworks/\"
    cp \"${OPENSSL_CRYPTO_LIB}\" \"\$BUNDLE/Contents/Frameworks/\"

    # Get the actual filenames
    SSL_NAME=\$(basename \"${OPENSSL_SSL_LIB}\")
    CRYPTO_NAME=\$(basename \"${OPENSSL_CRYPTO_LIB}\")

    # Fix library paths in main binary
    install_name_tool -change \"${OPENSSL_SSL_LIB}\" \"@loader_path/../Frameworks/\$SSL_NAME\" \"\$BINARY\" 2>/dev/null || true
    install_name_tool -change \"${OPENSSL_CRYPTO_LIB}\" \"@loader_path/../Frameworks/\$CRYPTO_NAME\" \"\$BINARY\" 2>/dev/null || true

    # Fix libssl's reference to libcrypto
    install_name_tool -change \"${OPENSSL_CRYPTO_LIB}\" \"@loader_path/\$CRYPTO_NAME\" \"\$BUNDLE/Contents/Frameworks/\$SSL_NAME\" 2>/dev/null || true

    # Fix library IDs
    install_name_tool -id \"@loader_path/../Frameworks/\$SSL_NAME\" \"\$BUNDLE/Contents/Frameworks/\$SSL_NAME\" 2>/dev/null || true
    install_name_tool -id \"@loader_path/../Frameworks/\$CRYPTO_NAME\" \"\$BUNDLE/Contents/Frameworks/\$CRYPTO_NAME\" 2>/dev/null || true

    # Re-sign
    codesign --force --sign - \"\$BUNDLE/Contents/Frameworks/\$CRYPTO_NAME\" 2>/dev/null || true
    codesign --force --sign - \"\$BUNDLE/Contents/Frameworks/\$SSL_NAME\" 2>/dev/null || true
    codesign --force --deep --sign - \"\$BUNDLE\" 2>/dev/null || true
}

# Bundle for VST3
bundle_openssl \"\$HOME/Library/Audio/Plug-Ins/VST3/ElevenLabs VST.vst3\"

# Bundle for AU
bundle_openssl \"\$HOME/Library/Audio/Plug-Ins/Components/ElevenLabs VST.component\"

echo \"OpenSSL bundled successfully\"
")

    # Add custom target to run after all plugin builds
    add_custom_target(BundleOpenSSL ALL
        COMMAND chmod +x ${BUNDLE_SCRIPT}
        COMMAND ${BUNDLE_SCRIPT}
        DEPENDS ElevenLabsVST_VST3 ElevenLabsVST_AU
        COMMENT "Bundling OpenSSL libraries into plugins"
    )
endif()
