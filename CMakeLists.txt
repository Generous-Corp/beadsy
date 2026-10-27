cmake_minimum_required(VERSION 3.24)
project(Beadsy VERSION 1.0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Pulp CONFIG REQUIRED)

option(BEADSY_USE_REVIEWED_PANEL_ARTWORK
       "Embed the reviewed Beadsy panel vector asset"
       ON)

# The converted panel is an authoring-time, rights-gated vector asset. Embed it
# into the editor so installed plugins never read a developer checkout. A clean
# checkout may not contain the probe asset while its rights review is pending;
# use a deterministic neutral panel in that case so DSP and headless builds do
# not depend on an ignored developer artifact.
set(BEADSY_PANEL_ASSET "${CMAKE_CURRENT_SOURCE_DIR}/provenance/generated/beads-panel-beadsy.svg")
set(BEADSY_PANEL_PNG "${CMAKE_CURRENT_SOURCE_DIR}/provenance/generated/beads-panel.png")
# The Illustrator export uses clip paths and compound paths that are not
# rendered consistently by every Skia SVG backend.  Prefer the authoring-time
# rasterization after removing its red construction circles; retain the SVG
# path below as the import/design source and fallback for clean checkouts.
set(BEADSY_PANEL_CLEAN_PNG "${CMAKE_CURRENT_SOURCE_DIR}/provenance/generated/beads-panel-clean.png")
if(BEADSY_USE_REVIEWED_PANEL_ARTWORK AND EXISTS "${BEADSY_PANEL_CLEAN_PNG}")
    file(READ "${BEADSY_PANEL_CLEAN_PNG}" BEADSY_PANEL_PNG_HEX HEX)
    string(REGEX REPLACE "([0-9A-Fa-f][0-9A-Fa-f])" "0x\\1," BEADSY_PANEL_PNG_BYTES "${BEADSY_PANEL_PNG_HEX}")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/beadsy_panel_png.hpp.in
                   ${CMAKE_CURRENT_BINARY_DIR}/beadsy_panel_png.hpp @ONLY)
    execute_process(COMMAND base64 -i "${BEADSY_PANEL_CLEAN_PNG}"
                    OUTPUT_VARIABLE BEADSY_PANEL_PNG_B64
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REPLACE "\n" "" BEADSY_PANEL_PNG_B64 "${BEADSY_PANEL_PNG_B64}")
    set(BEADSY_PANEL_SVG [=[
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200.693 364.252">
  <image href="data:image/png;base64,@BEADSY_PANEL_PNG_B64@" x="0" y="0" width="200.693" height="364.252" preserveAspectRatio="none"/>
</svg>]=])
    string(CONFIGURE "${BEADSY_PANEL_SVG}" BEADSY_PANEL_SVG @ONLY)
    set(BEADSY_PANEL_SOURCE_STATUS "authorized Beads panel artwork (rasterized Illustrator import)")
elseif(BEADSY_USE_REVIEWED_PANEL_ARTWORK AND EXISTS "${BEADSY_PANEL_ASSET}")
    file(READ "${BEADSY_PANEL_ASSET}" BEADSY_PANEL_SVG)
    set(BEADSY_PANEL_SOURCE_STATUS "authorized Beads panel artwork")
elseif(BEADSY_USE_REVIEWED_PANEL_ARTWORK AND EXISTS "${BEADSY_PANEL_PNG}")
    # The authorized Illustrator panel is rasterized at build time because the
    # checked-in source is .ai and Skia's SVG loader cannot consume Illustrator
    # directly. Keep the original aspect ratio and let native controls sit on
    # top of the straight-on panel image.
    execute_process(COMMAND base64 -i "${BEADSY_PANEL_PNG}"
                    OUTPUT_VARIABLE BEADSY_PANEL_PNG_B64
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REPLACE "\n" "" BEADSY_PANEL_PNG_B64 "${BEADSY_PANEL_PNG_B64}")
    set(BEADSY_PANEL_SVG [=[
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200.693 364.252">
  <image href="data:image/png;base64,@BEADSY_PANEL_PNG_B64@" x="0" y="0" width="200.693" height="364.252" preserveAspectRatio="none"/>
</svg>]=])
    string(CONFIGURE "${BEADSY_PANEL_SVG}" BEADSY_PANEL_SVG @ONLY)
    set(BEADSY_PANEL_SOURCE_STATUS "reviewed Beads panel artwork (private opt-in)")
