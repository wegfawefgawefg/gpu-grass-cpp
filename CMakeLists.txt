cmake_minimum_required(VERSION 3.28)

project(gpu_grass_cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

include(FetchContent)

find_package(Vulkan REQUIRED)
find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_ALSA OFF CACHE BOOL "" FORCE)
set(SDL_JACK OFF CACHE BOOL "" FORCE)
set(SDL_PIPEWIRE OFF CACHE BOOL "" FORCE)
set(SDL_PULSEAUDIO OFF CACHE BOOL "" FORCE)
set(SDL_SNDIO OFF CACHE BOOL "" FORCE)
set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.2
  GIT_SHALLOW TRUE
)

FetchContent_Declare(
  SDL3_ttf
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
  GIT_TAG release-3.2.2
  GIT_SHALLOW TRUE
)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.8
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(SDL3)
FetchContent_MakeAvailable(SDL3_ttf)
FetchContent_MakeAvailable(imgui)

set(SHADER_NAMES
  grass.vert
  grass.frag
  ground.vert
  ground.frag
  marker.vert
  marker.frag
  present.vert
  present.frag
)

set(SHADER_OUTPUTS)
foreach(shader_name IN LISTS SHADER_NAMES)
  set(shader_source ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader_name})
  set(shader_output ${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader_name}.spv)
  add_custom_command(
    OUTPUT ${shader_output}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
    COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.3 -o ${shader_output} ${shader_source}
    DEPENDS ${shader_source}
    VERBATIM
  )
  list(APPEND SHADER_OUTPUTS ${shader_output})
endforeach()

add_custom_target(gpu-grass-cpp-shaders DEPENDS ${SHADER_OUTPUTS})

set(IMGUI_SOURCES
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

add_executable(gpu-grass-cpp
  src/app.cpp
  src/camera.cpp
  src/grass_field.cpp
  src/main.cpp
  src/text_overlay.cpp
  src/vulkan_helpers.cpp
  src/vulkan_renderer.cpp
  ${IMGUI_SOURCES}
)

target_compile_options(gpu-grass-cpp
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-missing-field-initializers
)

target_include_directories(gpu-grass-cpp
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(gpu-grass-cpp
  PRIVATE
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    Vulkan::Vulkan
)

target_compile_definitions(gpu-grass-cpp
  PRIVATE
    GPU_GRASS_VERT_SHADER_PATH="${CMAKE_CURRENT_BINARY_DIR}/shaders/grass.vert.spv"
    GPU_GRASS_FRAG_SHADER_PATH="${CMAKE_CURRENT_BINARY_DIR}/shaders/grass.frag.spv"
    GPU_GROUND_VERT_SHADER_PATH="${CMAKE_CURRENT_BINARY_DIR}/shaders/ground.vert.spv"
    GPU_GROUND_FRAG_SHADER_PATH="${CMAKE_CURRENT_BINARY_DIR}/shaders/ground.frag.spv"
    GPU_MARKER_VERT_SHADER_PATH="${CMAKE_CURRENT_BINARY_DIR}/shaders/marker.vert.spv"
    GPU_MARKER_FRAG_SHADER_PATH="${CMAKE_CURRENT_BINARY_DIR}/shaders/marker.frag.spv"
    GPU_PRESENT_VERT_SHADER_PATH="${CMAKE_CURRENT_BINARY_DIR}/shaders/present.vert.spv"
    GPU_PRESENT_FRAG_SHADER_PATH="${CMAKE_CURRENT_BINARY_DIR}/shaders/present.frag.spv"
  )

add_dependencies(gpu-grass-cpp gpu-grass-cpp-shaders)
