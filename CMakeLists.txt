cmake_minimum_required(VERSION 3.24)
project(Beasy VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Pulp CONFIG REQUIRED)

# The converted panel is an authoring-time, rights-gated vector asset. Embed it
# into the editor so installed plugins never read a developer checkout. A clean
# checkout may not contain the probe asset while its rights review is pending;
# use a deterministic neutral panel in that case so DSP and headless builds do
# not depend on an ignored developer artifact.
set(BEASY_PANEL_ASSET "${CMAKE_CURRENT_SOURCE_DIR}/provenance/generated/beads-panel.svg")
if(EXISTS "${BEASY_PANEL_ASSET}")
    file(READ "${BEASY_PANEL_ASSET}" BEASY_PANEL_SVG)
    set(BEASY_PANEL_SOURCE_STATUS "rights-gated authoring probe")
else()
    # Keep the product useful when the rights-gated authoring probe is absent.
    # This is an original, deliberately sparse panel scaffold: its labels and
    # section marks describe Beasy's own five bound controls, while the
    # guide rings make the existing placement map inspectable. It does not
    # reproduce Beads artwork or imply a fidelity comparison.
    set(BEASY_PANEL_SVG [=[
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200.693 364.252">
  <rect width="200.693" height="364.252" fill="#e8e5df"/>
  <rect x="3" y="3" width="194.693" height="358.252" rx="4" fill="#f7f5ef" stroke="#59636b" stroke-width="1"/>
  <path d="M14 28H186 M14 33H186" stroke="#59636b" stroke-width="0.5" opacity="0.72"/>
  <text x="100.3465" y="17" text-anchor="middle" font-family="sans-serif" font-size="8" font-weight="700" letter-spacing="2" fill="#202326">BEASY</text>
  <text x="100.3465" y="25" text-anchor="middle" font-family="sans-serif" font-size="3.1" letter-spacing="1" fill="#334b55">MIDI GRAIN SYNTH</text>

  <text x="100.3465" y="46" text-anchor="middle" font-family="sans-serif" font-size="3.2" font-weight="700" letter-spacing="1.2" fill="#334b55">GRAIN</text>
  <path d="M24 49H176" stroke="#59636b" stroke-width="0.5" opacity="0.45"/>

  <!-- Functional labels for the five source-backed Beasy parameters. -->
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
    set(BEASY_PANEL_SOURCE_STATUS "independent scaffold; rights-gated artwork probe unavailable")
    message(STATUS "Beasy panel probe unavailable; using independent panel scaffold")
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/beasy_panel_svg.hpp.in
               ${CMAKE_CURRENT_BINARY_DIR}/beasy_panel_svg.hpp @ONLY)

add_library(beasy_beads_dsp STATIC
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
target_include_directories(beasy_beads_dsp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/beads_dsp
)
target_compile_features(beasy_beads_dsp PUBLIC cxx_std_20)

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

add_executable(Beasy-test test_beasy.cpp)
target_link_libraries(Beasy-test PRIVATE Pulp::format Catch2::Catch2WithMain beasy_beads_dsp)
target_include_directories(Beasy-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(Beasy-test PRIVATE BEASY_TESTING=1)

# Pulp keeps the allocation probe in its test harness (it is not part of the
# installed SDK).  Point this test-only target at a matching Pulp checkout
# when the realtime allocation gate is desired; ordinary SDK consumers can
# continue building the functional tests without the private harness.
set(BEASY_PULP_TEST_HARNESS_DIR "" CACHE PATH
    "Pulp test/harness directory for Beasy realtime allocation tests")
if(NOT BEASY_PULP_TEST_HARNESS_DIR AND DEFINED ENV{PULP_SOURCE_DIR})
    set(BEASY_PULP_TEST_HARNESS_DIR
        "$ENV{PULP_SOURCE_DIR}/test/harness")
endif()
if(EXISTS "${BEASY_PULP_TEST_HARNESS_DIR}/rt_allocation_probe.cpp")
    target_sources(Beasy-test PRIVATE
        "${BEASY_PULP_TEST_HARNESS_DIR}/rt_allocation_probe.cpp")
    target_include_directories(Beasy-test PRIVATE
        "${BEASY_PULP_TEST_HARNESS_DIR}")
    target_compile_definitions(Beasy-test PRIVATE
        BEASY_HAS_RT_ALLOCATION_PROBE=1)
else()
    message(STATUS
        "Beasy realtime allocation probe unavailable; set "
        "BEASY_PULP_TEST_HARNESS_DIR to Pulp's test/harness")
endif()
catch_discover_tests(Beasy-test)

# Small, authorized offline harness for standalone-adjacent audio/state proof.
# It uses Pulp's HeadlessHost seam rather than claiming that the GUI app itself
# exposes a state or audio measurement bridge.
add_executable(Beasy-headless-harness beasy_headless_harness.cpp)
target_link_libraries(Beasy-headless-harness PRIVATE
    Pulp::format beasy_beads_dsp)
target_include_directories(Beasy-headless-harness PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

if(TARGET Pulp::view)
    add_executable(Beasy-editor-test test_beasy_editor.cpp)
    target_link_libraries(Beasy-editor-test PRIVATE
        Pulp::format Pulp::view Catch2::Catch2WithMain beasy_beads_dsp)
    target_include_directories(Beasy-editor-test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
    target_compile_definitions(Beasy-editor-test PRIVATE BEASY_NATIVE_EDITOR=1)
    catch_discover_tests(Beasy-editor-test)
endif()

pulp_add_plugin(Beasy
    FORMATS         VST3 CLAP LV2 AU AAX Standalone
    PLUGIN_NAME     "Beasy"
    BUNDLE_ID       "com.independent_audio.beasy"
    MANUFACTURER    "Independent Audio"
    VERSION         "1.0.0"
    CATEGORY        Instrument
    PLUGIN_CODE     "Bead"
    MANUFACTURER_CODE "Inde"
    AAX_PRODUCT_CODE "GraP"
    AAX_NATIVE_CODE "Bead"
)

foreach(_fmt IN ITEMS VST3 CLAP LV2 AU AAX Standalone)
    if(TARGET Beasy_${_fmt})
        target_link_libraries(Beasy_${_fmt} PRIVATE beasy_beads_dsp)
        if(TARGET Pulp::view)
            target_link_libraries(Beasy_${_fmt} PRIVATE Pulp::view)
            target_include_directories(Beasy_${_fmt} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
            target_compile_definitions(Beasy_${_fmt} PRIVATE BEASY_NATIVE_EDITOR=1)
        endif()
    endif()
endforeach()

# The installed Pulp SDK builds the LV2 shared object but does not emit the
# discovery TTL files. Keep this product-local manifest in the bundle so LV2
# hosts can discover Beasy's audio, control, and MIDI ports.
if(TARGET Beasy_LV2)
    add_custom_command(TARGET Beasy_LV2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/lv2"
            "${CMAKE_BINARY_DIR}/LV2/Beasy.lv2"
        VERBATIM)
endif()

# Standalone state command/control-thread integration against Pulp's state bridge.
if(TARGET Pulp::standalone)
    add_executable(Beasy-standalone-state-test test_beasy_standalone_state.cpp)
    target_link_libraries(Beasy-standalone-state-test PRIVATE
        Pulp::standalone Catch2::Catch2WithMain beasy_beads_dsp)
    target_include_directories(Beasy-standalone-state-test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})
    catch_discover_tests(Beasy-standalone-state-test)
endif()
