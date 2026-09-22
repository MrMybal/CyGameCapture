# CyGameCaptureRS — native ReShade add-on (.addon64)
enable_language(RC)

set(CYGC_RS_THIRDPARTY ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty)

# Spout2 subset that a DirectX *sender* needs. No OpenGL, no SpoutLibrary.
# (SpoutDX / SpoutDX12 / SpoutDirectX / SpoutCopy are vendored for the future D3D12 bridge
#  but not compiled into the add-on yet.)
set(CYGC_SPOUT_SOURCES
    ${CYGC_RS_THIRDPARTY}/Spout2/SpoutGL/SpoutUtils.cpp
    ${CYGC_RS_THIRDPARTY}/Spout2/SpoutGL/SpoutSharedMemory.cpp
    ${CYGC_RS_THIRDPARTY}/Spout2/SpoutGL/SpoutSenderNames.cpp
    ${CYGC_RS_THIRDPARTY}/Spout2/SpoutGL/SpoutFrameCount.cpp)
set_source_files_properties(${CYGC_SPOUT_SOURCES} PROPERTIES COMPILE_OPTIONS "/W1")

set(CYGC_RS_SOURCES
    Source/Addon/AddonMain.cpp
    Source/Addon/Log.hpp
    Source/Addon/Log.cpp
    Source/Addon/Config.hpp
    Source/Addon/Config.cpp
    Source/Addon/DeviceContext.hpp
    Source/Addon/DeviceContext.cpp
    Source/Addon/CyGameCaptureRS.rc
    Source/ResourceTracker/FormatUtils.hpp
    Source/ResourceTracker/FormatUtils.cpp
    Source/ResourceTracker/ResourceTracker.hpp
    Source/ResourceTracker/ResourceTracker.cpp
    Source/BufferInspector/InspectorModel.hpp
    Source/BufferInspector/InspectorModel.cpp
    Source/Preview/PreviewManager.hpp
    Source/Preview/PreviewManager.cpp
    Source/Preview/ThumbnailGrid.hpp
    Source/Preview/ThumbnailGrid.cpp
    Source/Capture/SpoutSender.hpp
    Source/Capture/SpoutSender.cpp
    Source/Capture/CaptureManager.hpp
    Source/Capture/CaptureManager.cpp
    Source/Capture/FrameSnapshots.hpp
    Source/Capture/FrameSnapshots.cpp
    Source/Assistant/AiAssistant.hpp
    Source/Assistant/AiAssistant.cpp
    Source/Backends/Backend.hpp
    Source/Backends/Backend.cpp
    Source/Backends/D3D12/D3D11On12Bridge.hpp
    Source/Backends/D3D12/D3D11On12Bridge.cpp
    Source/GpuPass/FullscreenPass.hpp
    Source/GpuPass/FullscreenPass.cpp
    Source/GpuPass/PassInput.hpp
    Source/GpuPass/PassInput.cpp
    Source/GpuPass/DepthPass.hpp
    Source/GpuPass/DepthPass.cpp
    Source/GpuPass/HudPass.hpp
    Source/GpuPass/HudPass.cpp
    Source/GpuPass/BlitPass.hpp
    Source/GpuPass/BlitPass.cpp
    Source/UI/Overlay.hpp
    Source/UI/Overlay.cpp
    Source/UI/Logo.hpp
    Source/UI/Logo.cpp
    Source/UI/Localization.hpp
    Source/UI/Localization.cpp
    Source/UI/Translations_fr.inl)

set_source_files_properties(${CYGC_RS_SOURCES} PROPERTIES COMPILE_DEFINITIONS "WIN32_LEAN_AND_MEAN")

add_library(CyGameCaptureRS SHARED ${CYGC_RS_SOURCES} ${CYGC_SPOUT_SOURCES})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/Source PREFIX "Source" FILES ${CYGC_RS_SOURCES})
source_group("ThirdParty\\Spout2" FILES ${CYGC_SPOUT_SOURCES})

target_include_directories(CyGameCaptureRS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CYGC_RS_THIRDPARTY}/reshade/include
    ${CYGC_RS_THIRDPARTY}/imgui
    ${CYGC_RS_THIRDPARTY}/Spout2/SpoutGL)

target_compile_definitions(CyGameCaptureRS PRIVATE
    ImTextureID=ImU64              # required by reshade_overlay.hpp (resource_view handle == 64-bit)
    SPOUT_BUILD_STATIC             # Spout compiled straight into the add-on
    CYGC_ADDON_NAME="CyGameCaptureRS")

target_link_libraries(CyGameCaptureRS PRIVATE CyGameCaptureCore CyGameCaptureBranding
    d3d11 d3d12 dxgi dxguid d3dcompiler winmm shell32 advapi32 version comctl32 psapi user32 gdi32 ole32 windowscodecs)

set_target_properties(CyGameCaptureRS PROPERTIES
    OUTPUT_NAME "CyGameCaptureRS"
    PREFIX ""
    SUFFIX ".addon64"
    RUNTIME_OUTPUT_DIRECTORY "${CYGC_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CYGC_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${CYGC_OUTPUT_DIR}"
    PDB_OUTPUT_DIRECTORY     "${CYGC_OUTPUT_DIR}")

# Keep symbols in Release too: crash dumps from inside games are much easier to read.
target_compile_options(CyGameCaptureRS PRIVATE $<$<CONFIG:Release>:/Zi>)
target_link_options(CyGameCaptureRS PRIVATE $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>)
# The binary only names its symbol file, without the folder it was built in
target_link_options(CyGameCaptureRS PRIVATE /PDBALTPATH:%_PDB%)
