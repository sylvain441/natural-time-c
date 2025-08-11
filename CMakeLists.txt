cmake_minimum_required(VERSION 3.20)

project(natural_time
  VERSION 0.1.0
  DESCRIPTION "Portable C core for Natural Time (translation from natural-time-js)"
  LANGUAGES C)

# 🔴 Build options
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Warnings (💡 similar to strict TypeScript)
if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Library
add_library(natural_time
  src/natural_time.c
  vendor/astronomy_c/astronomy.c
)
target_include_directories(natural_time PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Vendor: Astronomy Engine (💡 C submodule) — headers live in vendor/astronomy/source/c
target_include_directories(natural_time PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/astronomy_c)

# Suppress vendor warnings so -Werror does not fail the build (🔴 third-party code)
set_source_files_properties(
  vendor/astronomy_c/astronomy.c
  PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-parameter;-Wno-extra-semi;-Wno-missing-field-initializers"
)

# Version define
target_compile_definitions(natural_time PUBLIC NTC_VERSION="${PROJECT_VERSION}")

# Tests
include(CTest)
if(BUILD_TESTING)
  add_executable(test_smoke tests/unit/test_smoke.c)
  target_link_libraries(test_smoke PRIVATE natural_time)
  add_test(NAME smoke COMMAND test_smoke)
endif()

# Packaging hooks can be added later for XCFramework/AAR builds


