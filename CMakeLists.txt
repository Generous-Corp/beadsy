cmake_minimum_required(VERSION 3.24)
project(Grainfield VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Pulp CONFIG REQUIRED)

# The converted panel is an authoring-time, rights-gated vector asset. Embed it
# into the editor so installed plugins never read a developer checkout. A clean
# checkout may not contain the probe asset while its rights review is pending;
# use a deterministic neutral panel in that case so DSP and headless builds do
# not depend on an ignored developer artifact.
set(GRAINFIELD_PANEL_ASSET "${CMAKE_CURRENT_SOURCE_DIR}/provenance/generated/beads-panel.svg")
if(EXISTS "${GRAINFIELD_PANEL_ASSET}")
    file(READ "${GRAINFIELD_PANEL_ASSET}" GRAINFIELD_PANEL_SVG)
    set(GRAINFIELD_PANEL_SOURCE_STATUS "rights-gated authoring probe")
else()
    set(GRAINFIELD_PANEL_SVG
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 200.693 364.252\"><rect width=\"200.693\" height=\"364.252\" fill=\"#161513\"/></svg>")
    set(GRAINFIELD_PANEL_SOURCE_STATUS "deterministic neutral fallback; artwork probe unavailable")
    message(STATUS "Grainfield panel probe unavailable; using deterministic neutral fallback")
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/grainfield_panel_svg.hpp.in
               ${CMAKE_CURRENT_BINARY_DIR}/grainfield_panel_svg.hpp @ONLY)

add_library(grainfield_beads_dsp STATIC
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
target_include_directories(grainfield_beads_dsp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/beads_dsp
)
target_compile_features(grainfield_beads_dsp PUBLIC cxx_std_20)

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

add_executable(Grainfield-test test_grainfield.cpp)
target_link_libraries(Grainfield-test PRIVATE Pulp::format Catch2::Catch2WithMain grainfield_beads_dsp)
target_include_directories(Grainfield-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(Grainfield-test PRIVATE GRAINFIELD_TESTING=1)

# Pulp keeps the allocation probe in its test harness (it is not part of the
# installed SDK).  Point this test-only target at a matching Pulp checkout
# when the realtime allocation gate is desired; ordinary SDK consumers can
# continue building the functional tests without the private harness.
set(GRAINFIELD_PULP_TEST_HARNESS_DIR "" CACHE PATH
    "Pulp test/harness directory for Grainfield realtime allocation tests")
if(NOT GRAINFIELD_PULP_TEST_HARNESS_DIR AND DEFINED ENV{PULP_SOURCE_DIR})
    set(GRAINFIELD_PULP_TEST_HARNESS_DIR
        "$ENV{PULP_SOURCE_DIR}/test/harness")
endif()
if(EXISTS "${GRAINFIELD_PULP_TEST_HARNESS_DIR}/rt_allocation_probe.cpp")
    target_sources(Grainfield-test PRIVATE
        "${GRAINFIELD_PULP_TEST_HARNESS_DIR}/rt_allocation_probe.cpp")
    target_include_directories(Grainfield-test PRIVATE
        "${GRAINFIELD_PULP_TEST_HARNESS_DIR}")
    target_compile_definitions(Grainfield-test PRIVATE
        GRAINFIELD_HAS_RT_ALLOCATION_PROBE=1)
else()
    message(STATUS
        "Grainfield realtime allocation probe unavailable; set "
        "GRAINFIELD_PULP_TEST_HARNESS_DIR to Pulp's test/harness")
endif()
catch_discover_tests(Grainfield-test)

if(TARGET Pulp::view)
    add_executable(Grainfield-editor-test test_grainfield_editor.cpp)
    target_link_libraries(Grainfield-editor-test PRIVATE
        Pulp::format Pulp::view Catch2::Catch2WithMain grainfield_beads_dsp)
    target_include_directories(Grainfield-editor-test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
    target_compile_definitions(Grainfield-editor-test PRIVATE GRAINFIELD_NATIVE_EDITOR=1)
    catch_discover_tests(Grainfield-editor-test)
endif()

pulp_add_plugin(Grainfield
    FORMATS         VST3 CLAP LV2 AU AAX Standalone
    PLUGIN_NAME     "Grainfield"
    BUNDLE_ID       "com.independent_audio.grainfield"
    MANUFACTURER    "Independent Audio"
    VERSION         "1.0.0"
    CATEGORY        Instrument
    PLUGIN_CODE     "Grai"
    MANUFACTURER_CODE "Inde"
    AAX_PRODUCT_CODE "GraP"
    AAX_NATIVE_CODE "Grai"
)

foreach(_fmt IN ITEMS VST3 CLAP LV2 AU AAX Standalone)
    if(TARGET Grainfield_${_fmt})
        target_link_libraries(Grainfield_${_fmt} PRIVATE grainfield_beads_dsp)
        if(TARGET Pulp::view)
            target_link_libraries(Grainfield_${_fmt} PRIVATE Pulp::view)
            target_include_directories(Grainfield_${_fmt} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
            target_compile_definitions(Grainfield_${_fmt} PRIVATE GRAINFIELD_NATIVE_EDITOR=1)
        endif()
    endif()
endforeach()

# The installed Pulp SDK builds the LV2 shared object but does not emit the
# discovery TTL files. Keep this product-local manifest in the bundle so LV2
# hosts can discover Grainfield's audio, control, and MIDI ports.
if(TARGET Grainfield_LV2)
    add_custom_command(TARGET Grainfield_LV2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/lv2"
            "${CMAKE_BINARY_DIR}/LV2/Grainfield.lv2"
        VERBATIM)
endif()
