#!/usr/bin/env bash
# One command from a fresh clone to passing tests. Safe to re-run any time.
set -euo pipefail
cd "$(dirname "$0")"

say() { printf '\n==> %s\n' "$*"; }
die() { printf '\nERROR: %s\n' "$*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

say "Checking system tools"
case "$(uname -s)" in
  Darwin)
    xcode-select -p >/dev/null 2>&1 ||
      die "Apple's command line tools are missing. Run: xcode-select --install   then re-run ./setup.sh"
    if ! have cmake; then
      have brew || die "CMake is missing. Install Homebrew (https://brew.sh), then re-run ./setup.sh"
      say "Installing CMake with Homebrew"
      brew install cmake
    fi
    ;;
  Linux)
    missing=()
    have c++ || missing+=(build-essential)
    have cmake || missing+=(cmake)
    python3 -c 'import venv, ensurepip' 2>/dev/null || missing+=(python3-venv)
    if [[ ${#missing[@]} -gt 0 ]]; then
      have apt-get || die "Install these with your package manager, then re-run: ${missing[*]}"
      say "Installing ${missing[*]} with apt (it will ask for your password)"
      sudo apt-get update
      sudo apt-get install -y "${missing[@]}"
    fi
    ;;
  *)
    die "Unsupported system. On Windows, run this inside WSL (see README)."
    ;;
esac

cmake_ok=$(cmake --version | awk 'NR==1 { split($3, v, "."); print (v[1] > 3 || (v[1] == 3 && v[2] >= 21)) }')
[[ "$cmake_ok" == 1 ]] ||
  die "CMake 3.21 or newer is needed; you have $(cmake --version | head -1). Upgrade it, then re-run."

say "Installing Python tools into .venv"
[[ -x .venv/bin/python ]] || python3 -m venv .venv
.venv/bin/python -m pip install --quiet --disable-pip-version-check -r tools/requirements.txt

jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)

say "Configuring (the first run downloads GoogleTest)"
cmake --preset dev

say "Building"
cmake --build --preset dev --parallel "$jobs"

say "Running tests"
ctest --preset dev

say "All set: everything builds and the tests pass."
