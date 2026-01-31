cmake_minimum_required(VERSION 3.20)
project(deepbound LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies ---
# Note: In a real environment, we'd use find_package or FetchContent.
# For now, I'll setup the structure and assume standard headers/libs are available or will be added.

include(FetchContent)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
  glad
  GIT_REPOSITORY https://github.com/Dav1dde/glad.git
  GIT_TAG v0.1.36
)
FetchContent_MakeAvailable(glad)

# stb (Image Loading)
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG master
)
FetchContent_MakeAvailable(stb)

# GLM
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

# FastNoise2
FetchContent_Declare(
  FastNoise2
  GIT_REPOSITORY https://github.com/Auburn/FastNoise2.git
  GIT_TAG v0.10.0-alpha
)
FetchContent_MakeAvailable(FastNoise2)

# ImGui
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

# Create ImGui Library
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
target_link_libraries(imgui PUBLIC glfw glad) # backends need glfw and glad/gl

# Core Library
add_library(deepbound_core STATIC)
target_include_directories(deepbound_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(deepbound_core PUBLIC ${glad_SOURCE_DIR}/include)

file(GLOB_RECURSE CORE_SOURCES src/core/*.cpp src/core/*.hpp)
# Add glad.c to sources (it's usually in src/glad.c or similar in the repo, need to check structure or use a simpler loader)
# Actually, the dav1dde/glad repo might be the generator. simpler to use a pre-generated one or just use `gl3w` or something simpler?
# A better way is to use a dedicated glad cmake repo or just download the files.
# Let's try to just download the files directly to `src/thirdparty` or something simpler. 
# But let's check if there is a 'glad-cmake' wrapper.

# Using a standard GLAD cmake approach is tricky without a proper repo.
# Let's use a simpler repo that provides CMake support or just include the files if I can.
# Actually, the user has `vssurvivalmod-master` which might have hints, but unlikely.
# I'll use a reliable mirror or just standard `https://github.com/Dav1dde/glad` and hope `add_subdirectory` works if it has one? No, it's the generator.

# BETTER PLAN: Use `libglad` or just `FetchContent` a repository that HAS the generated files.
# https://github.com/libigl/libigl-glad is often used.

target_sources(deepbound_core PRIVATE ${CORE_SOURCES})

target_link_libraries(deepbound_core PUBLIC glfw nlohmann_json::nlohmann_json glad glm::glm FastNoise2 imgui)
target_compile_definitions(deepbound_core PUBLIC GLFW_INCLUDE_NONE)
target_include_directories(deepbound_core PUBLIC ${stb_SOURCE_DIR})

# Game App
add_executable(deepbound_game src/game/main.cpp)
target_link_libraries(deepbound_game PRIVATE deepbound_core)

# Editor App
add_executable(deepbound_editor src/editor/main.cpp)
target_link_libraries(deepbound_editor PRIVATE deepbound_core)

# --- Resource Copy (Optional but good for dev) ---
add_custom_command(TARGET deepbound_game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:deepbound_game>/assets)
