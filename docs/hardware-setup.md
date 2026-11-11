# Hardware setup

How to set up the Raspberry Pi, wire a W25N01GV chip to it, and check that the
chip answers. About an hour the first time. Section numbers refer to the
[W25N01GV datasheet, Revision O](https://www.winbond.com/resource-files/W25N01GV%20Rev%20O%20092619.pdf).

## What you need

- Raspberry Pi 4, its USB-C power supply, and a microSD card (16 GB or more)
- A W25N01GV chip and the clamshell socket it sits in
- 8 female–female jumper wires
- A small breadboard (recommended; see step 3)
- One 0.1 µF ceramic capacitor (recommended)

## 1. Put an operating system on the Pi

On your laptop:

1. Install **Raspberry Pi Imager** from raspberrypi.com/software and open it.
2. Device: **Raspberry Pi 4**. Operating system: **Raspberry Pi OS Lite (64-bit)**
   (under "Raspberry Pi OS (other)"). Storage: the microSD card.
3. When it asks about settings, choose **Edit settings**:
   - Hostname: `vincentftl`
   - A username and password. Write them down
   - Wi-Fi: a network your laptop will also be on. Campus Wi-Fi that needs a
     login page or a university account often won't work; a phone hotspot
     always does
   - On the **Services** tab, turn on **SSH** with password login
4. Write the card, put it in the Pi, and plug the Pi in. Wait two minutes.

From your laptop, on the same Wi-Fi:

```sh
ssh yourname@vincentftl.local
```

(`yourname` is the username from step 3.) You're now typing commands on the
Pi. The steps below all run there.

## 2. Turn on SPI and get the code

SPI is the four-wire connection the chip talks over. It's off by default.

```sh
sudo raspi-config nonint do_spi 0
sudo reboot
```

Wait 30 seconds, `ssh` back in, then:

```sh
ls /dev/spidev0.*                    # should list /dev/spidev0.0 and /dev/spidev0.1
sudo apt update
sudo apt install -y git python3-spidev
git clone https://github.com/vinc3ntl1n/FTL.git
cd FTL
./setup.sh                           # about 10 minutes on a Pi 4
```

`setup.sh` ends with "All set" here too. That proves our C++ builds on the Pi.

## 3. Wire the chip

**Unplug the Pi's power before you touch a wire.** Plug it back in only after
the three checks at the end of this step.

**The chip.** Open the socket, drop the chip in with its **dot at the corner
marked pin 1**, and close it. The pins on the socket's board are numbered 1 to
8, the same as the chip's pins. Look for the numbers or a square pad at pin 1.

**The Pi.** Its 40 header pins are numbered 1 to 40: odd numbers on the row
toward the middle of the board, even numbers on the row at the edge. Pin 1 is
the one farthest from the USB ports. To check, run `pinout` on the Pi; it
draws the header for you.

| Chip pin | Name | What it does | Connect to |
|---------:|------|--------------|------------|
| 1 | /CS | "I'm talking to you" | Pi pin **24** |
| 2 | DO | Chip → Pi data | Pi pin **21** |
| 3 | /WP | Write-protect pin | **3.3 V** |
| 4 | GND | Ground | Pi pin **6** |
| 5 | DI | Pi → chip data | Pi pin **19** |
| 6 | CLK | Clock | Pi pin **23** |
| 7 | /HOLD | Pause pin | **3.3 V** |
| 8 | VCC | Power | Pi pin **1** (3.3 V) |

Three chip pins need 3.3 V and the Pi has only two 3.3 V pins (1 and 17), so:

- **With a breadboard (recommended).** Put the socket's board in the
  breadboard. Run Pi pin 1 to one row of the breadboard and connect chip pins
  8, 3 and 7 to that row. Put the capacitor between chip pin 8 and chip pin 4,
  as close to the socket as you can. It smooths the chip's power.
- **Without one.** Chip pin 8 to Pi pin 1, chip pin 7 to Pi pin 17, and leave
  pin 3 unconnected for now. The chip ignores /WP in the mode it starts up in
  (§7.1.3), but /HOLD must never be left unconnected (§4.4). Get a breadboard
  and the capacitor before long runs.

Pins 19, 21, 23 and 24 are the Pi's SPI0 pins. The chip select is CE0, which
the scripts below use as bus 0, device 0.

**Before you plug the power back in, check:**

1. The chip's dot is at socket pin 1. Back to front, it gets power on the wrong pin.
2. Chip pin 8 goes to Pi pin 1 or 17. **Never pin 2 or 4.** Those are 5 V, and
   5 V destroys the chip (it takes 2.7 to 3.6 V, §9.2).
3. Every wire is pushed all the way on. A loose wire looks like a bug in our
   code.

Keep the wires short, and keep them from being bumped.

## 4. Say hello to the chip

On the Pi, from the `FTL` folder:

```sh
python3 tools/pi/chip_id.py
```

It should print:

```
Chip ID:  EF AA 21   (want EF AA 21)
Protection:     7C   as expected  (every block write-protected, as at every power-up)
Configuration:  18   as expected  (ECC on, buffer read mode)
Status:         00   as expected  (idle, no failed program or erase)
```

`EF` is Winbond's maker code and `AA 21` means W25N01GV (§8.2.2). If you see
something else, the script tells you what to check.

The script only reads. Running it again is always safe.

## 5. Find each chip's factory bad blocks

Do this once for every chip, **before anything programs or erases it**. The
factory marks a bad block with a non-`FF` byte at the start of the block's
first page (§10.2). Erasing a bad block wipes that mark for good.

Give every chip a number. Write the numbers on the storage box, one chip per
compartment. Then, for chip 1:

```sh
python3 tools/pi/scan_bad_blocks.py chip-1
```

It reads page 0 of all 1,024 blocks and saves the bad ones to
`bad-blocks-chip-1.txt`. A new chip has at most 20 (§10.1) and often none. If the
script warns that there are more, or that block 0 is bad, the chip has been
used before. Set it aside and tell the captain.

Unplug the power, swap in the next chip, plug it back in, and run it again
with `chip-2`, and so on. Keep one chip that nobody writes to as a spare.

## 6. Run the FTL on the chip

Once the driver is written, this runs the whole FTL on the first 16 blocks of
the chip and checks every read against the reference model:

```sh
cmake --build --preset dev
build/dev/ftl_run --chip --blocks 16 --bad-blocks bad-blocks-chip-1.txt
```

It erases those 16 blocks first, so never point it at a chip whose data you
want to keep. It takes about 5 minutes at the default 1 MHz. Once it works,
add `--speed 8000000` to make it about 8 times faster.

## Working on the Pi as a team

Anyone on the same Wi-Fi as the Pi can work on it with
`ssh yourname@vincentftl.local`. Only one person at a time should run anything
that uses the chip.
