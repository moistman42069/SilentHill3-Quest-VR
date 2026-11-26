cmake_minimum_required(VERSION 3.20)
# SH3 Quest VR: C for sh3-nx's runtime and the lifted game, C++17 for the VR layer (src/vr, from our Ratchet/Jak ports).
project(sh3nx C CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB GEN CONFIGURE_DEPENDS src/recomp/gen/*.c)
file(GLOB RT CONFIGURE_DEPENDS src/runtime/*.c src/win32/*.c src/d3d8/*.c src/game/*.c)
# The headset (OpenXR on the Quest and PC VR, the scripted sim on a PC) and SH3's VR. vr_menu.cpp / vr_options.cpp are
# Ratchet's, kept for reference only.
set(VR src/vr/sh3vr.cpp src/vr/sh3vr_body.cpp src/vr/sh3vr_combat.cpp src/vr/sh3vr_ik.cpp src/vr/sh3vr_menu.cpp src/vr/sh3vr_parts.cpp src/vr/sh3vr_reload.cpp src/vr/sh3vr_sounds.cpp
       src/vr/xr_openxr.cpp src/vr/xr_sim.cpp)

if(ANDROID)
  # The Quest: libmain.so, loaded by SDL2's SDLActivity (android/), which calls SDL_main (rt_core.c).
  add_library(sh3 SHARED ${GEN} ${RT} ${VR})
  set_target_properties(sh3 PROPERTIES OUTPUT_NAME main)
else()
  add_executable(sh3 ${GEN} ${RT} ${VR})
endif()
target_include_directories(sh3 PRIVATE src/recomp/gen third_party/khronos/include)
# Lifted code relies on wrapping arithmetic and type punning through guest memory.
target_compile_options(sh3 PRIVATE -fwrapv -fno-strict-aliasing)
set_source_files_properties(${GEN} PROPERTIES COMPILE_OPTIONS "-w")
# The launcher's images are built into the binary (src/d3d8/launcher.c).
file(SHA1 ${CMAKE_SOURCE_DIR}/assets/splash.bmp SPLASH_SHA1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS assets/splash.bmp)
set_source_files_properties(src/d3d8/launcher.c PROPERTIES COMPILE_DEFINITIONS
                            "SRC_DIR=\"${CMAKE_SOURCE_DIR}\";SPLASH_SHA1=\"${SPLASH_SHA1}\""
                            OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/assets/splash.bmp;${CMAKE_SOURCE_DIR}/assets/logo.bmp;${CMAKE_SOURCE_DIR}/assets/font.bmp")

if(NINTENDO_SWITCH)
  # Built with the devkitPro toolchain (tools/build-switch.sh); OpenGL comes through EGL and mesa.
  target_include_directories(sh3 PRIVATE ${DEVKITPRO}/portlibs/switch/include)
  target_link_libraries(sh3 PRIVATE SDL2 EGL glapi drm_nouveau stdc++ pthread m)
  nx_generate_nacp(sh3.nacp NAME "Silent Hill 3" AUTHOR "sh3-nx" VERSION "0.06")
  nx_create_nro(sh3 OUTPUT sh3-nx.nro NACP sh3.nacp NOICON)
elseif(ANDROID)
  # SH3 Quest VR: NDK 27.2, arm64-v8a, android-32 (tools/build_quest.py). SDL2 from its source (SH3_SDL2_SRC: libSDL2.so,
  # which SDLActivity loads before libmain.so), the OpenXR loader from Khronos' AAR (SH3_OPENXR_DIR: its
  # prefab/modules/openxr_loader/libs/android.arm64-v8a/libopenxr_loader.so).
  set(SH3_SDL2_SRC "C:/SH3Quest/deps/SDL2-2.32.10" CACHE PATH "SDL2's source")
  set(SH3_OPENXR_DIR "" CACHE PATH "the OpenXR loader AAR, unpacked")
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  add_subdirectory(${SH3_SDL2_SRC} sdl2 EXCLUDE_FROM_ALL)
  add_library(openxr_loader SHARED IMPORTED)
  set_target_properties(openxr_loader PROPERTIES IMPORTED_LOCATION
                        ${SH3_OPENXR_DIR}/prefab/modules/openxr_loader/libs/android.arm64-v8a/libopenxr_loader.so)
  target_compile_definitions(sh3 PRIVATE SH3_QUEST=1)
  target_link_libraries(sh3 PRIVATE SDL2::SDL2 openxr_loader EGL GLESv3 android log m dl)
  # Crash reports unwind the host stack (host.c): keep frame pointers and the unwind tables.
  target_compile_options(sh3 PRIVATE -fno-omit-frame-pointer -funwind-tables)
  # 0.1.5 (owner: "COMPLETELY MAXIMIZE PERFORMANCE"): the game's calls between the lifted files (tens of thousands of
  # functions, 250 a file) are bound inside the library, straight to their targets instead of through the PLT. The
  # names stay exported, so a crash report's dladdr still names every frame (host.c). The registers' thread-locals
  # are hidden for the same reason (recomp_types.h, RECOMP_TLS).
  target_link_options(sh3 PRIVATE -Wl,-Bsymbolic-functions)
  # The bridges' table (rt.h BRIDGE_REG, section game_bridges) is found by __start_/__stop_game_bridges: lld's
  # --gc-sections with start-stop-gc (its default) dropped the section and left those undefined (session 15, the first
  # Android link), so the section is kept whole.
  target_link_options(sh3 PRIVATE -Wl,--build-id=sha1 -Wl,-z,nostart-stop-gc)
elseif(WIN32)
  # SH3 Quest VR: the PC test build (tools/build_pc.bat): LLVM clang with the MSVC libraries, SDL2's VC files in
  # SH3_SDL2_DIR (include/SDL2/*.h, lib/SDL2.lib, lib/SDL2.dll), POSIX shims in src/compat/win.
  set(SH3_SDL2_DIR "C:/SH3Quest/deps/sdl2" CACHE PATH "SDL2 headers and VC libraries")
  target_sources(sh3 PRIVATE src/compat/win/compat_win.c)
  target_include_directories(sh3 PRIVATE src/compat/win ${SH3_SDL2_DIR}/include)
  target_compile_definitions(sh3 PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS SDL_MAIN_HANDLED)
  set_property(SOURCE ${RT} src/compat/win/compat_win.c APPEND PROPERTY COMPILE_OPTIONS
               -include ${CMAKE_SOURCE_DIR}/src/compat/win/compat_force.h)
  target_link_libraries(sh3 PRIVATE ${SH3_SDL2_DIR}/lib/SDL2.lib dbghelp opengl32)
  target_link_options(sh3 PRIVATE -Wl,/STACK:16777216 -Wl,/DEBUG:FULL)
  add_custom_command(TARGET sh3 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                     ${SH3_SDL2_DIR}/lib/SDL2.dll $<TARGET_FILE_DIR:sh3>)
else()
  # GL is loaded at run time through SDL (src/d3d8/gl.c), so nothing links libGL.
  find_package(SDL2 REQUIRED)
  target_link_libraries(sh3 PRIVATE SDL2::SDL2 m pthread dl)
endif()
