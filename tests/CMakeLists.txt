# GoogleTest is downloaded at configure time so nobody installs it by hand.
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
  URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Every tests/*_test.cpp is picked up automatically, so adding a test file
# never means editing this list (and six PRs never conflict on it).
file(GLOB test_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp)
add_executable(vincentftl_tests ${test_sources})
target_link_libraries(vincentftl_tests PRIVATE vincentftl ftl_warnings GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(vincentftl_tests)
