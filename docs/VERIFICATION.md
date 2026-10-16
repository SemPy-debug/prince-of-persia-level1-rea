# Verification and limitations

This page summarizes the measured acceptance of the native level-one executable shipped as v1.0.0, verified on 16 October 2026. It distinguishes original-game comparisons, native host checks and user testing. It does not assert complete equivalence for every possible input.

## Completed original/native route

The unchanged original Apple II game and the native C implementation completed the same keyboard-only route through level one:

- **2,466 consecutive gameplay interval endpoints** compared.
- **108 selected game variables** and **2,315 owned mutable bytes** agreed at every endpoint.
- Native state was reset once; no intermediate original state or recorded helper return was replayed into the native game.
- The route includes sword collection, guard defeat, exit opening, exit animation and the music polls encountered along the way.
- The run stopped at the original level transition before loading level two.
- Plain and instrumented original runs had equal full physical endpoint fingerprints and instruction/input/output counts, checking that observation did not change the measured run.

The music polls encountered on that route were 45, 114, 135, 39 and 238 for resources 3, 4, 7, 8 and 9 respectively. Separate finite music controls covered all nine identified first-level cues: resources 1, 2, 3, 4, 6, 7, 8, 9 and 12, with **962 original units and 31,780 speaker edges** compared.

## Rendering and host checks

Live-room component controls compared **1,137 complete 8 KB pages** (**9,314,304 bytes**) covering initial rooms, changed geometry, mutable tile states, lighting/animation phases and foreground inputs. These are independently redrawn room views; they do not establish identical rendering history in every continuous frame.

The production host's event handling, rendering and PCM code completed the same 2,466-frame route using SDL dummy video. It reached the level-one completion state with the sword collected and the guard defeated. Dummy video is a headless integration check, not a real Windows display, keyboard or audio-device test.

A user separately opened the Windows package and reported that it worked. Its runtime report recorded the Windows video driver, real input events, an open audio device and queued PCM. This confirms launch and interaction; it does not measure sound reaching the speakers or a complete manual level traversal.

The accepted initial Windows executable has SHA-256:

```text
4cd7144b388dbd150a20a33fde4251134f0789817b1dccf03b742f5da2d32ee3
```

The v1.0.0 release preserves that exact executable. A later rebuild may have a different hash. Check the checksums accompanying each release rather than assuming this hash identifies every release.

## Limits that remain

- The route and component tests cover finite selected cases, not every possible gameplay input or a complete Cartesian state space.
- Host wall-time pacing approximates observed original timing; it is not an Apple II cycle-exact host.
- Three flame-occlusion bits differed in two historical opening-page comparisons. No arbitrary pixel patch was used to conceal those counterexamples.
- Audible output has not been independently confirmed, although the Windows audio device opened and PCM was queued.
- The game implements level one only. It does not include later levels, saved games or a general Apple II emulator.

Raw original media, machine snapshots, private workstation reports and browser capture profiles are not part of this public source package. The documentation records the scope of the completed investigation without making those research artifacts runtime dependencies.

## Running the public tests

The [test instructions](../tests/README.md) describe the portable audio and headless production-host checks included with the source. Those tests can run without original media or the private research corpus. They supplement the acceptance summarized here; they do not recreate the entire original-game comparison investigation.

