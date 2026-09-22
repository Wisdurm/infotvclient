cmake_minimum_required(VERSION 3.31)
include(FetchContent)
project(infotvclient
  DESCRIPTION "Show student status on tv screen"
)
# set(CMAKE_CXX_STANDARD 2)
# Libraries
FetchContent_Declare(
  ixwebsocket
  GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
  GIT_TAG v12.0.1
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(ixwebsocket)
FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL
  GIT_TAG main
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(SDL3)
# Build
add_executable(infotvclient
  main.cpp
)
target_link_libraries(infotvclient PRIVATE SDL3::SDL3 ixwebsocket)
