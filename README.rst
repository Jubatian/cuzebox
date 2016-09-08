
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
