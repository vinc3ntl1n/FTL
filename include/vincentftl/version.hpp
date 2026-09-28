#pragma once

#include <string_view>

namespace vincentftl {

// The project version from CMakeLists.txt. Exists so the library has one real
// symbol for the smoke test to link against before the emulator lands.
std::string_view version();

}  // namespace vincentftl