else()
    # Keep the product useful when the rights-gated authoring probe is absent.
    # This is an original, deliberately sparse panel scaffold: its labels and
    # section marks describe Beadsy's own five bound controls, while the
    # guide rings make the existing placement map inspectable. It does not
    # reproduce Beads artwork or imply a fidelity comparison.
    set(BEADSY_PANEL_SVG [=[
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200.693 364.252">
  <rect width="200.693" height="364.252" fill="#e8e5df"/>
  <rect x="3" y="3" width="194.693" height="358.252" rx="4" fill="#f7f5ef" stroke="#59636b" stroke-width="1"/>
  <path d="M14 28H186 M14 33H186" stroke="#59636b" stroke-width="0.5" opacity="0.72"/>
  <text x="100.3465" y="17" text-anchor="middle" font-family="sans-serif" font-size="8" font-weight="700" letter-spacing="2" fill="#202326">BEADSY</text>
  <text x="100.3465" y="25" text-anchor="middle" font-family="sans-serif" font-size="3.1" letter-spacing="1" fill="#334b55">MIDI GRAIN SYNTH</text>

  <text x="100.3465" y="46" text-anchor="middle" font-family="sans-serif" font-size="3.2" font-weight="700" letter-spacing="1.2" fill="#334b55">GRAIN</text>
  <path d="M24 49H176" stroke="#59636b" stroke-width="0.5" opacity="0.45"/>

  <!-- Functional labels for the five source-backed Beadsy parameters. -->
  <g font-family="sans-serif" font-size="3.2" font-weight="700" letter-spacing="0.65" fill="#202326">
    <text x="101.4" y="97" text-anchor="middle">DENSITY</text>
    <text x="33.8" y="139" text-anchor="middle">TIME</text>
    <text x="168.9" y="139" text-anchor="middle">PITCH</text>
    <text x="69.8" y="198" text-anchor="middle">SIZE</text>
    <text x="133" y="198" text-anchor="middle">SHAPE</text>
  </g>

  <!-- Placement guides are visual scaffolding, not captured panel artwork. -->
  <g fill="none" stroke="#59636b" stroke-width="0.45" opacity="0.34" stroke-dasharray="1.2 1.2">
    <circle cx="101.4" cy="68.2" r="26.2"/>
    <circle cx="33.8" cy="108.6" r="26.2"/>
    <circle cx="168.9" cy="108.6" r="26.2"/>
    <circle cx="69.8" cy="171.5" r="20.4"/>
    <circle cx="133" cy="171.5" r="20.4"/>
  </g>

  <path d="M24 205H176" stroke="#59636b" stroke-width="0.5" opacity="0.55"/>
  <text x="100.3465" y="217" text-anchor="middle" font-family="sans-serif" font-size="3.2" font-weight="700" letter-spacing="1.2" fill="#334b55">MIX / MOTION</text>
  <g fill="none" stroke="#59636b" stroke-width="0.5" opacity="0.42">
    <path d="M24 224H176 M24 229H176 M24 234H176"/>
    <path d="M24 224V234 M176 224V234"/>
  </g>
  <text x="100.3465" y="245" text-anchor="middle" font-family="sans-serif" font-size="3" letter-spacing="0.7" fill="#657078">PARAMETER CONTROLS</text>

  <path d="M24 331H176" stroke="#59636b" stroke-width="0.5" opacity="0.6"/>
  <text x="100.3465" y="344" text-anchor="middle" font-family="sans-serif" font-size="3" letter-spacing="0.85" fill="#334b55">48 KHZ  •  MONO VOICE  •  MIDI</text>
  <text x="100.3465" y="353" text-anchor="middle" font-family="sans-serif" font-size="2.5" letter-spacing="0.5" fill="#657078">INDEPENDENT PRODUCT SCAFFOLD</text>
</svg>]=])
    set(BEADSY_PANEL_SOURCE_STATUS "independent scaffold; panel artwork opt-in disabled or unavailable")
    message(STATUS "Beadsy panel probe unavailable; using independent panel scaffold")
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/beadsy_panel_svg.hpp.in
               ${CMAKE_CURRENT_BINARY_DIR}/beadsy_panel_svg.hpp @ONLY)

