# Test tools for CyGameCapture.
#
#  CyGameCaptureTestApp         : tiny D3D11 "game" rendering a clean scene into an offscreen
#                                 render target, then compositing scene + fake HUD into the back buffer.
#                                 Drop ReShade (dxgi.dll, add-on build) + CyGameCaptureRS.addon64 next to it.
#  CyGameCaptureSpoutReceiverTest: headless Spout receiver (SpoutDX) that dumps frames of a sender
#                                 to .bmp files, so the whole pipeline can be validated without OBS.
#  CyGameCaptureVideoProbe      : decodes a recording and reports what really survived in it (pixel
#                                 format, bit depth, distinct levels). Optional: it needs the FFmpeg
#                                 import libraries produced by Tools\GenerateFFmpegImportLibs.cmd.

set(CYGC_RS_THIRDPARTY ${CMAKE_SOURCE_DIR}/CyGameCaptureRS/ThirdParty)

# ---------------------------------------------------------------- test app
set_source_files_properties(CyGameCaptureTestApp/main.cpp CyGameCaptureTestAppD3D12/main.cpp CyGameCaptureSpoutReceiverTest/main.cpp PROPERTIES COMPILE_DEFINITIONS "WIN32_LEAN_AND_MEAN")

# Every test tool carries the CyGameCapture icon, so they are recognisable in Explorer and the taskbar
set(CYGC_TOOLS_RC ${CMAKE_CURRENT_SOURCE_DIR}/CyGameCaptureTools.rc)

add_executable(CyGameCaptureTestApp WIN32 CyGameCaptureTestApp/main.cpp ${CYGC_TOOLS_RC})
target_link_libraries(CyGameCaptureTestApp PRIVATE CyGameCaptureBranding d3d11 dxgi dxguid d3dcompiler user32 gdi32)
set_target_properties(CyGameCaptureTestApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CYGC_OUTPUT_DIR}/TestApp"
    PDB_OUTPUT_DIRECTORY     "${CYGC_OUTPUT_DIR}/TestApp")

# The add-on is copied next to the test app on every build so the folder is self-contained
# (ReShade searches *.addon64 in the application directory by default). A dedicated target is
# used instead of a POST_BUILD step on the test app, so the copy also happens when only the add-on changed.
add_custom_target(CyGameCaptureTestAppDeploy ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:CyGameCaptureRS> "${CYGC_OUTPUT_DIR}/TestApp/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_PDB_FILE:CyGameCaptureRS> "${CYGC_OUTPUT_DIR}/TestApp/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/CyGameCaptureTestApp/ReShade.ini "${CYGC_OUTPUT_DIR}/TestApp/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:CyGameCaptureAI> "${CYGC_OUTPUT_DIR}/TestApp/"
    DEPENDS CyGameCaptureRS CyGameCaptureAI CyGameCaptureTestApp
    COMMENT "Deploying CyGameCaptureRS.addon64 and ReShade.ini next to the test application")

# ---------------------------------------------------------------- test app (D3D12)
add_executable(CyGameCaptureTestAppD3D12 WIN32 CyGameCaptureTestAppD3D12/main.cpp ${CYGC_TOOLS_RC})
target_link_libraries(CyGameCaptureTestAppD3D12 PRIVATE CyGameCaptureBranding d3d12 dxgi dxguid d3dcompiler user32 gdi32)
set_target_properties(CyGameCaptureTestAppD3D12 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CYGC_OUTPUT_DIR}/TestAppD3D12"
    PDB_OUTPUT_DIRECTORY     "${CYGC_OUTPUT_DIR}/TestAppD3D12")

add_custom_target(CyGameCaptureTestAppD3D12Deploy ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:CyGameCaptureRS> "${CYGC_OUTPUT_DIR}/TestAppD3D12/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_PDB_FILE:CyGameCaptureRS> "${CYGC_OUTPUT_DIR}/TestAppD3D12/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/CyGameCaptureTestAppD3D12/ReShade.ini "${CYGC_OUTPUT_DIR}/TestAppD3D12/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:CyGameCaptureAI> "${CYGC_OUTPUT_DIR}/TestAppD3D12/"
    DEPENDS CyGameCaptureRS CyGameCaptureAI CyGameCaptureTestAppD3D12
    COMMENT "Deploying CyGameCaptureRS.addon64 and ReShade.ini next to the D3D12 test application")

