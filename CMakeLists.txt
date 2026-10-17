cmake_minimum_required(VERSION 3.24)
project(Grainfield VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Pulp 0.0.0-pulp-rs REQUIRED)

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
target_link_libraries(Grainfield-test PRIVATE Pulp::format Catch2::Catch2WithMain)
target_include_directories(Grainfield-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
catch_discover_tests(Grainfield-test)

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
