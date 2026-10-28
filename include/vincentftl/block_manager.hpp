#pragma once


#include <cstdint>
#include <optional>
#include <vector>


#include "geometry.hpp"
#include "nand_device.hpp"


namespace vincentftl {


class BlockManager {
 public:
  explicit BlockManager(NandDevice& device);


  // The next blank page to program, or nothing when no blank page is left.
  std::optional<PhysicalPage> allocate_page();


  // The data on this page has been replaced. The page is now garbage.
  void mark_invalid(PhysicalPage page);


  // True if this page was handed out and has not been marked invalid since.
  bool is_valid(PhysicalPage page) const;


  // How many pages in this block hold live data.
  std::uint32_t valid_pages(BlockNumber block) const;


  // How many good blocks have not been opened yet.
  std::uint32_t free_block_count() const;

  // The full block with the fewest live pages: the cheapest one for garbage collection to clean.
  // Nothing if no block is full, or if every full block is completely live, since cleaning one of
  // those would free nothing. Ties go to the lowest-numbered block.
  std::optional<BlockNumber> pick_victim() const;


  // The chip has erased this block. Its pages are blank, and it goes back to the free blocks.
  // Does nothing unless the block is full and has no live pages left.
  void block_erased(BlockNumber block);


  // How many times this block has been erased since the manager was created.
  std::uint32_t erase_count(BlockNumber block) const;



 private:
  enum class BlockState { kBad, kFree, kOpen, kFull };


  struct Block {
    BlockState state = BlockState::kFree;
    std::uint32_t valid_pages = 0;
    std::uint32_t erase_count = 0;
  };


  // One entry per block.
  std::vector<Block> blocks_;
  // One entry per page: is it live?
  std::vector<bool> page_valid_;
  // The block being filled right now.
  std::optional<BlockNumber> open_block_;
  std::uint32_t next_page_in_open_block_ = 0;
  std::uint32_t free_blocks_ = 0;
};


}  // namespace vincentftl
