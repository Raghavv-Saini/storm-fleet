cmake_minimum_required(VERSION 3.20)
project(StormFleet CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4 GIT_SHALLOW TRUE)
FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG 1.0.1 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(glfw glm)

add_library(glad STATIC external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

# GL-free logic shared by the app and the tests
add_library(core STATIC
  src/ocean_core.cpp
  src/ship_core.cpp
  src/camera.cpp
  src/weather_core.cpp
  src/event.cpp)
target_include_directories(core PUBLIC src)
target_link_libraries(core PUBLIC glm::glm)
target_compile_definitions(core PUBLIC GLM_ENABLE_EXPERIMENTAL)

add_executable(stormfleet
  src/main.cpp
  src/gl_util.cpp
  src/ocean.cpp
  src/ship.cpp
  src/textures.cpp
  src/sky.cpp
  src/weather.cpp
  src/particles.cpp
  src/renderer.cpp
  src/audio.cpp)
target_include_directories(stormfleet PRIVATE external/stb external/miniaudio)
target_link_libraries(stormfleet PRIVATE core glad glfw)
if(WIN32)
  target_link_options(stormfleet PRIVATE -static -static-libgcc -static-libstdc++)
endif()

add_executable(tests tests/test_core.cpp)
target_link_libraries(tests PRIVATE core)

add_custom_command(TARGET stormfleet POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:stormfleet>/shaders)
