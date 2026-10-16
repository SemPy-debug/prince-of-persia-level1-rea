#!/usr/bin/env bash
# Cross-build a standalone Windows AMD64 application on Linux or Ubuntu/WSL.
set -euo pipefail
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$task_root"
out=${1:-build/windows}
compiler=${CC:-x86_64-w64-mingw32-gcc}
python=${PYTHON:-python3}
: "${SDL2_SDK:?Set SDL2_SDK to the extracted SDL2 MinGW development SDK root}"
sdk=$SDL2_SDK
if [[ -d "$sdk/x86_64-w64-mingw32" ]]; then
    sdk=$sdk/x86_64-w64-mingw32
fi
sdk=$(cd -- "$sdk" && pwd)
for required in include/SDL2/SDL.h lib/libSDL2.a lib/libSDL2main.a; do
    [[ -f "$sdk/$required" ]] || { printf 'Missing SDL2 SDK file: %s\n' "$sdk/$required" >&2; exit 1; }
done
command -v "$compiler" >/dev/null
command -v "$python" >/dev/null
command -v sha256sum >/dev/null
mapfile -t files < <(sed -e 's/\r$//' -e '/^[[:space:]]*#/d' -e '/^[[:space:]]*$/d' scripts/sources.list)
for input in "${files[@]}"; do
    [[ -f "$input" ]] || { printf 'Missing build input: %s\n' "$input" >&2; exit 1; }
done
mkdir -p "$out"
"$compiler" -std=c17 -O2 -MM -Isrc -I"$sdk/include/SDL2" "${files[@]}" > "$out/dependencies.mk"
"$python" scripts/build_dependency_manifest.py "$out/dependencies.mk" "$out/source-manifest.sha256" \
    "$sdk/lib/libSDL2.a" "$sdk/lib/libSDL2main.a"
"$compiler" -std=c17 -O2 -Wall -Wextra -Werror -pedantic -static -static-libgcc \
    -ffunction-sections -fdata-sections -Wl,--gc-sections -mwindows -Isrc -I"$sdk/include/SDL2" \
    "${files[@]}" -L"$sdk/lib" -lmingw32 -lSDL2main -lSDL2 -lm -ldinput8 -ldxguid -ldxerr8 \
    -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid \
    -o "$out/PrinceOfPersia-Level1.exe"
sha256sum --check "$out/source-manifest.sha256" > "$out/source-verification.txt"
sha256sum "$out/PrinceOfPersia-Level1.exe" > "$out/executable.sha256"
printf 'Built %s\n' "$out/PrinceOfPersia-Level1.exe"

