# CyGameCaptureOBS — OBS Studio plugin (obs-plugins/64bit/CyGameCaptureOBS.dll)
#
# libobs is used through its public C API only. The headers are the ones of the OBS release this is
# built against (ThirdParty/obs-studio/libobs, see THIRD_PARTY_NOTICES.md), and the import library is
# generated from the installed obs.dll by Tools/GenerateObsImportLib.cmd so no OBS build tree is needed.

set(CYGC_OBS_THIRDPARTY ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty)
set(CYGC_OBS_LIB ${CYGC_OBS_THIRDPARTY}/obs-studio/lib/obs.lib)

if(NOT EXISTS ${CYGC_OBS_LIB})
    message(WARNING
        "CyGameCaptureOBS: ${CYGC_OBS_LIB} is missing. "
        "Run Tools/GenerateObsImportLib.cmd once (it reads the obs.dll of the installed OBS Studio). "
        "Skipping the OBS plugin.")
    return()
endif()

# Spout2 subset a receiver needs: sender name table, shared memory, access mutex / frame counter.
# No OpenGL, no SpoutDX: the GPU side is handled by OBS through gs_texture_open_shared().
set(CYGC_OBS_SPOUT_SOURCES
    ${CMAKE_SOURCE_DIR}/CyGameCaptureRS/ThirdParty/Spout2/SpoutGL/SpoutUtils.cpp
    ${CMAKE_SOURCE_DIR}/CyGameCaptureRS/ThirdParty/Spout2/SpoutGL/SpoutSharedMemory.cpp
    ${CMAKE_SOURCE_DIR}/CyGameCaptureRS/ThirdParty/Spout2/SpoutGL/SpoutSenderNames.cpp
    ${CMAKE_SOURCE_DIR}/CyGameCaptureRS/ThirdParty/Spout2/SpoutGL/SpoutFrameCount.cpp)
set_source_files_properties(${CYGC_OBS_SPOUT_SOURCES} PROPERTIES COMPILE_OPTIONS "/W1")

set(CYGC_OBS_SOURCES
    Source/PluginMain.cpp
    Source/CaptureSource.cpp
    Source/SpoutDiscovery.hpp
    Source/SpoutDiscovery.cpp
    Source/BufferRecorder.hpp
    Source/BufferRecorder.cpp
    Source/CyGameCaptureOBS.rc)

add_library(CyGameCaptureOBS SHARED ${CYGC_OBS_SOURCES} ${CYGC_OBS_SPOUT_SOURCES})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/Source PREFIX "Source" FILES ${CYGC_OBS_SOURCES})
source_group("ThirdParty\\Spout2" FILES ${CYGC_OBS_SPOUT_SOURCES})

target_include_directories(CyGameCaptureOBS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CYGC_OBS_THIRDPARTY}/obs-studio/libobs
    ${CMAKE_SOURCE_DIR}/CyGameCaptureRS/ThirdParty/Spout2/SpoutGL)

# C4201 (nameless struct/union) comes from libobs' own headers, which are C and use them everywhere
target_compile_options(CyGameCaptureOBS PRIVATE /wd4201)

target_compile_definitions(CyGameCaptureOBS PRIVATE
    SPOUT_BUILD_STATIC             # Spout compiled straight into the plugin
    CYGC_ADDON_NAME="CyGameCaptureOBS")

set_source_files_properties(${CYGC_OBS_SOURCES} PROPERTIES COMPILE_DEFINITIONS "WIN32_LEAN_AND_MEAN")

target_link_libraries(CyGameCaptureOBS PRIVATE CyGameCaptureCore CyGameCaptureBranding ${CYGC_OBS_LIB} advapi32 shell32 winmm)

set_target_properties(CyGameCaptureOBS PROPERTIES
    OUTPUT_NAME "CyGameCaptureOBS"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY ${CYGC_OUTPUT_DIR}/obs-plugins/64bit
    LIBRARY_OUTPUT_DIRECTORY ${CYGC_OUTPUT_DIR}/obs-plugins/64bit)

# OBS looks for the locale files next to the plugin, in data/obs-plugins/<module name>/
add_custom_command(TARGET CyGameCaptureOBS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/data
        ${CYGC_OUTPUT_DIR}/data/obs-plugins/CyGameCaptureOBS
    COMMENT "Copying CyGameCaptureOBS locale data")
