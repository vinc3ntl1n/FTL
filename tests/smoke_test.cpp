#include <gtest/gtest.h>

#include "vincentftl/version.hpp"

// Proves the toolchain works end to end: the library builds, links into a test
// binary, and GoogleTest runs it.
TEST(Smoke, LibraryLinksIntoTests) { EXPECT_FALSE(vincentftl::version().empty()); }
