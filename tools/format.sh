#!/usr/bin/env bash
# Formats every C++ file in the repo with the pinned clang-format.
#
#   tools/format.sh           rewrite files in place
#   tools/format.sh --check   change nothing; fail if any file needs formatting (CI runs this)
set -euo pipefail
cd "$(dirname "$0")/.."

pinned=$(sed -n 's/^clang-format==//p' tools/requirements.txt)

if [[ -x .venv/bin/clang-format ]]; then
  clang_format=.venv/bin/clang-format
elif command -v clang-format >/dev/null 2>&1; then
  clang_format=clang-format
else
  echo "clang-format not found. Run ./setup.sh first." >&2
  exit 1
fi

# Different clang-format versions format differently, so a mismatch would make
# code pass here and fail in CI.
actual=$("$clang_format" --version | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
if [[ "$actual" != "$pinned" ]]; then
  echo "clang-format $actual found, but the repo pins $pinned. Run ./setup.sh." >&2
  exit 1
fi

# Tracked files plus new files you haven't `git add`ed yet. Captured first so
# a git failure stops the script instead of silently checking nothing.
listing=$(git ls-files --cached --others --exclude-standard -- '*.cpp' '*.hpp' '*.h' '*.cc')
files=()
while IFS= read -r f; do [[ -n "$f" ]] && files+=("$f"); done <<<"$listing"
if [[ ${#files[@]} -eq 0 ]]; then
  echo "No C++ files to format."
  exit 0
fi

if [[ "${1:-}" == "--check" ]]; then
  if ! "$clang_format" --dry-run --Werror "${files[@]}"; then
    echo >&2
    echo "Formatting check failed. Run tools/format.sh, then commit the result." >&2
    exit 1
  fi
  echo "Formatting OK (${#files[@]} files)."
else
  "$clang_format" -i "${files[@]}"
  echo "Formatted ${#files[@]} files."
fi
