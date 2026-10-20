cmake_minimum_required(VERSION 3.26)
project(eversim LANGUAGES CXX)

# C++ Standard: C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for language servers (clangd, etc.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find SDL3, SDL3_image, SDL3_ttf packages
find_package(SDL3 CONFIG QUIET)
find_package(SDL3_image CONFIG QUIET)
find_package(SDL3_ttf CONFIG QUIET)

# Fall back to pkg-config if CMake config files were not found
if(NOT (SDL3_FOUND AND SDL3_image_FOUND AND SDL3_ttf_FOUND))
    find_package(PkgConfig REQUIRED)
    if(NOT SDL3_FOUND)
        pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
        add_library(SDL3::SDL3 ALIAS PkgConfig::SDL3)
    endif()
    if(NOT SDL3_image_FOUND)
        pkg_check_modules(SDL3_image REQUIRED IMPORTED_TARGET sdl3-image)
        add_library(SDL3_image::SDL3_image ALIAS PkgConfig::SDL3_image)
    endif()
    if(NOT SDL3_ttf_FOUND)
        pkg_check_modules(SDL3_ttf REQUIRED IMPORTED_TARGET sdl3-ttf)
        add_library(SDL3_ttf::SDL3_ttf ALIAS PkgConfig::SDL3_ttf)
    endif()
endif()

# Source and header files
set(SOURCES
    src/Main.cpp
    src/GraphicsDevice.cpp
    src/SpriteBatch.cpp
)

set(HEADERS
    src/GraphicsDevice.h
    src/SpriteBatch.h
    src/SpriteShaders.h
)

# Executable target
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_ttf::SDL3_ttf
)

# Shader compilation and SpriteShaders.h generation
find_program(SLANGC_EXECUTABLE
    NAMES slangc
    HINTS
        $ENV{VULKAN_SDK}/bin
        $ENV{VK_SDK_PATH}/bin
        $ENV{SLANG_DIR}/bin
        /home/sagara/Programs/vulkan/1.4.350.1/x86_64/bin
    DOC "Path to the Slang compiler executable"
)

set(SLANG_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/sprite.slang")
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders")
set(SPV_VS "${SHADER_OUTPUT_DIR}/sprite_vs.spv")
set(SPV_FS "${SHADER_OUTPUT_DIR}/sprite_fs.spv")
set(SHADER_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/src/SpriteShaders.h")
set(GENERATE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateShaderHeader.cmake")

if(SLANGC_EXECUTABLE)
    # Automatic generation whenever sprite.slang or GenerateShaderHeader.cmake changes
    add_custom_command(
        OUTPUT "${SPV_VS}" "${SPV_FS}" "${SHADER_HEADER}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
        COMMAND ${SLANGC_EXECUTABLE} "${SLANG_SOURCE}"
            -target spirv -profile spirv_1_0 -Wno-50011 -entry vs_main -stage vertex
            -o "${SPV_VS}"
        COMMAND ${SLANGC_EXECUTABLE} "${SLANG_SOURCE}"
            -target spirv -profile spirv_1_0 -Wno-50011 -entry fs_main -stage fragment
            -o "${SPV_FS}"
        COMMAND ${CMAKE_COMMAND}
            "-DVS_SPV=${SPV_VS}"
            "-DFS_SPV=${SPV_FS}"
            "-DOUTPUT_HEADER=${SHADER_HEADER}"
            -P "${GENERATE_SCRIPT}"
        DEPENDS "${SLANG_SOURCE}" "${GENERATE_SCRIPT}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Compiling Slang shaders and generating SpriteShaders.h"
    )

    add_custom_target(generate_shader_header
        DEPENDS "${SPV_VS}" "${SPV_FS}" "${SHADER_HEADER}"
    )
    add_dependencies(${PROJECT_NAME} generate_shader_header)

    # Explicit 'shaders' target to force recompilation anytime
    add_custom_target(shaders
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
        COMMAND ${SLANGC_EXECUTABLE} "${SLANG_SOURCE}"
            -target spirv -profile spirv_1_0 -Wno-50011 -entry vs_main -stage vertex
            -o "${SPV_VS}"
        COMMAND ${SLANGC_EXECUTABLE} "${SLANG_SOURCE}"
            -target spirv -profile spirv_1_0 -Wno-50011 -entry fs_main -stage fragment
            -o "${SPV_FS}"
        COMMAND ${CMAKE_COMMAND}
            "-DVS_SPV=${SPV_VS}"
            "-DFS_SPV=${SPV_FS}"
            "-DOUTPUT_HEADER=${SHADER_HEADER}"
            -P "${GENERATE_SCRIPT}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Compiling Slang shaders and generating SpriteShaders.h"
        SOURCES "${SLANG_SOURCE}"
    )
else()
    if(EXISTS "${SHADER_HEADER}")
        message(STATUS "slangc not found. Using precompiled shader header: ${SHADER_HEADER}")
    else()
        message(WARNING "slangc not found and ${SHADER_HEADER} does not exist. Specify -DSLANGC_EXECUTABLE=/path/to/slangc.")
    endif()

    add_custom_target(shaders
        COMMAND ${CMAKE_COMMAND} -E echo "Error: slangc not found. Please set SLANGC_EXECUTABLE to your slangc binary."
        COMMAND ${CMAKE_COMMAND} -E false
    )
endif()

# Automatically sync assets to the build directory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_target(copy_assets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "${CMAKE_CURRENT_BINARY_DIR}/assets"
        COMMENT "Syncing assets to build directory"
    )
    if(TARGET generate_shader_header)
        add_dependencies(copy_assets generate_shader_header)
    endif()
    add_dependencies(${PROJECT_NAME} copy_assets)
endif()
