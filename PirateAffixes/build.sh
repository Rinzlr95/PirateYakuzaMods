#!/bin/sh
set -eu
cd "$(dirname "$0")"
: "${ZIG:=zig}"
[ "$("$ZIG" version)" = "0.14.1" ] || { echo "Use Zig 0.14.1." >&2; exit 1; }
mkdir -p build
"$ZIG" cc -target x86_64-windows-gnu -shared -Os -s -std=c11 -Wall -Wextra -Werror -Iminhook/include PirateAffixes.c minhook/src/buffer.c minhook/src/hook.c minhook/src/trampoline.c minhook/src/hde/hde64.c -lbcrypt -ladvapi32 -lcomctl32 -luser32 -lgdi32 -o build/PirateAffixes.asi
