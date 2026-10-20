# GenerateShaderHeader.cmake
# Cross-platform script to convert compiled SPIR-V binaries into a C++ header.
#
# Parameters passed via -D:
#   VS_SPV: Path to compiled vertex shader SPIR-V binary
#   FS_SPV: Path to compiled fragment shader SPIR-V binary
#   OUTPUT_HEADER: Destination path for the generated C++ header

if(NOT DEFINED VS_SPV OR NOT DEFINED FS_SPV OR NOT DEFINED OUTPUT_HEADER)
    message(FATAL_ERROR "VS_SPV, FS_SPV, and OUTPUT_HEADER must all be defined.")
endif()

if(NOT EXISTS "${VS_SPV}")
    message(FATAL_ERROR "Vertex shader SPIR-V file not found: ${VS_SPV}")
endif()

if(NOT EXISTS "${FS_SPV}")
    message(FATAL_ERROR "Fragment shader SPIR-V file not found: ${FS_SPV}")
endif()

# Function to convert a binary file into a formatted C++ uint8_t array
function(format_spv_to_c_array FILE_PATH ARRAY_NAME OUT_VAR)
    file(READ "${FILE_PATH}" HEX_CONTENT HEX)
    string(REGEX MATCHALL ".." BYTES "${HEX_CONTENT}")

    set(RESULT "inline constexpr uint8_t ${ARRAY_NAME}[] = {\n")
    set(LINE "    ")
    set(COL 0)
    foreach(BYTE ${BYTES})
        if(COL GREATER 0)
            string(APPEND LINE " ")
        endif()
        string(APPEND LINE "0x${BYTE},")
        math(EXPR COL "${COL} + 1")
        if(COL EQUAL 12)
            string(APPEND RESULT "${LINE}\n")
            set(LINE "    ")
            set(COL 0)
        endif()
    endforeach()
    if(NOT COL EQUAL 0)
        string(APPEND RESULT "${LINE}\n")
    endif()
    string(APPEND RESULT "};\n")
    string(APPEND RESULT "inline constexpr size_t ${ARRAY_NAME}_Size = sizeof(${ARRAY_NAME});\n")
    set(${OUT_VAR} "${RESULT}" PARENT_SCOPE)
endfunction()

format_spv_to_c_array("${VS_SPV}" "SpriteVertexShaderSPIRV" VS_CODE)
format_spv_to_c_array("${FS_SPV}" "SpriteFragmentShaderSPIRV" FS_CODE)

set(HEADER_CONTENT "// Generated automatically from src/shaders/sprite.slang by CMake. Do not edit directly.
#pragma once
#include <cstdint>
#include <cstddef>

namespace SpriteShaders {

// Compiled from src/shaders/sprite.slang (SPIR-V 1.0 for Vulkan 1.0 baseline)
${VS_CODE}
${FS_CODE}
} // namespace SpriteShaders
")

# Only write to file if content has changed to avoid unnecessary re-compilations
if(EXISTS "${OUTPUT_HEADER}")
    file(READ "${OUTPUT_HEADER}" CURRENT_CONTENT)
    if("${CURRENT_CONTENT}" STREQUAL "${HEADER_CONTENT}")
        message(STATUS "Shader header is up to date: ${OUTPUT_HEADER}")
        return()
    endif()
endif()

file(WRITE "${OUTPUT_HEADER}" "${HEADER_CONTENT}")
message(STATUS "Generated shader header: ${OUTPUT_HEADER}")

