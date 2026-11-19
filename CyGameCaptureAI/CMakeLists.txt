# CyGameCaptureAI — the helper behind the add-on's "Find the setting that removes the game's interface".
#
# A separate process on purpose: reading the pictures back, encoding them and waiting minutes for an AI
# client must never happen inside the game. It sits next to CyGameCaptureRS.addon64, which starts it.
add_executable(CyGameCaptureAI
    Source/main.cpp
    Source/MiniJson.hpp
    Source/Pictures.hpp
    Source/Pictures.cpp
    Source/AiClients.hpp
    Source/AiClients.cpp
    Source/SdkConversation.cpp
    Source/CyGameCaptureAI.rc)

target_compile_definitions(CyGameCaptureAI PRIVATE WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(CyGameCaptureAI PRIVATE CyGameCaptureCore CyGameCaptureBranding CyGameCaptureAutomationClient CyMCP::AI CyMCP::MCP d3d11 dxgi windowscodecs ole32)

set_target_properties(CyGameCaptureAI PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CYGC_OUTPUT_DIR}"
    PDB_OUTPUT_DIRECTORY     "${CYGC_OUTPUT_DIR}")

foreach(cygc_mcp_destination "${CYGC_OUTPUT_DIR}/TestApp" "${CYGC_OUTPUT_DIR}/TestAppD3D12" "${CYGC_OUTPUT_DIR}/obs-plugins/64bit")
    add_custom_command(TARGET CyGameCaptureAI POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${cygc_mcp_destination}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:CyGameCaptureMCP>" "${cygc_mcp_destination}/CyGameCaptureMCP.dll"
        VERBATIM)
endforeach()
