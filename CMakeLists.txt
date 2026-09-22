cmake_minimum_required(VERSION 3.21)
project(CyGameCapture VERSION 0.1.0 LANGUAGES CXX RC)

# ---------------------------------------------------------------------------
# CyGameCapture — ReShade GPU buffer inspector & OBS capture system
#   CyGameCaptureCore : shared, dependency-free code (naming, versions, ...)
#   Resources/Branding: logo and application icon, embedded in every binary
#   CyGameCaptureRS   : native ReShade add-on (.addon64)
#   CyGameCaptureAI   : helper the add-on starts to ask an AI client where the picture without UI is
#   CyGameCaptureOBS  : OBS Studio plugin (receives the Spout senders of RS and UE)
#   Tests             : D3D11 test application + Spout receiver test tool
# ---------------------------------------------------------------------------

if(NOT WIN32)
    message(FATAL_ERROR "CyGameCapture targets Windows only (ReShade / DirectX / Spout2).")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "CyGameCapture must be built as a 64-bit binary (ReShade .addon64).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Static CRT: the add-on is loaded inside arbitrary game processes, avoid any vcredist dependency.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(CYGC_BUILD_TESTS "Build the test application and the Spout receiver test" ON)

set(CYGC_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>")

# WIN32_LEAN_AND_MEAN is applied per source set (Spout's SpoutFrameCount.cpp needs mmsystem.h)
add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8 /MP /EHsc)

option(CYGC_BUILD_OBS "Build the OBS Studio plugin (needs ThirdParty/obs-studio/lib/obs.lib)" ON)

add_subdirectory(Resources/Branding)
add_subdirectory(CyGameCaptureCore)
add_subdirectory(CyGameCaptureRS)
add_subdirectory(CyGameCaptureAI)
if(CYGC_BUILD_OBS)
    add_subdirectory(CyGameCaptureOBS)
endif()
if(CYGC_BUILD_TESTS)
    add_subdirectory(Tests)
endif()
