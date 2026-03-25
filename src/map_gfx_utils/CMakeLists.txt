# Compile the map-gfx-utils static library
cmake_minimum_required (VERSION 3.20)

cmake_policy(SET CMP0091 NEW)
if ( NOT DEFINED USES_CMAKE_PRESETS )
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  if (WIN32)
    set(VCPKG_TARGET_TRIPLET x64-windows-static)
  endif ()
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
  set(STATIC_SUFFIX _static)
endif ()

set(This MapGfxUtils)

project(${This})

set(MapGfxUtilsLibPath ${CMAKE_CURRENT_SOURCE_DIR})
cmake_path(GET MapGfxUtilsLibPath PARENT_PATH SrcPath)
cmake_path(GET SrcPath PARENT_PATH ChkdraftRootPath)
include_directories(
    ${ChkdraftRootPath}/src
    ${ChkdraftRootPath}/src/mapping_core/opengl
)

if ( PROJECT_IS_TOP_LEVEL )
  set(CMAKE_BUILD_PARALLEL_LEVEL)
  set(CMAKE_C_STANDARD 99)
  set(CMAKE_CXX_STANDARD 20)
  if ( "${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC" )
    add_compile_options(/permissive- ${ASAN})
  endif ()
  add_compile_definitions(_UNICODE UNICODE NOMINMAX _CRT_SECURE_NO_WARNINGS)
  add_subdirectory(${ChkdraftRootPath}/src/cross_cut ${CMAKE_CURRENT_BINARY_DIR}/cross_cut_lib)
  add_subdirectory(${ChkdraftRootPath}/src/mapping_core ${CMAKE_CURRENT_BINARY_DIR}/mapping_core_lib)
endif()

find_package(Freetype REQUIRED)
get_target_property(FREETYPE_INCLUDE_DIRS freetype INTERFACE_INCLUDE_DIRECTORIES)
include_directories(${FREETYPE_INCLUDE_DIRS})

find_package(harfbuzz CONFIG REQUIRED)
get_target_property(HARFBUZZ_INCLUDE_DIRS harfbuzz::harfbuzz INTERFACE_INCLUDE_DIRECTORIES)
include_directories(${HARFBUZZ_INCLUDE_DIRS})

find_package(WebP CONFIG REQUIRED)

find_package(ICU COMPONENTS uc i18n REQUIRED)

find_package(glad CONFIG REQUIRED)
get_target_property(GLAD_INCLUDE_DIRS glad::glad INTERFACE_INCLUDE_DIRECTORIES)
include_directories(${GLAD_INCLUDE_DIRS})

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

include(FetchContent)
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG ae721c5 # 2024-02-13
)
FetchContent_MakeAvailable(stb)
include_directories(${stb_SOURCE_DIR}/include)

include(FetchContent)
FetchContent_Declare(
  rarecpp
  GIT_REPOSITORY https://github.com/TheNitesWhoSay/RareCpp.git
  GIT_TAG 67de40e # release-2.6.2
)
FetchContent_MakeAvailable(rarecpp)
include_directories(${rarecpp_SOURCE_DIR}/include)

set(Headers
  gfx_util.h
  map_timings.h
  renderer.h
  sc_map.h
  screenshot.h
  ../mapping_core/opengl/glfw/context.h
  ../mapping_core/opengl/glfw/window.h
  ../mapping_core/opengl/glad/utils.h
  ../mapping_core/opengl/gl/base.h
  ../mapping_core/opengl/gl/camera.h
  ../mapping_core/opengl/gl/context_semaphore.h
  ../mapping_core/opengl/gl/font.h
  ../mapping_core/opengl/gl/fps.h
  ../mapping_core/opengl/gl/palette.h
  ../mapping_core/opengl/gl/program.h
  ../mapping_core/opengl/gl/shader.h
  ../mapping_core/opengl/gl/texture.h
  ../mapping_core/opengl/gl/uniform.h
  ../mapping_core/opengl/gl/unique_resource.h
  ../mapping_core/opengl/gl/utils.h
  ../mapping_core/opengl/gl/vertices.h
  ../mapping_core/render/color_cycler.h
  ../mapping_core/render/game_clock.h
  ../mapping_core/render/memory_tiered_tasks.h
  ../mapping_core/render/map_actor.h
  ../mapping_core/render/map_animations.h
  ../mapping_core/render/map_image.h
  ../mapping_core/render/sc_gl_graphics.h
)
set(Sources
  gfx_util.cpp
  lato.ttf.cpp
  main.cpp
  renderer.cpp
  sc_map.cpp
  screenshot.cpp
  ../mapping_core/opengl/stb/stb.cpp
  ../mapping_core/render/map_actor.cpp
  ../mapping_core/render/map_animations.cpp
  ../mapping_core/render/map_image.cpp
  ../mapping_core/render/sc_gl_graphics.cpp
)

