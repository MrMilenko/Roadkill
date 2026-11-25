cmake_minimum_required(VERSION 3.16)
project(Roadkill C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra)

# The game: the verified decompiled code (src/<area>), the runtime that runs it (src/rt) and the
# host services standing in for Win32 / DirectX / the Watcom C runtime (src/host).
file(GLOB_RECURSE ROADKILL_GAME CONFIGURE_DEPENDS src/*.c)
list(FILTER ROADKILL_GAME EXCLUDE REGEX "src/app/")
list(FILTER ROADKILL_GAME EXCLUDE REGEX "src/platform/sdl_display\\.c$")

find_package(SDL2 REQUIRED CONFIG)
find_package(Threads REQUIRED)

add_executable(roadkill src/app/tm2.c ${ROADKILL_GAME})
target_include_directories(roadkill PRIVATE src src/rt)
target_link_libraries(roadkill PRIVATE SDL2::SDL2 Threads::Threads m)
# The x87 float code the game was verified against: no fused multiply-adds anywhere, and the trig
# at the x87 helper boundary must stay separate sin / cos calls (docs/RUNTIME.md).
target_compile_options(roadkill PRIVATE -ffp-contract=off)
set_source_files_properties(src/engine/en10_viewport.c src/engine/en10_view.c src/engine/en10_dsound.c
    PROPERTIES COMPILE_OPTIONS "-fno-builtin")

# Optional: the arena viewer (needs OpenGL).
option(ROADKILL_VIEWER "Build the arena viewer (tm2view)" OFF)
if(ROADKILL_VIEWER)
    find_package(OpenGL REQUIRED)
    add_library(roadkill_game STATIC ${ROADKILL_GAME})
    target_include_directories(roadkill_game PUBLIC src src/rt)
    target_compile_options(roadkill_game PRIVATE -ffp-contract=off)
    add_executable(tm2view src/app/viewer.c)
    target_link_libraries(tm2view PRIVATE roadkill_game SDL2::SDL2 OpenGL::GL Threads::Threads m)
    if(APPLE)
        target_compile_definitions(tm2view PRIVATE GL_SILENCE_DEPRECATION)
    endif()
endif()
