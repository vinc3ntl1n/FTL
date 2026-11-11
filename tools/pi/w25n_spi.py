"""Small helpers for talking to a W25N01GV over the Raspberry Pi's SPI0 from Python.

For bring-up checks only. The real driver is the C++ one. Section numbers refer to the
W25N01GV datasheet, Revision O (linked from docs/nand-rules.md).
"""

import time

import spidev

BLOCKS = 1024
PAGES_PER_BLOCK = 64
DATA_BYTES = 2048

JEDEC_ID = [0xEF, 0xAA, 0x21]  # Winbond, W25N01GV (8.2.2)

# The three status registers and their addresses (7.1, 7.2, 7.3).
PROTECTION_REG = 0xA0
CONFIG_REG = 0xB0
STATUS_REG = 0xC0

BUSY = 0x01  # bit 0 of STATUS_REG (7.3.6)


def open_chip(speed_hz=1_000_000):
    """SPI0 with chip select CE0 (header pin 24). 1 MHz is slow and forgiving of long wires."""
    spi = spidev.SpiDev()
    spi.open(0, 0)
    spi.mode = 0
    spi.max_speed_hz = speed_hz
    return spi


def read_register(spi, address):
    """Read Status Register (0Fh, 8.2.3): opcode, register address, then the value comes back."""
    return spi.xfer2([0x0F, address, 0x00])[2]


def write_register(spi, address, value):
    """Write Status Register (1Fh, 8.2.4). Volatile: a reset or power cycle undoes it."""
    spi.xfer2([0x1F, address, value])


def wait_ready(spi, timeout_s=0.1):
    """Waits until the chip finishes what it is doing. The slowest thing, an erase, takes 10 ms."""
    deadline = time.monotonic() + timeout_s
    while read_register(spi, STATUS_REG) & BUSY:
        if time.monotonic() > deadline:
            raise TimeoutError("the chip stayed busy; check the wiring with chip_id.py")


def reset(spi):
    """Device Reset (FFh, 8.2.1). Puts every register back to its power-up value."""
    spi.xfer2([0xFF])
    time.sleep(0.001)  # a reset takes at most 500 us


def read_jedec_id(spi):
    """Read JEDEC ID (9Fh, 8.2.2): opcode, 8 dummy clocks, then 3 ID bytes."""
    return spi.xfer2([0x9F, 0x00, 0x00, 0x00, 0x00])[2:5]


def load_page(spi, page):
    """Page Data Read (13h, 8.2.14): copies one page into the chip's 2,112-byte buffer."""
    spi.xfer2([0x13, 0x00, (page >> 8) & 0xFF, page & 0xFF])
    wait_ready(spi)


def read_buffer(spi, column, count):
    """Read Data (03h, 8.2.15, buffer mode): 2-byte column address, 1 dummy byte, then data.

    Column 0 is the first data byte; column 2048 is the first spare byte.
    """
    reply = spi.xfer2([0x03, (column >> 8) & 0xFF, column & 0xFF, 0x00] + [0x00] * count)
    return reply[4:]


def hexes(values):
    return " ".join(f"{v:02X}" for v in values)
