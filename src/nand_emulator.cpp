#include "vincentftl/nand_emulator.hpp"

#include <stdexcept>

namespace vincentftl {

NandEmulator::NandEmulator(std::uint32_t block_count,
                           const std::vector<BlockNumber>& factory_bad_blocks) {
  if (block_count == 0 || block_count > kBlockCount) {
    throw std::invalid_argument("NandEmulator: block_count must be from 1 to 1024");
  }
  blocks_.resize(block_count);
  pages_.resize(std::size_t{block_count} * kPagesPerBlock);

  for (BlockNumber bad : factory_bad_blocks) {
    if (!in_range(bad)) {
      throw std::invalid_argument("NandEmulator: factory bad block is past the end of the device");
    }
    if (bad.value == 0) {
      throw std::invalid_argument("NandEmulator: block 0 is always good (datasheet section 10.1)");
    }
    blocks_[bad.value].factory_bad = true;

    // The factory marks a bad block with a non-0xFF byte at data byte 0 and spare byte 0 of its
    // first page (datasheet section 10.2).
    auto marked = std::make_unique<StoredPage>();
    marked->data.fill(kErasedByte);
    marked->spare.fill(kErasedByte);
    marked->data[0] = 0x00;
    marked->spare[0] = 0x00;
    pages_[PhysicalPage::in_block(bad, 0).value] = std::move(marked);
  }
}

std::uint32_t NandEmulator::block_count() const {
  return static_cast<std::uint32_t>(blocks_.size());
}

Status NandEmulator::read_page(PhysicalPage page, PageData& data, PageSpare& spare) {
  if (!in_range(page)) return Status::kBadAddress;

  const StoredPage* stored = pages_[page.value].get();
  if (stored == nullptr) {
    data.fill(kErasedByte);
    spare.fill(kErasedByte);
  } else {
    data = stored->data;
    spare = stored->spare;
  }
  return Status::kOk;
}

Status NandEmulator::read_spare(PhysicalPage page, PageSpare& spare) {
  if (!in_range(page)) return Status::kBadAddress;

  const StoredPage* stored = pages_[page.value].get();
  if (stored == nullptr) {
    spare.fill(kErasedByte);
  } else {
    spare = stored->spare;
  }
  return Status::kOk;
}

Status NandEmulator::program_page(PhysicalPage page, const PageData& data, const PageSpare& spare) {
  if (!in_range(page)) return Status::kBadAddress;

  BlockState& block = blocks_[page.block().value];
  if (block.factory_bad) return Status::kProgramFailed;
  if (pages_[page.value] != nullptr) return Status::kNotErased;
  if (page.page_in_block() < block.next_page) return Status::kOutOfOrder;
  for (std::size_t i = 0; i < kPageSpareBytes; ++i) {
    if (is_reserved_spare_byte(i) && spare[i] != kErasedByte) return Status::kReservedSpareByte;
  }

  pages_[page.value] = std::make_unique<StoredPage>(StoredPage{data, spare});
  block.next_page = page.page_in_block() + 1;
  ++pages_programmed_;
  return Status::kOk;
}

Status NandEmulator::erase_block(BlockNumber block_number) {
  if (!in_range(block_number)) return Status::kBadAddress;

  BlockState& block = blocks_[block_number.value];
  if (block.factory_bad) return Status::kEraseFailed;

  for (std::uint32_t page = 0; page < kPagesPerBlock; ++page) {
    pages_[PhysicalPage::in_block(block_number, page).value].reset();  // back to erased
  }
  block.next_page = 0;
  ++block.erase_count;
  ++blocks_erased_;
  return Status::kOk;
}

bool NandEmulator::is_factory_bad(BlockNumber block) {
  return !in_range(block) || blocks_[block.value].factory_bad;
}

std::uint32_t NandEmulator::erase_count(BlockNumber block) const {
  return blocks_.at(block.value).erase_count;
}

bool NandEmulator::in_range(PhysicalPage page) const { return page.value < pages_.size(); }

bool NandEmulator::in_range(BlockNumber block) const { return block.value < blocks_.size(); }

}  // namespace vincentftl
