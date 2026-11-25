cmake_minimum_required(VERSION 3.16)
project(Roadkill C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(ROADKILL_WARNINGS -Wall -Wextra)

# The game: the verified decompiled code (src/<area>), the runtime that runs it (src/rt) and the
# host services standing in for Win32 / DirectX / the Watcom C runtime (src/host).
file(GLOB_RECURSE ROADKILL_GAME CONFIGURE_DEPENDS src/*.c)
list(FILTER ROADKILL_GAME EXCLUDE REGEX "src/app/")
list(FILTER ROADKILL_GAME EXCLUDE REGEX "src/platform/sdl_display\\.c$")

# SDL2: the system's by default; ROADKILL_BUNDLED_SDL=ON builds SDL 2.32.10 from source and links it
# into the executable (the release builds: one file, nothing to install)
option(ROADKILL_BUNDLED_SDL "Build SDL2 from source and link it statically" OFF)
if(ROADKILL_BUNDLED_SDL)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL2
        URL https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz
        URL_HASH SHA256=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165)
    FetchContent_MakeAvailable(SDL2)
    set(ROADKILL_SDL SDL2::SDL2-static)
else()
    find_package(SDL2 REQUIRED CONFIG)
    set(ROADKILL_SDL SDL2::SDL2)
endif()
find_package(Threads REQUIRED)

# The movies' video decoders: FFmpeg's Indeo 3 / Indeo 4, LGPL-2.1-or-later (third_party/indeo/README.md).
# The FFmpeg files build as they are, without warnings; the glue (ffcompat.c, indeo.c) with them.
file(GLOB INDEO_SRC CONFIGURE_DEPENDS third_party/indeo/*.c)
add_library(indeo STATIC ${INDEO_SRC})
target_include_directories(indeo PUBLIC third_party/indeo)
set_source_files_properties(third_party/indeo/indeo3.c third_party/indeo/indeo4.c third_party/indeo/ivi.c
    third_party/indeo/ivi_dsp.c PROPERTIES COMPILE_OPTIONS "-w")

add_executable(roadkill src/app/tm2.c ${ROADKILL_GAME})
target_include_directories(roadkill PRIVATE src src/rt)
target_link_libraries(roadkill PRIVATE indeo ${ROADKILL_SDL} Threads::Threads m)
if(MINGW)
    # one roadkill.exe beside SDL2.dll: the C runtime pieces (winpthread, libgcc) linked in
    target_link_options(roadkill PRIVATE -static -static-libgcc)
    # a windowed program (no console window beside the game); ROADKILL_WIN_CONSOLE=ON keeps the
    # console for the host's messages on stderr
    option(ROADKILL_WIN_CONSOLE "Windows: show a console with the log" OFF)
    if(NOT ROADKILL_WIN_CONSOLE)
        target_link_options(roadkill PRIVATE -mwindows)
    endif()
    if(NOT ROADKILL_BUNDLED_SDL)
        add_custom_command(TARGET roadkill POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:roadkill>)
    endif()
endif()

# tools/test_movie: decodes a movie's first frames to PPM / raw YUV (checks the decoders)
add_executable(test_movie tools/test_movie.c src/host/movies/avi.c)
target_include_directories(test_movie PRIVATE src)
target_link_libraries(test_movie PRIVATE indeo)

# The x87 float code the game was verified against: no fused multiply-adds anywhere, and the trig
# at the x87 helper boundary must stay separate sin / cos calls (docs/RUNTIME.md).
target_compile_options(roadkill PRIVATE -ffp-contract=off ${ROADKILL_WARNINGS})
set_source_files_properties(src/engine/en10_viewport.c src/engine/en10_view.c src/engine/en10_dsound.c
    PROPERTIES COMPILE_OPTIONS "-fno-builtin")

# Optional: the arena viewer (needs OpenGL).
option(ROADKILL_VIEWER "Build the arena viewer (tm2view)" OFF)
if(ROADKILL_VIEWER)
    find_package(OpenGL REQUIRED)
    add_library(roadkill_game STATIC ${ROADKILL_GAME})
    target_include_directories(roadkill_game PUBLIC src src/rt)
    target_link_libraries(roadkill_game PUBLIC indeo)
    target_compile_options(roadkill_game PRIVATE -ffp-contract=off)
    add_executable(tm2view src/app/viewer.c)
    target_link_libraries(tm2view PRIVATE roadkill_game ${ROADKILL_SDL} OpenGL::GL Threads::Threads m)
    if(APPLE)
        target_compile_definitions(tm2view PRIVATE GL_SILENCE_DEPRECATION)
    endif()
endif()
