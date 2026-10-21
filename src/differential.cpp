
#include "vincentftl/differential.hpp"

#include <random>
#include <sstream>

namespace vincentftl {

std::optional<std::string> run_differential(Ftl& subject, Ftl& reference, std::uint32_t seed,
                                            std::uint32_t operations) {
  if (subject.capacity() != reference.capacity()) {
    std::ostringstream out;
    out << "Ftl capacity mismatch" << std::endl;
    return out.str();
  }

  std::mt19937 rng(seed);

  for (std::uint32_t i = 0; i < operations; i++) {
    bool current_op = ((rng() % 2) == 0);
    const LogicalPage page{rng() % subject.capacity()};

    if (current_op == true) {
      PageData data;
      for (auto& byte : data) {
        byte = static_cast<std::uint8_t>(rng());
      }

      const FtlStatus subject_status = subject.write(page, data);
      const FtlStatus ref_status = reference.write(page, data);

      if (subject_status != ref_status) {
        std::ostringstream out;
        out << "Ftl write status mismatch at operation number " << i << " and logical page number "
            << page.value << std::endl
            << "Subject Status: " << subject_status << std::endl
            << "Reference Status: " << ref_status << std::endl;
        return out.str();
      }
    } else {
      PageData subject_data;
      PageData ref_data;

      const FtlStatus subject_status = subject.read(page, subject_data);
      const FtlStatus ref_status = reference.read(page, ref_data);

      if (subject_status != ref_status) {
        std::ostringstream out;
        out << "Ftl read status mismatch at operation number " << i << " and logical page number "
            << page.value << std::endl
            << "Subject Status: " << subject_status << std::endl
            << "Reference Status: " << ref_status << std::endl;
        return out.str();
      } else if (subject_status == FtlStatus::kOk && subject_data != ref_data) {
        std::ostringstream out;
        out << "Ftl read data mismatch at operation number " << i << " and logical page number "
            << page.value << std::endl;
        return out.str();
      }
    }
  }
  return std::nullopt;
};
}  // namespace vincentftl
