# Runnable tests

These tests run on Linux or Ubuntu/WSL, using Python 3's standard library and
a C17 compiler. They require the checked-in `src` and `generated` files;
they do not require Conda, REA, original disk images, emulator state or private
research fixtures.

Run the audio conversion tests without SDL:

```sh
python3 -m unittest discover -s tests -p 'test_audio_pcm.py' -v
```

For the production host tests, install your distribution's SDL2 development
package, GCC and Python 3 first. SDL2 is discovered with `sdl2-config` or
`pkg-config`. Then run:

```sh
python3 -m unittest discover -s tests -p 'test_host_portable.py' -v
```

Alternatively set `SDL2_CFLAGS` and `SDL2_LIBS` to explicit compiler/linker
arguments. `HOST_CC` overrides GCC. These variables are parsed as shell-like
argument lists; quote paths containing spaces inside their values.

The host suite checks eight input/pause/audio adapter scenarios, then replays
the compact input-only route in `generated_host_route.h` through the actual
production event, gameplay, rendering and PCM code. It expects level-one
victory after 2,466 gameplay frames, sword collection and guard defeat. The
route fixture contains inputs only, with no game state or original helper
replay. SDL's dummy video/audio drivers keep the tests headless. They do not
establish Windows window behavior, physical keyboard delivery or audible
speaker output, and this finite route does not cover every possible input.

Build products and diagnostic JSON are written only under ignored `build/`.
The larger original-game comparison suite remains a separate local research
corpus and is not included or required by this runnable test set.

