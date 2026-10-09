cmake_minimum_required(VERSION 3.21)
project(foghoge LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# --- raylib 5.5, pinned ------------------------------------------------------
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(CUSTOMIZE_BUILD OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

# --- game --------------------------------------------------------------------
add_executable(foghoge src/main.cpp)
target_link_libraries(foghoge PRIVATE raylib)

if(MSVC)
  target_compile_options(foghoge PRIVATE /W4 /utf-8)
else()
  target_compile_options(foghoge PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Copy runtime data next to the executable so it runs from any working directory.
add_custom_command(TARGET foghoge POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:foghoge>/assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:foghoge>/shaders
)