# ---------------------------------------------------------------- receiver test
set(CYGC_SPOUTDX_SOURCES
    ${CYGC_RS_THIRDPARTY}/Spout2/SpoutGL/SpoutUtils.cpp
    ${CYGC_RS_THIRDPARTY}/Spout2/SpoutGL/SpoutSharedMemory.cpp
    ${CYGC_RS_THIRDPARTY}/Spout2/SpoutGL/SpoutSenderNames.cpp
    ${CYGC_RS_THIRDPARTY}/Spout2/SpoutGL/SpoutFrameCount.cpp
    ${CYGC_RS_THIRDPARTY}/Spout2/SpoutGL/SpoutCopy.cpp
    ${CYGC_RS_THIRDPARTY}/Spout2/SpoutGL/SpoutDirectX.cpp
    ${CYGC_RS_THIRDPARTY}/Spout2/SpoutDX/SpoutDX.cpp)
set_source_files_properties(${CYGC_SPOUTDX_SOURCES} PROPERTIES COMPILE_OPTIONS "/W1;/wd4828")

add_executable(CyGameCaptureSpoutReceiverTest CyGameCaptureSpoutReceiverTest/main.cpp ${CYGC_SPOUTDX_SOURCES} ${CYGC_TOOLS_RC})
target_include_directories(CyGameCaptureSpoutReceiverTest PRIVATE
    ${CYGC_RS_THIRDPARTY}/Spout2/SpoutGL
    ${CYGC_RS_THIRDPARTY}/Spout2/SpoutDX)
target_compile_definitions(CyGameCaptureSpoutReceiverTest PRIVATE SPOUT_BUILD_STATIC)
target_link_libraries(CyGameCaptureSpoutReceiverTest PRIVATE CyGameCaptureCore CyGameCaptureBranding
    d3d11 dxgi winmm shell32 advapi32 version comctl32 psapi user32 gdi32 ole32)
set_target_properties(CyGameCaptureSpoutReceiverTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CYGC_OUTPUT_DIR}"
    PDB_OUTPUT_DIRECTORY     "${CYGC_OUTPUT_DIR}")

# ---------------------------------------------------------------- recording probe (optional)
# It decodes with the FFmpeg libraries OBS already ships, so there is nothing extra to install: only the
# public headers are vendored, and the import libraries come from the installed OBS. Without them the
# probe is skipped and everything else still builds.
set(CYGC_FFMPEG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/CyGameCaptureVideoProbe/ThirdParty/ffmpeg)

if(EXISTS ${CYGC_FFMPEG_DIR}/lib/avcodec.lib AND EXISTS ${CYGC_FFMPEG_DIR}/lib/avformat.lib AND EXISTS ${CYGC_FFMPEG_DIR}/lib/avutil.lib)
    add_executable(CyGameCaptureVideoProbe CyGameCaptureVideoProbe/main.cpp ${CYGC_TOOLS_RC})
    target_include_directories(CyGameCaptureVideoProbe PRIVATE ${CYGC_FFMPEG_DIR}/include)
    target_link_libraries(CyGameCaptureVideoProbe PRIVATE CyGameCaptureBranding
        ${CYGC_FFMPEG_DIR}/lib/avcodec.lib
        ${CYGC_FFMPEG_DIR}/lib/avformat.lib
        ${CYGC_FFMPEG_DIR}/lib/avutil.lib)
    # FFmpeg's headers are C and lean on constructs MSVC warns about at /W4
    target_compile_options(CyGameCaptureVideoProbe PRIVATE /wd4244 /wd4996)
    # Delay loaded so the tool can point the loader at the OBS installation itself at start-up,
    # instead of requiring its bin folder on the PATH.
    target_link_libraries(CyGameCaptureVideoProbe PRIVATE delayimp)
    target_link_options(CyGameCaptureVideoProbe PRIVATE
        /DELAYLOAD:avcodec-60.dll /DELAYLOAD:avformat-60.dll /DELAYLOAD:avutil-58.dll)
    set_target_properties(CyGameCaptureVideoProbe PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CYGC_OUTPUT_DIR}"
        PDB_OUTPUT_DIRECTORY     "${CYGC_OUTPUT_DIR}")
else()
    message(STATUS
        "CyGameCaptureVideoProbe skipped: run Tools/GenerateFFmpegImportLibs.cmd once to build the "
        "import libraries from the FFmpeg DLLs of the installed OBS Studio.")
endif()
