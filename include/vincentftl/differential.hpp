#pragma once


#include <cstdint>
#include <optional>
#include <string>


#include "vincentftl/ftl.hpp"


namespace vincentftl {


// Runs `operations` random writes and reads on both FTLs and compares every answer.
// The same `seed` always produces the same run, so a failure can be repeated.
// Returns nothing if the two always agreed, or a sentence describing the first disagreement.
std::optional<std::string> run_differential(Ftl& subject, Ftl& reference, std::uint32_t seed,
                                            std::uint32_t operations);


}  // namespace vincentftl
