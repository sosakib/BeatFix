cmake_minimum_required(VERSION 3.28)
project(BeatFix VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(
        /permissive-
        /utf-8
        /fp:precise
        /W4
        /EHsc
    )
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS JUCE_MODAL_LOOPS_PERMITTED=1)
endif()

# Global include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# 1. libebur128
add_library(ebur128 STATIC
    third_party/libebur128/ebur128.c
)
target_include_directories(ebur128 PUBLIC third_party/libebur128)
target_compile_definitions(ebur128 PRIVATE _USE_MATH_DEFINES)
if(MSVC)
    target_compile_options(ebur128 PRIVATE /W1)
endif()

# 2. bf_dsp static library
add_library(bf_dsp STATIC
    src/bf_dsp/grid.cpp
    src/bf_dsp/smooth.cpp
    src/bf_dsp/tempo_fit.cpp
    src/bf_dsp/warp.cpp
    src/bf_dsp/peaks.cpp
    src/bf_dsp/dither.cpp
    src/bf_dsp/render_plan.cpp
    src/bf_dsp/stretch_signalsmith.cpp
    src/bf_dsp/stretch_resample.cpp
    src/bf_dsp/loudness.cpp
)
target_include_directories(bf_dsp PUBLIC
    src/bf_dsp
    third_party
)
target_link_libraries(bf_dsp PUBLIC ebur128)

# 3. bf_io static library
add_library(bf_io STATIC
    src/bf_io/pcm_cache.cpp
    src/bf_io/decode.cpp
    src/bf_io/export.cpp
)
target_include_directories(bf_io PUBLIC
    src/bf_io
    third_party
)
target_link_libraries(bf_io PUBLIC bf_dsp bcrypt)

# 4. bf_analysis static library
set(ONNXRUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime")
add_library(bf_analysis STATIC
    src/bf_analysis/dsp_tracker.cpp
    src/bf_analysis/beatthis_onnx.cpp
)
target_include_directories(bf_analysis PUBLIC
    src/bf_analysis
    "${ONNXRUNTIME_DIR}/include"
    third_party
)
target_link_libraries(bf_analysis PUBLIC
    bf_dsp
    bf_io
    "${ONNXRUNTIME_DIR}/lib/onnxruntime.lib"
)

# 5. bf_engine static library
add_library(bf_engine STATIC
    src/bf_engine/project_json.cpp
    src/bf_engine/render_scheduler.cpp
    src/bf_engine/session.cpp
)
target_include_directories(bf_engine PUBLIC
    src/bf_engine
    third_party
)
target_link_libraries(bf_engine PUBLIC
    bf_dsp
    bf_io
    bf_analysis
)

# 6. Test suite executable (Catch2)
add_executable(bf_tests
    tests/unit/dsp_math_test.cpp
    tests/integration/session_integration_test.cpp
    third_party/catch2/catch_amalgamated.cpp
)
target_include_directories(bf_tests PRIVATE
    src
    third_party
)
if(MSVC)
    set_source_files_properties(third_party/catch2/catch_amalgamated.cpp PROPERTIES COMPILE_FLAGS "/W1")
endif()
target_link_libraries(bf_tests PRIVATE
    bf_engine
    bf_analysis
    bf_io
    bf_dsp
)

# Copy onnxruntime.dll to test binary directory
add_custom_command(TARGET bf_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
    $<TARGET_FILE_DIR:bf_tests>
)

# 7. bftool CLI executable
add_executable(bftool
    apps/beatfix-cli/main.cpp
)
target_include_directories(bftool PRIVATE
    src
    third_party
)
target_link_libraries(bftool PRIVATE
    bf_engine
    bf_analysis
    bf_io
    bf_dsp
)

# Copy onnxruntime.dll to bftool directory
add_custom_command(TARGET bftool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
    $<TARGET_FILE_DIR:bftool>
)

enable_testing()
add_test(NAME bf_tests COMMAND bf_tests)

# 8. JUCE 9 GUI Application
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/juce EXCLUDE_FROM_ALL)

juce_add_gui_app(BeatFix
    PRODUCT_NAME "BeatFix"
    COMPANY_NAME "BeatFix"
    VERSION "0.1.0"
)

target_sources(BeatFix PRIVATE
    apps/beatfix/Main.cpp
    apps/beatfix/MainComponent.cpp
    apps/beatfix/WaveformView.cpp
    apps/beatfix/AudioPlayer.cpp
    apps/beatfix/ExportModal.cpp
)

target_include_directories(BeatFix PRIVATE
    src
    third_party
    apps/beatfix
)

target_link_libraries(BeatFix PRIVATE
    bf_engine
    bf_analysis
    bf_io
    bf_dsp
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils
)

# Copy onnxruntime.dll to BeatFix executable directory
add_custom_command(TARGET BeatFix POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
    $<TARGET_FILE_DIR:BeatFix>
)


