# Rockband Mod (2026) addition -- see README.md for the credit / changes
# note (AGPL-3.0 §5a).
#
# Dev-time only: converts a Unity FBX mesh (as used by YARG,
# https://github.com/YARC-Official/YARG, LGPL-3.0-or-later) into a plain
# Wavefront .obj that plugin/src/ModelAsset.cpp can parse without Assimp (or
# anything else) linked into the shipped plugin. This tool is never part of
# the main plugin build -- build and run it by hand, once per asset; the
# .obj/.png output is what actually ships, checked into
# plugin/assets/models/.
#
# Usage:
#   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#   build/model_convert.exe <input.fbx> <output.obj>
cmake_minimum_required(VERSION 3.24)
project(model_convert)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_VIEW OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
# only the FBX importer is needed
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
FetchContent_Declare(assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.4.3
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(assimp)

add_executable(model_convert model_convert.cpp)
target_link_libraries(model_convert PRIVATE assimp::assimp)
