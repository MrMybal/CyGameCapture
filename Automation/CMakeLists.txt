# MIT transport adapter; native applications retain their own licenses.
set(CYGC_CYMCP_SOURCE_DIR "D:/_Project/CyMCPSDK" CACHE PATH "CyMCPSDK source checkout")
    set(CYMCP_BUILD_HTTP ON CACHE BOOL "" FORCE)
    set(CYMCP_BUILD_PLATFORM ON CACHE BOOL "" FORCE)
    set(CYMCP_BUILD_MCP ON CACHE BOOL "" FORCE)
    set(CYMCP_BUILD_AI ON CACHE BOOL "" FORCE)
    set(CYMCP_BUILD_LOCAL OFF CACHE BOOL "" FORCE)
    set(CYMCP_BUILD_CHAT_UI OFF CACHE BOOL "" FORCE)
    set(CYMCP_BUILD_CHAT_APP OFF CACHE BOOL "" FORCE)
    set(CYMCP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CYMCP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
if(NOT EXISTS "${CYGC_CYMCP_SOURCE_DIR}/CMakeLists.txt")
    include(FetchContent)
    FetchContent_Declare(cygc_cymcp GIT_REPOSITORY https://github.com/MrMybal/CyMCPSDK.git GIT_TAG 34d4bcae126bc83d1a2925b5047dac36e3d09d92)
    FetchContent_MakeAvailable(cygc_cymcp)
    set(CYGC_CYMCP_SOURCE_DIR "${cygc_cymcp_SOURCE_DIR}")
else()
    # Compile an immutable snapshot from the local checkout. Other projects may be
    # editing that checkout concurrently; their uncommitted changes are never touched.
    find_package(Git REQUIRED)
    set(CYGC_CYMCP_REVISION "34d4bcae126bc83d1a2925b5047dac36e3d09d92")
    set(CYGC_CYMCP_BUILD_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/source-${CYGC_CYMCP_REVISION}")
    if(NOT EXISTS "${CYGC_CYMCP_BUILD_SOURCE_DIR}/CMakeLists.txt")
        set(sdk_archive "${CMAKE_CURRENT_BINARY_DIR}/CyMCPSDK-${CYGC_CYMCP_REVISION}.tar")
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${CYGC_CYMCP_SOURCE_DIR}" archive --format=tar "--output=${sdk_archive}" "${CYGC_CYMCP_REVISION}"
            RESULT_VARIABLE sdk_archive_result)
        if(NOT sdk_archive_result EQUAL 0)
            message(FATAL_ERROR "Cannot snapshot the selected CyMCPSDK source revision")
        endif()
        file(ARCHIVE_EXTRACT INPUT "${sdk_archive}" DESTINATION "${CYGC_CYMCP_BUILD_SOURCE_DIR}")
    endif()
    add_subdirectory("${CYGC_CYMCP_BUILD_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/CyMCPSDK")
endif()
add_library(CyGameCaptureAutomationClient STATIC Source/Client.cpp)
target_include_directories(CyGameCaptureAutomationClient PUBLIC Include)
target_link_libraries(CyGameCaptureAutomationClient PUBLIC CyMCP::AI CyMCP::MCP)
target_compile_definitions(CyGameCaptureAutomationClient PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_executable(CyGameCaptureCLI Source/CLI.cpp)
target_link_libraries(CyGameCaptureCLI PRIVATE CyGameCaptureAutomationClient)
set_target_properties(CyGameCaptureCLI PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CYGC_OUTPUT_DIR}")
add_custom_command(TARGET CyGameCaptureCLI POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_SOURCE_DIR}/CyGameCaptureUE/Binaries/ThirdParty/CyMCPSDK/Win64"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:CyGameCaptureCLI>" "${CMAKE_SOURCE_DIR}/CyGameCaptureUE/Binaries/ThirdParty/CyMCPSDK/Win64/CyGameCaptureCLI.exe"
    VERBATIM)
add_library(CyGameCaptureMCP SHARED Source/Host.cpp)
target_compile_definitions(CyGameCaptureMCP PRIVATE CYGC_AUTOMATION_BUILD NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(CyGameCaptureMCP PUBLIC Include)
target_link_libraries(CyGameCaptureMCP PRIVATE CyMCP::Core CyMCP::MCP CyMCP::Http CyMCP::AI)
set_target_properties(CyGameCaptureMCP PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CYGC_OUTPUT_DIR}")
foreach(destination "${CYGC_OUTPUT_DIR}/TestApp" "${CYGC_OUTPUT_DIR}/TestAppD3D12" "${CYGC_OUTPUT_DIR}/obs-plugins/64bit")
    add_custom_command(TARGET CyGameCaptureMCP POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${destination}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:CyGameCaptureMCP>" "${destination}/CyGameCaptureMCP.dll"
        VERBATIM)
endforeach()
add_custom_command(TARGET CyGameCaptureMCP POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_SOURCE_DIR}/CyGameCaptureUE/Binaries/ThirdParty/CyMCPSDK/Win64"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:CyGameCaptureMCP>" "${CMAKE_SOURCE_DIR}/CyGameCaptureUE/Binaries/ThirdParty/CyMCPSDK/Win64/CyGameCaptureMCP.dll"
    VERBATIM)
# The app's ImGui is supplied by ReShade, not CyUI's ABI. Conversation is used in the existing panel.

# Local integration probe, never installed or packaged.
add_executable(CyGameCaptureTransportProbe EXCLUDE_FROM_ALL Tests/TransportProbe.cpp)
target_link_libraries(CyGameCaptureTransportProbe PRIVATE CyMCP::MCP)
set_target_properties(CyGameCaptureTransportProbe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
