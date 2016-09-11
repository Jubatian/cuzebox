
CUzebox emulator
==============================================================================

:Author:    Sandor Zsuga (Jubatian)
:License:   GNU GPLv3 (version 3 of the GNU General Public License)




Overview
------------------------------------------------------------------------------


This is a currently experimental emulator for the Uzebox game console written
entirely in C, using SDL 2.

Presently it only supports the AVR CPU core itself (with precise timing),
essential peripherals will be added later on.

It can be compiled with Emscripten as well for providing games embedded in web
sites. For games it supports it works better than the Uzem emulator.

For building, use the Make_config.mk file to specify target platform (and if
necessary, for now just edit stuff, but should work for Linux and Emscripten).




Controls
------------------------------------------------------------------------------


Currently only Player 1's SNES controller is added, as follows:

- Arrow keys: D-Pad
- Q: Button Y
- W: Button X
- A: Button B
- S: Button A
- Enter: Select
- Space: Start

The emulator itself can be controlled with the following keys:

- ESC: Exit
- F2: Toggle "small" display (faster, but more blurry if resized)
- F3: Toggle debug informations (slightly faster with them off)
- F4: Toggle frame rate limiter
- F11: Toggle full screen

If you want to see how fast the AVR core can possibly run, turn off debug
informations and use a small display while the frame rate limiter is off. On
computers where the AVR core can actually run very fast this will still have
some rendering (and notably blit to screen) overhead (since the frames are
still fully rendered even then).
