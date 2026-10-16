# Prince of Persia — Level 1

A native Windows reconstruction of the first level of the 1989 Apple II game, with newly written C gameplay, rendering and audio code. Original graphics, animation tables, music and first-level data are retained. No existing Prince of Persia engine or port was adapted. The game ends after level one.

## Download and play

**[Download v1.0.0 for Windows x64](https://github.com/SemPy-debug/prince-of-persia-level1-rea/releases/download/v1.0.0/PrinceOfPersia-Level1-v1.0.0-Windows-x64.zip)**

1. Download the Windows game ZIP above and extract it to a folder you can write to.
2. Open **PrinceOfPersia-Level1.exe**.
3. Press **Enter** to begin.

Keep the manual and license files alongside the game. GitHub's automatic **Source code** ZIP contains the developer files, not the ready-to-run game. The [release page](https://github.com/SemPy-debug/prince-of-persia-level1-rea/releases/tag/v1.0.0) also offers the [standalone EXE](https://github.com/SemPy-debug/prince-of-persia-level1-rea/releases/download/v1.0.0/PrinceOfPersia-Level1.exe).

The game runs directly on **Windows x64**. No installation, Python, Conda, WSL, Codex, REA, Apple II emulator or separate SDL installation is required to play. The executable uses Windows system libraries and opens a game window and audio device. Audio-device failure does not prevent play.

## Controls

| Action | Key |
| --- | --- |
| Run / turn | Left / Right |
| Jump / climb | Up; combine with a direction to jump that way |
| Crouch | Down |
| Careful step | Stop, then hold Shift and tap Left / Right |
| Catch / hold a ledge | Hold Shift |
| Collect an item | Face it nearby and tap Shift |
| Sword strike | Tap Shift after collecting the sword |
| Parry | Tap Up as an attack approaches |
| Pause / resume | Enter or Space |
| Restart / mute / quit | R / M / Escape |

Step on floor plates to operate linked gates. Reach the opened final exit and press **Up**. See the [full player manual](docs/MANUAL.md), compact [controls reference](CONTROLS.txt), or [quick start](PLAY.txt) for all moves, combat and troubleshooting.

## Source and rebuilding

The C implementation is in `src/`; retained asset and lookup data are in `generated/`. The executable implements the game behavior directly, without running original Apple II CPU instructions.

Cross-build on Linux or Ubuntu/WSL with Bash, Python 3, a C17-capable MinGW-w64 toolchain and the official **SDL 2.32.10 MinGW development SDK**:

```sh
export SDL2_SDK=/path/to/SDL2-2.32.10
bash scripts/build_windows.sh
```

`SDL2_SDK` accepts the extracted SDK root or its `x86_64-w64-mingw32` subdirectory. The output is `build/windows/PrinceOfPersia-Level1.exe`. Optional `CC` and `PYTHON` environment variables select the cross-compiler and Python executable; a positional argument changes the output directory. The script records source dependencies and checksums.

These dependencies are for development only. See [runnable tests](tests/README.md) and [verification scope](docs/VERIFICATION.md).

## REA and verification

REA's executable analysis tools and Ghidra were used to investigate mapped original code and produce structured evidence. A general Apple II emulator supplied original execution and bank-aware research captures. The native C game was then written and compared with that behavior. REA's skill supplied workflow guidance; REA did not automatically generate a finished game. Neither REA nor the research emulator is bundled in the playable executable.

A captured **2,466-frame** first-level route matched **108 selected game variables** and **2,315 owned mutable bytes** at every gameplay endpoint, including sword collection, guard defeat and the exit. The production host also completed that route with SDL dummy video. A user separately launched the Windows executable and played with real keyboard input.

Verification covers finite routes and component controls, not every possible input or pixel. Host pacing approximates original timing. Three flame-occlusion bits differed in two historical opening-page comparisons. Windows audio-device opening and queued PCM were observed; audible output was not independently confirmed. See [details and limitations](docs/VERIFICATION.md).

## Credits and notices

Prince of Persia was created by Jordan Mechner. This unofficial project does not imply affiliation with or endorsement by Jordan Mechner or Ubisoft. Original game material and third-party components retain their respective notices; no repository-wide MIT license is asserted for the complete game.

See [provenance](docs/PROVENANCE.md), [third-party notices](THIRD-PARTY-NOTICES.txt), and `licenses/`. SDL and the credited Apple2js display reference have their own licenses, recorded separately.
