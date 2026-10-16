# Provenance and tool roles

## Native implementation and retained data

This project reconstructs the first level of the 1989 Apple II Prince of Persia. Its gameplay, rendering, window integration and audio playback are newly written native C, informed by original-game observations, documentation and original machine-code investigation. No existing Prince of Persia engine or port was adapted.

The project is not wholly new artwork or a new game design. Original graphics, animation tables, musical scores, level data and other retained lookup data remain original material. Files under `generated/` contain that retained data and generated representations used by the native implementation.

The Windows executable executes the C algorithms directly. It does not embed an Apple II CPU emulator or wrap the original game in an emulator. The original assets and the native implementation have different provenance.

## What REA contributed

The investigation used the actual REA executable/MCP tools, not only the accompanying skill text:

1. Establish the selected original executable bank, mapping and relevant entry points.
2. Inspect original instructions, calls and data through REA with Ghidra.
3. Trace selected behavior and retain structured evidence and unresolved questions.
4. Write a separate native C implementation.
5. Compare native behavior with controlled original execution, including bounded process verification and the completed first-level route.

REA's skill provided workflow instructions. Its guided prompts helped frame investigation and verification; they are not commands that automatically produce a complete clone. No meaningful percentage of the final game can be attributed to REA alone.

A general Apple II emulator provided original execution and bank-aware research captures. Ordinary compilers, test harnesses, SDL and build tools produced and checked the native game. These tools have distinct roles; the research emulator, REA, Ghidra, Codex, Python and WSL are not required to play the Windows release.

The original investigation used REA 6.5.0 and its matching workflow instructions. Official upstream documentation may describe newer versions.

## References and notices

- [REA project and documentation](https://github.com/morluto/rea)
- [REA reconstruction example: DX-Ball](https://rea.tools/showcase/dx-ball/)
- [REA raw-binary analysis guide](https://github.com/morluto/rea/blob/main/docs/raw-binary.md)
- [Jordan Mechner's original Apple II source archive and rights statement](https://github.com/jmechner/Prince-of-Persia-Apple-II)
- [Jordan Mechner's library and original technical documentation](https://www.jordanmechner.com/en/library/)
- [Apple2js](https://github.com/whscullin/apple2js): standard HGR display palette and neighborhood-behavior reference, credited in the third-party notices.
- [SDL](https://www.libsdl.org/): native window, input and audio support; SDL 2.32.10 is statically linked in the initial Windows package.

See [THIRD-PARTY-NOTICES.txt](../THIRD-PARTY-NOTICES.txt) and the `licenses/` directory for retained notices. Original game material, the native implementation and third-party components are identified separately. SDL and Apple2js notices apply to their respective components, not to the complete game.

This project is unofficial and does not imply affiliation with or endorsement by Jordan Mechner, Ubisoft, REA, SDL or Apple2js contributors.

