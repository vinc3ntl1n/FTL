# VincentFTL

[![CI](https://github.com/vinc3ntl1n/FTL/actions/workflows/ci.yml/badge.svg)](https://github.com/vinc3ntl1n/FTL/actions/workflows/ci.yml)

A Flash Translation Layer written from scratch in C++, running on a raw Winbond
W25N01GV SPI NAND chip driven by a Raspberry Pi 4. We use it to measure how
storage software policy affects the physical wear of real memory.

## Getting started

You're done when setup ends with **"All set: everything builds and the tests
pass."** Stuck for more than 30 minutes? Ask in Discord.

### macOS

1. Check that `git --version` works. If a window asks to install the command
   line developer tools, say yes and wait for it to finish.
2. Install [Homebrew](https://brew.sh) if you don't have it (`brew --version`
   to check).
3. Clone and run setup:
   ```sh
   git clone https://github.com/vinc3ntl1n/FTL.git
   cd FTL
   ./setup.sh
   ```

### Linux (Ubuntu / Debian)

```sh
sudo apt install -y git
git clone https://github.com/vinc3ntl1n/FTL.git
cd FTL
./setup.sh
```

`setup.sh` installs the compiler, CMake and Python venv support with `apt` if
they're missing, so it may ask for your password.

### Windows (WSL)

The project builds in Linux, and WSL runs a real Ubuntu inside Windows.

1. Open PowerShell as Administrator, run `wsl --install`, and restart.
2. Open the **Ubuntu** app from the Start menu and create a username and
   password when asked.
3. Inside Ubuntu, follow the **Linux** steps above. Clone into your Linux home
   folder (`cd ~` first), not under `/mnt/c/`. Builds there are very slow.
4. For VS Code, install the **WSL** extension, then run `code .` from the repo
   folder in Ubuntu.

## Everyday commands

| To… | Run |
|-----|-----|
| Build | `cmake --build --preset dev` |
| Run all tests | `ctest --preset dev` |
| Run only some tests | `ctest --preset dev -R Emulator` (matches test names) |
| Format your code | `tools/format.sh` |
| Check formatting the way CI does | `tools/format.sh --check` |
| Build and test with sanitizers, as CI does | `cmake --preset asan && cmake --build --preset asan && ctest --preset asan` |

The sanitizer build runs the same tests with AddressSanitizer and
UndefinedBehaviorSanitizer turned on. It catches reads past the end of a
buffer, use after free and similar memory bugs. Without it they usually show up
as wrong answers much later. If CI's sanitizer job fails and the normal job
passes, that's usually why.

## Layout

```
include/vincentftl/   headers — the interfaces everyone codes against
src/                  implementation
tests/                GoogleTest tests; any tests/*_test.cpp is picked up automatically
tools/                Python scripts (plots, heatmap) and dev tools
docs/                 write-ups, e.g. the NAND rules the emulator enforces
```

## How we work

Branch → pull request → review → merge. See [CONTRIBUTING.md](CONTRIBUTING.md).
Every PR runs three CI checks: build and test, sanitizers, and format. All
three must pass, plus one approving review, before anything reaches `main`.
