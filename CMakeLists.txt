cmake_minimum_required(VERSION 3.16)

project(app)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Output directory for built objects.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# SDL
set(SDL_STATIC ON)
set(SDL_SHARED OFF)
add_subdirectory(thirdparty/SDL EXCLUDE_FROM_ALL)

# glm
set(GLM_BUILD_TESTS OFF)
add_subdirectory(thirdparty/glm EXCLUDE_FROM_ALL)

add_executable(app WIN32 src/app_main.cpp src/gfx.cpp)

target_include_directories(app PRIVATE thirdparty/stb)
target_link_libraries(app PRIVATE SDL3::SDL3)
target_link_libraries(app PRIVATE glm::glm)
