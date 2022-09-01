cmake_minimum_required(VERSION 3.14)

project(Ray LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "-o3")

find_package(Qt6 REQUIRED COMPONENTS Concurrent)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Gui)
find_package(Qt6 REQUIRED COMPONENTS Xml)

add_definitions(-D_USE_MATH_DEFINES)
add_definitions(-DTIXML_USE_STL)
add_definitions(-DGLM_SWIZZLE)
add_definitions(-DGLM_FORCE_RADIANS)

add_executable(Ray
  main.cpp

  ./utils/RGBA.h
  ./utils/RGBA.cpp
  ./utils/SceneData.h
  ./utils/ScenefileReader.h
  ./utils/ScenefileReader.cpp
  ./utils/SceneParser.h
  ./utils/SceneParser.cpp
)

target_link_libraries(Ray PRIVATE
    Qt6::Concurrent
    Qt6::Core
    Qt6::Gui
    Qt6::Xml
)

if (MSVC OR MSYS OR MINGW)
    # Set this flag to silence warning on Windows
    set(CMAKE_CXX_FLAGS "-Wno-volatile")
endif()

if (APPLE)
    # Set this flag to silence warning on macOS
    set(CMAKE_CXX_FLAGS "-Wno-deprecated-volatile")
endif()
