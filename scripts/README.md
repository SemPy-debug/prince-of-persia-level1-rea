# Windows build from Linux or Ubuntu/WSL

Requirements: Bash, a Windows AMD64 MinGW-w64 GCC toolchain, Python 3, GNU
coreutils, and SDL2's MinGW development SDK. Python uses only its standard
library. Conda, REA and private research fixtures are not build dependencies.

The tested SDK is SDL 2.32.10. Obtain `SDL2-devel-2.32.10-mingw.tar.gz` from
[the official SDL release](https://github.com/libsdl-org/SDL/releases/tag/release-2.32.10)
and extract it outside the repository or into ignored `tooling/`. Set
`SDL2_SDK` to its `SDL2-2.32.10` directory (or its nested
`x86_64-w64-mingw32` directory). From any checkout location:

```sh
SDL2_SDK=/path/to/SDL2-2.32.10 bash scripts/build_windows.sh
```

The script writes `build/windows/PrinceOfPersia-Level1.exe`, source and
dependency hashes, and an executable SHA-256. It statically links SDL2 and
uses Windows system DLLs at runtime. The executable requires neither Python
nor WSL when played on Windows.

To select tools or output:

```sh
CC=x86_64-w64-mingw32-gcc PYTHON=python3 SDL2_SDK=/path/to/SDL2-2.32.10 \
  bash scripts/build_windows.sh build/custom-output
```

The manifest checks that compiler-reported input files did not change during
the build. It can contain absolute external SDK paths and stays in ignored
build output. A build receipt is not a claim of bit-for-bit reproducibility
across different compilers or build timestamps.

See [`tests/README.md`](../tests/README.md) for the independently runnable
portable test subset and its full first-level input route.

To package an already-built EXE, without changing its bytes:

```sh
python3 scripts/package_release.py --exe build/windows/PrinceOfPersia-Level1.exe --version 1.0.0
```

The output under ignored `dist/` includes a Windows ZIP, player documentation,
license notices, individual downloads and SHA-256 checksums. The optional
`--expected-sha256` argument checks the input EXE before packaging. Use a new
`--output` folder for each packaging run.
