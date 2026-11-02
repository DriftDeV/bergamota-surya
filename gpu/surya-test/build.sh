#!/bin/bash
# Build adreno618_test as arm64e, strip PAC + ad-hoc sign it with renegade (runs on the PAC-less Kryo 465), and build the
# host self-check variant (no IOKit) and run it.
#   gpu/surya-test/build.sh [OUTDIR]       default: gpu/surya-test/out
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:-$HERE/out}"
SDK="$(xcrun --sdk iphoneos --show-sdk-path)"
mkdir -p "$OUT"
xcrun --sdk iphoneos clang -arch arm64e -miphoneos-version-min=18.0 -isysroot "$SDK" -O1 -g -Wall -Wextra -Wno-unused-parameter \
    "$HERE/adreno618_test.c" -o "$OUT/adreno618_test.arm64e" -framework IOKit -framework CoreFoundation
"$HERE/../../tools/renegade/renegade" strip --sign -o "$OUT/adreno618_test" "$OUT/adreno618_test.arm64e"
chmod +x "$OUT/adreno618_test"
ls -l "$OUT/adreno618_test"
# host variant: only the generator and --selfcheck
cc -Wall -Wextra -O1 -DHOST_SELFCHECK "$HERE/adreno618_test.c" -o "$OUT/adreno618_selfcheck"
"$OUT/adreno618_selfcheck"