add_compile_definitions(_UNICODE UNICODE CASCLIB_UNICODE __STORMLIB_NO_STATIC_LINK__)
add_library(${This}Lib STATIC ${Sources} ${Headers})

add_executable(${This} ${Sources} ${Headers})

find_package(OpenGL REQUIRED)
if (WIN32)
target_link_libraries(${This} PRIVATE CrossCut MappingCore glad::glad glfw glm::glm Freetype::Freetype harfbuzz::harfbuzz harfbuzz::harfbuzz-subset ICU::uc ICU::i18n WebP::webp WebP::webpdecoder WebP::webpdemux ${OPENGL_LIBRARIES} Comctl32.lib Winmm.lib Msimg32.lib)
else ()
target_link_libraries(${This} PRIVATE CrossCut MappingCore glad::glad glfw glm::glm Freetype::Freetype harfbuzz::harfbuzz harfbuzz::harfbuzz-subset ICU::uc ICU::i18n WebP::webp WebP::webpdecoder WebP::webpdemux ${OPENGL_LIBRARIES})
endif ()

# Export build info for Rust build script
if (PROJECT_IS_TOP_LEVEL)
  get_directory_property(_ALL_INCLUDE_DIRS INCLUDE_DIRECTORIES)
  # stb headers are at repo root, not in /include subdir
  list(APPEND _ALL_INCLUDE_DIRS "${stb_SOURCE_DIR}")

  set(_ALL_LIB_DIRS
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/cross_cut_lib"
    "${CMAKE_CURRENT_BINARY_DIR}/mapping_core_lib"
  )

  # cmake-built static libraries
  set(_ALL_STATIC_LIBS "MapGfxUtilsLib" "MappingCore" "CrossCut")

  # Collect vcpkg lib dirs and scan for static libraries
  file(GLOB _VCPKG_LIB_DIRS "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/*/lib")
  list(APPEND _ALL_LIB_DIRS ${_VCPKG_LIB_DIRS})

  if (DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
    file(GLOB _VCPKG_CLASSIC_LIB_DIRS "$ENV{VCPKG_ROOT}/installed/*/lib")
    list(APPEND _ALL_LIB_DIRS ${_VCPKG_CLASSIC_LIB_DIRS})
  endif()

  foreach(_dir ${_VCPKG_LIB_DIRS} ${_VCPKG_CLASSIC_LIB_DIRS})
    file(GLOB _LIBS "${_dir}/lib*.a")
    foreach(_lib ${_LIBS})
      get_filename_component(_name "${_lib}" NAME_WE)
      string(REGEX REPLACE "^lib" "" _name "${_name}")
      list(APPEND _ALL_STATIC_LIBS "${_name}")
    endforeach()
  endforeach()
  list(REMOVE_DUPLICATES _ALL_STATIC_LIBS)

  # System libraries
  set(_ALL_SYSTEM_LIBS "stdc++" "GL" "X11" "pthread" "dl" "m")

  set(_INFO "")
  foreach(_dir ${_ALL_INCLUDE_DIRS})
    string(APPEND _INFO "INCLUDE_DIR=${_dir}\n")
  endforeach()
  foreach(_dir ${_ALL_LIB_DIRS})
    string(APPEND _INFO "LIB_DIR=${_dir}\n")
  endforeach()
  foreach(_lib ${_ALL_STATIC_LIBS})
    string(APPEND _INFO "STATIC_LIB=${_lib}\n")
  endforeach()
  foreach(_lib ${_ALL_SYSTEM_LIBS})
    string(APPEND _INFO "SYSTEM_LIB=${_lib}\n")
  endforeach()

  file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/rust_build_info.txt" CONTENT "${_INFO}")
endif()