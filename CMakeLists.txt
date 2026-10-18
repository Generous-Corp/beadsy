cmake_minimum_required(VERSION 3.24)
project(Grainfield VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Pulp CONFIG REQUIRED)

# The converted panel is an authoring-time, rights-gated vector asset. Embed it
# into the editor so installed plugins never read a developer checkout.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/provenance/generated/beads-panel.svg" GRAINFIELD_PANEL_SVG)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/grainfield_panel_svg.hpp.in
               ${CMAKE_CURRENT_BINARY_DIR}/grainfield_panel_svg.hpp @ONLY)

add_library(grainfield_beads_dsp STATIC
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
