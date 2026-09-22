# CyGameCaptureCore: code shared between CyGameCaptureRS and (future) CyGameCaptureOBS.
# Kept deliberately small: only add things here once they are really used by both sides.
add_library(CyGameCaptureCore INTERFACE)
target_include_directories(CyGameCaptureCore INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Include)
target_sources(CyGameCaptureCore INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Include/CyGameCaptureCore/Version.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Include/CyGameCaptureCore/SenderNaming.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Include/CyGameCaptureCore/StreamInfo.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Include/CyGameCaptureCore/FrameSync.hpp)
