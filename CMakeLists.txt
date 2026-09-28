cmake_minimum_required(VERSION 3.21)

project(VincentFTL VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(FTL_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(FTL_WERROR "Treat compiler warnings as errors (CI turns this on)" OFF)

# Sanitizers apply to everything, GoogleTest included, so they are set before
# it is fetched. -fno-sanitize-recover makes undefined behavior fail the test
# instead of printing a warning nobody reads.
if(FTL_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all
                      -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# Warnings apply to our code only, not to GoogleTest.
add_library(ftl_warnings INTERFACE)
target_compile_options(ftl_warnings INTERFACE -Wall -Wextra -Wpedantic)
if(FTL_WERROR)
  target_compile_options(ftl_warnings INTERFACE -Werror)
endif()

# Every src/*.cpp is picked up automatically, like tests/, so adding a file
# never means editing this list.
file(GLOB library_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(vincentftl ${library_sources})
target_include_directories(vincentftl PUBLIC include)
target_compile_definitions(vincentftl PRIVATE VINCENTFTL_VERSION="${PROJECT_VERSION}")
target_link_libraries(vincentftl PRIVATE ftl_warnings)

enable_testing()
add_subdirectory(tests)
