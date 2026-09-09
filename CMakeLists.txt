cmake_minimum_required(VERSION 3.16)
project(DoomGrok LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 REQUIRED)

add_executable(doom-grok
  app/main.cpp
  game/player.cpp
  ai/ai.cpp
  engine/audio/audio.cpp
  engine/camera/camera.cpp
  engine/collision/collision.cpp
  engine/input/input.cpp
  engine/map/map.cpp
  engine/timing/timing.cpp
  engine/renderer/raycast.cpp
)

target_include_directories(doom-grok PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${SDL2_INCLUDE_DIRS}
)

if(TARGET SDL2::SDL2)
  target_link_libraries(doom-grok PRIVATE SDL2::SDL2)
else()
  target_link_libraries(doom-grok PRIVATE ${SDL2_LIBRARIES})
endif()

# Copy campaign maps next to the binary for convenience
add_custom_command(TARGET doom-grok POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:doom-grok>/levels
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/levels/demo.map
          $<TARGET_FILE_DIR:doom-grok>/levels/demo.map
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/levels/e1m2.map
          $<TARGET_FILE_DIR:doom-grok>/levels/e1m2.map
)
