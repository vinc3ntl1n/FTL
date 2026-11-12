#!/usr/bin/env python3
"""Finds the factory bad blocks on one W25N01GV and saves the list.

Run it once per chip, before anything programs or erases that chip:

    python3 tools/pi/scan_bad_blocks.py chip-1

The factory marks a bad block with a non-FFh byte at data byte 0 or spare byte 0 of the block's
first page (datasheet 10.2). Erasing a bad block wipes that mark for good, so this has to run
first. It only reads. See docs/hardware-setup.md.
"""

import sys

import w25n_spi as chip

MAX_FACTORY_BAD = 20  # at least 1,004 of the 1,024 blocks are good (datasheet 10.1)
ECC_OFF = 0x08  # configuration register with ECC-E = 0 and BUF = 1 (7.2)


def scan(spi):
    bad = []
    for block in range(chip.BLOCKS):
        chip.load_page(spi, block * chip.PAGES_PER_BLOCK)
        data0 = chip.read_buffer(spi, 0, 1)[0]
        spare0 = chip.read_buffer(spi, chip.DATA_BYTES, 1)[0]
        if data0 != 0xFF or spare0 != 0xFF:
            bad.append(block)
            print(f"  block {block:4}: bad (data byte 0 = {data0:02X}, spare byte 0 = {spare0:02X})")
    return bad


def main():
    if len(sys.argv) != 2:
        print("Usage: python3 tools/pi/scan_bad_blocks.py <label>   for example: chip-1")
        return 2
    label = sys.argv[1]

    spi = chip.open_chip()
    chip.reset(spi)
    if chip.read_jedec_id(spi) != chip.JEDEC_ID:
        print("The chip doesn't answer. Run python3 tools/pi/chip_id.py first.")
        return 1
    chip.wait_ready(spi)

    # ECC off while scanning, so the marker bytes come back exactly as stored.
    chip.write_register(spi, chip.CONFIG_REG, ECC_OFF)
    if chip.read_register(spi, chip.CONFIG_REG) != ECC_OFF:
        print("Couldn't turn ECC off. Nothing was scanned.")
        return 1
    print(f"Scanning {chip.BLOCKS} blocks of {label}...")
    try:
        bad = scan(spi)
    finally:
        chip.reset(spi)  # turns ECC back on

    path = f"bad-blocks-{label}.txt"
    with open(path, "w") as out:
        out.write(f"# Factory bad blocks of {label}, one block number per line\n")
        out.writelines(f"{block}\n" for block in bad)

    print(f"\n{len(bad)} bad block(s) out of {chip.BLOCKS}. Saved to {path}")
    if 0 in bad:
        print("WARNING: block 0 is always good on a new chip, so this chip has been used before.")
    if len(bad) > MAX_FACTORY_BAD:
        print(f"WARNING: a new chip has at most {MAX_FACTORY_BAD} bad blocks. This one has")
        print("probably been used before, so these are old data, not factory marks.")
    if 0 in bad or len(bad) > MAX_FACTORY_BAD:
        print("Don't use this chip for the FTL yet. Tell the captain.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
