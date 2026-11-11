#!/usr/bin/env python3
"""First contact: checks that the W25N01GV answers, by reading its ID and status registers.

Run it on the Pi, from the repo folder:

    python3 tools/pi/chip_id.py

It only reads. It never programs or erases anything. See docs/hardware-setup.md.
"""

import sys

import w25n_spi as chip

# Power-up values (datasheet 8.2.4): every block write-protected, ECC on, and buffer read mode
# (the "G" at the end of W25N01GVZEIG). The status register is 0 once the chip is idle.
EXPECTED = [
    (chip.PROTECTION_REG, "Protection", 0x7C, "every block write-protected, as at every power-up"),
    (chip.CONFIG_REG, "Configuration", 0x18, "ECC on, buffer read mode"),
    (chip.STATUS_REG, "Status", 0x00, "idle, no failed program or erase"),
]

HELP = """
The chip didn't answer correctly. Unplug the Pi's power before touching any wire, then check:

  FF FF FF    Nothing is answering. Check chip pin 2 (DO) goes to Pi pin 21, chip pin 1 (/CS)
              to Pi pin 24, power (chip pin 8 to 3.3 V, chip pin 4 to GND), and that the chip
              sits in the socket with its dot at pin 1.
  00 00 00    Same checks, plus chip pin 7 (/HOLD) to 3.3 V.
  EF ...      A Winbond chip, but not a W25N01GV. Try another chip.
  Anything    A loose or swapped wire. Re-seat every wire, check each one against the table in
  else        docs/hardware-setup.md, and keep the wires short.

Still stuck after 30 minutes: stop and ask in Discord. A logic analyzer comes next."""


def main():
    spi = chip.open_chip()
    chip.reset(spi)
    chip_id = chip.read_jedec_id(spi)
    print(f"Chip ID:  {chip.hexes(chip_id)}   (want {chip.hexes(chip.JEDEC_ID)})")
    if chip_id != chip.JEDEC_ID:
        print(HELP)
        return 1

    chip.wait_ready(spi)
    surprises = 0
    for address, name, want, meaning in EXPECTED:
        value = chip.read_register(spi, address)
        verdict = "as expected" if value == want else f"EXPECTED {want:02X}"
        print(f"{name + ':':15} {value:02X}   {verdict}  ({meaning})")
        surprises += value != want

    print()
    if surprises:
        print("The chip answers, but a register isn't at its power-up value. The chip may have")
        print("been configured by a previous owner. Note which chip this is and tell the captain.")
    else:
        print("The chip answers. Next: python3 tools/pi/scan_bad_blocks.py chip-N")
    return 0


if __name__ == "__main__":
    sys.exit(main())
