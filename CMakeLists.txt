cmake_minimum_required(VERSION 3.0...3.25)
project(
    flecs-wgpu
    VERSION 0.1.0
    LANGUAGES CXX C
)

file(GLOB_RECURSE SRC src/*.cpp)
add_executable(app ${SRC})

option(FLECS_SHARED "Build shared flecs lib" ON)
set(FLECS_SHARED OFF)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
set(BUILD_SHARED_LIBS OFF)

option(BOX2D_BUILD_TESTBED "Build the Box2D testbed" ON)
set(BOX2D_BUILD_TESTBED OFF)

if (NOT EMSCRIPTEN)
    # Do not include this with emscripten, it provides its own version.
    add_subdirectory(libs/glfw)
else()
    add_library(glfw INTERFACE)
    target_link_options(glfw INTERFACE -sUSE_GLFW=3)

    # add_library(webgpu INTERFACE)
    # target_link_options(webgpu INTERFACE -sUSE_WEBGPU=1)
    # function(target_copy_webgpu_binaries)
    # endfunction()
endif()

add_subdirectory(libs/webgpu)
add_subdirectory(libs/glfw3webgpu)
add_subdirectory(libs/flecs)
add_subdirectory(libs/box2d)

target_link_libraries(app PRIVATE glfw webgpu glfw3webgpu flecs_static box2d)

target_copy_webgpu_binaries(app)

set_target_properties(app PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON
)

if (MSVC)
    # Disable macro redefinition warning for glfw
    target_compile_options(app PRIVATE /W4 /wd4127 /wd4005)
else()
    target_compile_options(app PRIVATE -Wall -Wextra -pedantic)
endif()

if (EMSCRIPTEN)
    # Add Emscripten-specific link options
    target_link_options(app PRIVATE
        -sUSE_GLFW=3 # Use Emscripten-provided GLFW
        -sUSE_WEBGPU # Handle WebGPU symbols
        -sASYNCIFY # Required by WebGPU-C++
        -sALLOW_MEMORY_GROWTH
        -sSTACK_SIZE=1mb
        -sEXPORTED_RUNTIME_METHODS=cwrap 
        -sMODULARIZE
        -sEXPORT_NAME="app"
    )
    set_target_properties(app PROPERTIES SUFFIX ".js")
endif()

target_compile_definitions(app PRIVATE
    ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
