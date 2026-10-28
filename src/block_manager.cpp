#include "vincentftl/block_manager.hpp"

namespace vincentftl {

using std::nullopt;
using std::optional;

BlockManager::BlockManager(NandDevice& nand) {
  blocks_.resize(nand.block_count());
  page_valid_.assign(blocks_.size() * kPagesPerBlock, false);

  for (unsigned i = 0; i < blocks_.size(); ++i) {
    if (nand.is_factory_bad(BlockNumber{i})) {
      blocks_[i].state = BlockState::kBad;
    } else {
      ++free_blocks_;
    }
  }
}

optional<PhysicalPage> BlockManager::allocate_page() {
  if (!open_block_) {
    for (unsigned i = 0; i < blocks_.size(); ++i) {
      if (blocks_[i].state == BlockState::kFree) {
        blocks_[i].state = BlockState::kOpen;
        open_block_ = BlockNumber{i};
        next_page_in_open_block_ = 0;
        --free_blocks_;
        break;
      }
    }
    if (!open_block_) return nullopt;
  }

  const unsigned block_number = open_block_->value;
  const PhysicalPage page = PhysicalPage::in_block(*open_block_, next_page_in_open_block_);
  ++next_page_in_open_block_;
  page_valid_[page.value] = true;
  ++blocks_[block_number].valid_pages;

  if (next_page_in_open_block_ == kPagesPerBlock) {
    blocks_[block_number].state = BlockState::kFull;
    open_block_.reset();
  }

  return page;
}

void BlockManager::mark_invalid(PhysicalPage page) {
  if (!is_valid(page)) return;
  page_valid_[page.value] = false;
  --blocks_[page.block().value].valid_pages;
}

bool BlockManager::is_valid(PhysicalPage page) const {
  return page.value < page_valid_.size() && page_valid_[page.value];
}

unsigned BlockManager::valid_pages(BlockNumber number) const {
  return blocks_[number.value].valid_pages;
}

unsigned BlockManager::free_block_count() const { return free_blocks_; }

std::optional<BlockNumber> BlockManager::pick_victim() const {
  std::optional<BlockNumber> victim=std::nullopt;
  unsigned fewest_live_pages = kPagesPerBlock;

  for (unsigned i = 0; i < blocks_.size(); ++i) {
    const Block& block = blocks_[i];
    if (block.state == BlockState::kFull && block.valid_pages < fewest_live_pages) {
      victim = BlockNumber{i};
      fewest_live_pages = block.valid_pages;
    }
  }

  return victim;

}  // namespace vincentftl