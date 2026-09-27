cmake_minimum_required(VERSION 3.15)
project(CGJRE LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
option(CGJRE_HOST "Build portable host harness" OFF)

set(CGJRE_PORTABLE_SOURCES
  src/smoke.c
  src/archive/zip.c
  src/archive/manifest.c
)

if(CGJRE_HOST)
  include(CTest)
  add_executable(cgjre-host
    ${CGJRE_PORTABLE_SOURCES}
    src/platform/host/main.c
    src/platform/host/inspect.c
  )
  target_include_directories(cgjre-host PRIVATE include)
  target_compile_options(cgjre-host PRIVATE -Wall -Wextra -Werror)
  if(BUILD_TESTING)
    add_test(NAME smoke COMMAND cgjre-host --smoke)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME archive COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_archive.py $<TARGET_FILE:cgjre-host>)
  endif()
else()
  include(GenerateG3A)
  include(Fxconv)
  find_package(Gint 2.9 REQUIRED)
  if(NOT "${FXSDK_PLATFORM_LONG}" STREQUAL "fxCG50")
    message(FATAL_ERROR "CGJRE currently supports only fx-CG50 (fxsdk build-cg)")
  endif()
  add_executable(cgjre
    ${CGJRE_PORTABLE_SOURCES}
    src/platform/gint/main.c
  )
  target_include_directories(cgjre PRIVATE include)
  target_compile_options(cgjre PRIVATE -Wall -Wextra -Os)
  target_link_options(cgjre PRIVATE "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/cgjre.map")
  target_link_libraries(cgjre Gint::Gint)
  generate_g3a(TARGET cgjre OUTPUT "CGJRE.g3a"
    NAME "CGJRE"
    ICONS assets-cg/icon-uns.png assets-cg/icon-sel.png)
endif()
