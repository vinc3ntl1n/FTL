# NAND rules

The rules of the Winbond W25N01GV that the emulator enforces, and the ones it
deliberately doesn't. Every rule cites the
[W25N01GV datasheet, Revision O](https://www.winbond.com/resource-files/W25N01GV%20Rev%20O%20092619.pdf)
(Winbond's manual for the chip). The emulator enforces exactly this list. If
the emulator and this file disagree, one of them has a bug.

## Geometry

| Unit | Size |
|------|------|
| Page | 2048 bytes of data + 64 bytes of spare area = 2112 bytes |
| Block | 64 pages |
| Chip | 1024 blocks = 65,536 pages = 128 MiB of data |

A page is the smallest thing you can read or program. A block is the smallest
thing you can erase. The constants live in `include/vincentftl/geometry.hpp`.

## Rules the emulator enforces

Each rule lists the `Status` the emulator returns when it's broken.

**1. Erased means every byte is `0xFF`, and erasing a block is the only way to
get back there.** Erase works on a whole block at a time. (§8.2.10)

**2. Program a page once, then erase its block before programming it again.**
Returns `kNotErased`.
The chip itself allows up to 4 programs of the same page between erases
(§8.2.13, "NoP" in §9.6), each one only able to turn 1 bits into 0 bits. We
allow one, on purpose:

- Our FTL writes a page's data and spare area in a single program, so a second
  program of the same page is always an FTL bug.
- With ECC on (rule 4), the chip writes its check bits on every program. A
  second program would have to change those bits, and it can't turn 0s back
  into 1s.

Because every program lands on an all-`0xFF` page, the chip's "program can only
turn 1s into 0s" rule is automatically satisfied.

**3. Within a block, program pages in increasing order.** Returns `kOutOfOrder`.
"The pages within the block have to be programmed sequentially from the lower
order page address to the higher order page address within the block." (§8.2.13)
Skipping pages is allowed: after page 5 you may program page 9, but never
page 4.

**4. Don't write into the spare bytes the chip reserves.** Returns
`kReservedSpareByte` if a program puts anything other than `0xFF` there.
ECC (error-correcting code) is on by default (§7.2.4). With ECC on, the chip
stores check bits in part of every page's spare area so it can repair a flipped
bit when the page is read. Anything we store in those bytes is overwritten
(§8.2.11). See the map below.

**5. Factory bad blocks can't be programmed or erased.** Programming one returns
`kProgramFailed`, and erasing one returns `kEraseFailed`, the same results the
chip would report through its failure bits (§7.3.3, §7.3.4). `is_factory_bad()`
returns true for them. Reading one works, and page 0 shows the factory's
marker: a non-`0xFF` byte (the emulator uses `0x00`) at data byte 0 and at
spare byte 0. (§8.2.7, §10.2)

**6. Addresses past the end of the chip return `kBadAddress`.** The emulator
never crashes on a bad address.

## Spare area map (ECC on)

The 64 spare bytes are four groups of 16, one per 512-byte quarter of the page's
data. Every group has the same layout (Figure 2 in the datasheet):

| Bytes in the group | Owner | Notes |
|--------------------|-------|-------|
| 0–1 | Chip | Bad-block marker in group 0. The datasheet doesn't say what bytes 0–1 hold in groups 1–3, so we leave them alone too |
| 2–3 | **FTL** | Not covered by ECC. A flipped bit here isn't corrected |
| 4–7 | **FTL** | Covered by ECC. Put important metadata here |
| 8–15 | Chip | ECC check bits, overwritten on every program |

That gives the FTL 16 ECC-covered bytes per page (bytes 4–7 of each group) and
8 more that ECC doesn't cover. On a read, the chip-owned bytes come back as
whatever the chip put there. Don't rely on their contents.

## What the chip does that the emulator doesn't

- **Timing.** A program takes 250–700 µs and an erase 2–10 ms (§9.6). The
  emulator is instant.
- **Wear and bit errors.** On the chip, bits start flipping as a block wears,
  and programs and erases eventually fail. The emulator never flips a bit, and
  it only fails programs and erases on factory bad blocks.
- **Write protection at power-up.** The chip starts with every block
  write-protected (§7.1.1). The driver has to turn protection off before the
  first program or erase.
- **The bad-block scan is one-shot.** A chip has at most 20 factory bad blocks
  (§8.2.7), and block 0 is always good when it ships (§10.1). Scan for the markers
  once on a fresh chip, before any program or erase, and keep the list.
  Erasing a bad block wipes its marker for good, and once the FTL has written a
  block, data byte 0 of page 0 is just user data (§10.2).
