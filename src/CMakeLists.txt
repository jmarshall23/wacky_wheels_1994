cmake_minimum_required(VERSION 3.24)

# Do not let Visual Studio's machine-wide vcpkg integration inject DLL-copy
# targets.  All non-system code for this project lives under thirdparty/.
set(CMAKE_VS_GLOBALS "VcpkgEnabled=false;VcpkgApplocalDeps=false")

project(WackyWheels95 VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
if(MSVC)
    # Keep the executable self-contained: SDL and the MSVC runtime are linked
    # into ww95.exe, leaving only Windows system DLL imports.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

get_filename_component(WW_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(WW_RUNTIME_OUTPUT "${WW_PROJECT_ROOT}/out")

# SDL is deliberately vendored and statically linked.  Keep third-party build
# products inside the main build tree and do not install SDL from this project.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/SDL EXCLUDE_FROM_ALL)

set(TIMIDITY_SOURCES
    thirdparty/libtimidity/src/common.c
    thirdparty/libtimidity/src/instrum.c
    thirdparty/libtimidity/src/mix.c
    thirdparty/libtimidity/src/output.c
    thirdparty/libtimidity/src/playmidi.c
    thirdparty/libtimidity/src/readmidi.c
    thirdparty/libtimidity/src/resample.c
    thirdparty/libtimidity/src/stream.c
    thirdparty/libtimidity/src/tables.c
    thirdparty/libtimidity/src/timidity.c
)
add_library(timidity STATIC ${TIMIDITY_SOURCES})
target_include_directories(timidity PUBLIC thirdparty/libtimidity/src)
target_compile_definitions(timidity PRIVATE
    TIMIDITY_BUILD=1 TIMIDITY_STATIC=1 _CRT_SECURE_NO_WARNINGS)
set_target_properties(timidity PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED TRUE)

add_executable(ww95
    main.c
    ww_common.c
    ww_archive.c
    ww_pcx.c
    ww_font.c
    ww_display.c
    ww_input.c
    ww_timing.c
    ww_intro.c
    ww_voc.c
    ww_audio.c
    ww_music.c
    ww_persistence.c
    ww_original_config.c
    ww_profiles.c
    ww_checksum.c
    ww_fixed_math.c
    ww_physics.c
    ww_track.c
    ww_renderer.c
    ww_perspective.c
    ww_sprite.c
    ww_render_queue.c
    ww_racer.c
    ww_world_object.c
    ww_dynamic_object.c
    ww_weapon.c
    ww_duck.c
    ww_water.c
    ww_hud.c
    ww_lap.c
    ww_finish.c
    ww_minimap.c
    ww_victory.c
    ww_race.c
    ww_menu.c
    ww_ai.c
    ww_game.c
)

target_include_directories(ww95 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(ww95 PRIVATE
    SDL_MAIN_HANDLED _CRT_SECURE_NO_WARNINGS TIMIDITY_STATIC=1)
target_link_libraries(ww95 PRIVATE SDL2::SDL2-static timidity)

if(MSVC)
    target_compile_options(ww95 PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(ww95 PRIVATE _CRT_NONSTDC_NO_DEPRECATE)
endif()

set_target_properties(ww95 PROPERTIES
    OUTPUT_NAME "ww95"
    VS_DEBUGGER_WORKING_DIRECTORY "${WW_PROJECT_ROOT}"
)

foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config_upper)
    set_target_properties(ww95 PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${config_upper}" "${WW_RUNTIME_OUTPUT}"
    )
endforeach()
set_target_properties(ww95 PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${WW_RUNTIME_OUTPUT}")

# The requested canonical executable lives beside the original game data.
add_custom_command(TARGET ww95 POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${WW_RUNTIME_OUTPUT}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:ww95>" "${WW_PROJECT_ROOT}/ww95.exe"
    VERBATIM
)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME ww95_assets
        COMMAND $<TARGET_FILE:ww95> --self-test
                --data "${WW_PROJECT_ROOT}/WACKY.DAT")
    set_tests_properties(ww95_assets PROPERTIES
        WORKING_DIRECTORY "${WW_PROJECT_ROOT}")

    add_test(NAME ww95_sdl_smoke
        COMMAND $<TARGET_FILE:ww95> --smoke-test
                --data "${WW_PROJECT_ROOT}/WACKY.DAT")
    set_tests_properties(ww95_sdl_smoke PROPERTIES
        WORKING_DIRECTORY "${WW_PROJECT_ROOT}"
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

    add_test(NAME ww95_race_smoke
        COMMAND $<TARGET_FILE:ww95> --race-smoke 30
                --data "${WW_PROJECT_ROOT}/WACKY.DAT")
    set_tests_properties(ww95_race_smoke PROPERTIES
        WORKING_DIRECTORY "${WW_PROJECT_ROOT}"
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

    add_test(NAME ww95_duck_smoke
        COMMAND $<TARGET_FILE:ww95> --duck-smoke 6
                --data "${WW_PROJECT_ROOT}/WACKY.DAT")
    set_tests_properties(ww95_duck_smoke PROPERTIES
        WORKING_DIRECTORY "${WW_PROJECT_ROOT}"
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
endif()
