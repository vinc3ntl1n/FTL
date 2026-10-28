#pragma once


#include <cstdint>
#include <optional>
#include <vector>


#include "vincentftl/block_manager.hpp"
#include "vincentftl/ftl.hpp"
#include "vincentftl/geometry.hpp"
#include "vincentftl/nand_device.hpp"


namespace vincentftl {


// Good blocks the FTL keeps back from the user, so garbage collection always has somewhere to copy
// live pages to.
inline constexpr std::uint32_t kReserveBlocks = 3;


// The FTL: remembers where the newest copy of every logical page lives on the chip.
class PageFtl final : public Ftl {
 public:
  explicit PageFtl(NandDevice& device);


  std::uint32_t capacity() const override;
  FtlStatus write(LogicalPage page, const PageData& data) override;
  FtlStatus read(LogicalPage page, PageData& data) override;
  FtlStatus mount() override;


 private:
  // Runs garbage collection until at least two blocks are free.
  FtlStatus make_room();


  NandDevice& device_;
  BlockManager blocks_;
  std::uint32_t capacity_ = 0;
  // The mapping table: one entry per logical page. Empty means "never written".
  std::vector<std::optional<PhysicalPage>> map_;
  // The reverse of the mapping table: one entry per physical page, saying which logical page's
  // data it holds. Only meaningful while the physical page is live.
  std::vector<LogicalPage> owner_;
};


}  // namespace vincentftl