add_library(beadsy_beads_dsp STATIC
    third_party/beads_dsp/beads/dsp/processor.cc
    third_party/beads_dsp/beads/dsp/delay_controller.cc
    third_party/beads_dsp/beads/dsp/delay_clock.cc
    third_party/beads_dsp/beads/dsp/mu_law.cc
    third_party/beads_dsp/beads/dsp/synth.cc
    third_party/beads_dsp/beads/dsp/trigger_generator.cc
    third_party/beads_dsp/beads/resources.cc
    third_party/beads_dsp/stmlib/dsp/units.cc
    third_party/beads_dsp/stmlib/utils/random.cc
)
target_include_directories(beadsy_beads_dsp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/beads_dsp
)
target_compile_features(beadsy_beads_dsp PUBLIC cxx_std_20)

# Tests
include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)
include(CTest)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(Beadsy-test test_beadsy.cpp)
target_link_libraries(Beadsy-test PRIVATE Pulp::format Catch2::Catch2WithMain beadsy_beads_dsp)
target_include_directories(Beadsy-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(Beadsy-test PRIVATE BEADSY_TESTING=1)

# Pulp keeps the allocation probe in its test harness (it is not part of the
# installed SDK).  Point this test-only target at a matching Pulp checkout
# when the realtime allocation gate is desired; ordinary SDK consumers can
# continue building the functional tests without the private harness.
set(BEADSY_PULP_TEST_HARNESS_DIR "" CACHE PATH
    "Pulp test/harness directory for Beadsy realtime allocation tests")
if(NOT BEADSY_PULP_TEST_HARNESS_DIR AND DEFINED ENV{PULP_SOURCE_DIR})
    set(BEADSY_PULP_TEST_HARNESS_DIR
        "$ENV{PULP_SOURCE_DIR}/test/harness")
endif()
if(EXISTS "${BEADSY_PULP_TEST_HARNESS_DIR}/rt_allocation_probe.cpp")
    target_sources(Beadsy-test PRIVATE
        "${BEADSY_PULP_TEST_HARNESS_DIR}/rt_allocation_probe.cpp")
    target_include_directories(Beadsy-test PRIVATE
        "${BEADSY_PULP_TEST_HARNESS_DIR}")
    target_compile_definitions(Beadsy-test PRIVATE
        BEADSY_HAS_RT_ALLOCATION_PROBE=1)
else()
    message(STATUS
        "Beadsy realtime allocation probe unavailable; set "
        "BEADSY_PULP_TEST_HARNESS_DIR to Pulp's test/harness")
endif()
catch_discover_tests(Beadsy-test)

add_executable(BeadsyFX-test test_beadsy_fx.cpp)
target_link_libraries(BeadsyFX-test PRIVATE
    Pulp::format Catch2::Catch2WithMain beadsy_beads_dsp)
target_include_directories(BeadsyFX-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
catch_discover_tests(BeadsyFX-test)

# Small, authorized offline harness for standalone-adjacent audio/state proof.
# It uses Pulp's HeadlessHost seam rather than claiming that the GUI app itself
# exposes a state or audio measurement bridge.
add_executable(Beadsy-headless-harness beadsy_headless_harness.cpp)
target_link_libraries(Beadsy-headless-harness PRIVATE
    Pulp::format beadsy_beads_dsp)
target_include_directories(Beadsy-headless-harness PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

if(TARGET Pulp::view)
    add_executable(Beadsy-editor-test test_beadsy_editor.cpp)
    target_link_libraries(Beadsy-editor-test PRIVATE
        Pulp::format Pulp::view Catch2::Catch2WithMain beadsy_beads_dsp)
    if(TARGET Pulp::host)
        target_link_libraries(Beadsy-editor-test PRIVATE Pulp::host)
    endif()
    target_include_directories(Beadsy-editor-test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
    target_compile_definitions(Beadsy-editor-test PRIVATE
        BEADSY_NATIVE_EDITOR=1 PULP_CONTEXT_MENU_INLINE_CONTROLS=1)
    catch_discover_tests(Beadsy-editor-test)
endif()

pulp_add_plugin(Beadsy
    FORMATS         VST3 CLAP LV2 AU AUv3 AAX Standalone
    PLUGIN_NAME     "Beadsy"
    BUNDLE_ID       "com.independent-audio.beadsy"
    MANUFACTURER    "Independent Audio"
    VERSION         "1.0.2"
    CATEGORY        Instrument
    ACCEPTS_MIDI
    PLUGIN_CODE     "Bead"
    MANUFACTURER_CODE "Inde"
    AAX_PRODUCT_CODE "GraP"
    AAX_NATIVE_CODE "Bead"
)

foreach(_fmt IN ITEMS VST3 CLAP LV2 AU AUv3 AAX Standalone)
    if(TARGET Beadsy_${_fmt})
        target_link_libraries(Beadsy_${_fmt} PRIVATE beadsy_beads_dsp)
        if(TARGET Pulp::view)
            target_link_libraries(Beadsy_${_fmt} PRIVATE Pulp::view)
            if(TARGET Pulp::host)
                target_link_libraries(Beadsy_${_fmt} PRIVATE Pulp::host)
            endif()
            target_include_directories(Beadsy_${_fmt} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
            target_compile_definitions(Beadsy_${_fmt} PRIVATE
                BEADSY_NATIVE_EDITOR=1 PULP_CONTEXT_MENU_INLINE_CONTROLS=1)
        endif()
        if(_fmt STREQUAL "LV2")
            # LV2's Pulp adapter exposes one aggregated input bus. Keep the
            # source selector honest until a second LV2 input port is wired.
            target_compile_definitions(Beadsy_${_fmt} PRIVATE BEADSY_LV2=1)
        endif()
        if(_fmt STREQUAL "AU")
            # Keep the instrument's optional Main In + Sidechain buses visible
            # to AUv2 hosts. Logic owns the project/track catalog and exposes
            # its source picker when this descriptor is negotiated; the
            # processor still defaults to MIDI Synth until a source is chosen.
        endif()
    endif()
endforeach()

# Audio-input companion.  The primary Beadsy artifact is an AU instrument
# (aumu) for MIDI workflows; this companion is an AU MusicEffect (aumf) so a
# DAW can feed the track/bus audio into the same Beads processor.  Host project
# source menus remain host-owned; this target simply exposes a real input bus.
pulp_add_plugin(BeadsyFX
    FORMATS         VST3 CLAP AU AUv3 Standalone
    PLUGIN_NAME     "Beadsy FX"
    BUNDLE_ID       "com.independent-audio.beadsy-fx"
    MANUFACTURER    "Independent Audio"
    VERSION         "1.0.2"
    CATEGORY        Effect
    ACCEPTS_MIDI
    PLUGIN_CODE     "BdFX"
    MANUFACTURER_CODE "Inde"
)

foreach(_fmt IN ITEMS VST3 CLAP AU AUv3 Standalone)
    if(TARGET BeadsyFX_${_fmt})
        target_compile_definitions(BeadsyFX_${_fmt} PRIVATE BEADSY_EFFECT_VARIANT=1)
        target_link_libraries(BeadsyFX_${_fmt} PRIVATE beadsy_beads_dsp)
        if(TARGET Pulp::view)
            target_link_libraries(BeadsyFX_${_fmt} PRIVATE Pulp::view)
            if(TARGET Pulp::host)
                target_link_libraries(BeadsyFX_${_fmt} PRIVATE Pulp::host)
            endif()
            target_include_directories(BeadsyFX_${_fmt} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
            target_compile_definitions(BeadsyFX_${_fmt} PRIVATE
                BEADSY_NATIVE_EDITOR=1 PULP_CONTEXT_MENU_INLINE_CONTROLS=1)
        endif()
        if(_fmt STREQUAL "AU")
            target_compile_definitions(BeadsyFX_${_fmt} PRIVATE BEADSY_AU_MAIN_INPUT_ONLY=1)
        endif()
    endif()
endforeach()

# The installed Pulp SDK builds the LV2 shared object but does not emit the
# discovery TTL files. Keep this product-local manifest in the bundle so LV2
# hosts can discover Beadsy's audio, control, and MIDI ports.
if(TARGET Beadsy_LV2)
    add_custom_command(TARGET Beadsy_LV2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/lv2"
            "${CMAKE_BINARY_DIR}/LV2/Beadsy.lv2"
        VERBATIM)
endif()

# Standalone state command/control-thread integration against Pulp's state bridge.
if(TARGET Pulp::standalone)
    add_executable(Beadsy-standalone-state-test test_beadsy_standalone_state.cpp)
    target_link_libraries(Beadsy-standalone-state-test PRIVATE
        Pulp::standalone Catch2::Catch2WithMain beadsy_beads_dsp)
    target_include_directories(Beadsy-standalone-state-test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})
    catch_discover_tests(Beadsy-standalone-state-test)
endif()
