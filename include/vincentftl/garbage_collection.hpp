#pragma once


#include <vector>


#include "vincentftl/block_manager.hpp"
#include "vincentftl/ftl.hpp"
#include "vincentftl/geometry.hpp"
#include "vincentftl/nand_device.hpp"


namespace vincentftl {


// One live page that garbage collection copied to a new place.
struct PageMove {
  PhysicalPage from;
  PhysicalPage to;
};


// Cleans one block so it can be used again. Copies every live page in `victim` to a blank page
// from `blocks`, marks the old copy as garbage, then erases `victim`. Adds one entry to `moves` for
// every page it copies, so the FTL can update its mapping table.
//
// It does not tell `blocks` that the victim is blank again. The caller does that with
// block_erased once this returns kOk.
//
// Returns kOk, kNoSpace if it ran out of blank pages to copy into, or kDeviceError if the chip
// reported a failure.
FtlStatus collect_block(NandDevice& device, BlockManager& blocks, BlockNumber victim,
                        std::vector<PageMove>& moves);


}  // namespace vincentftl
