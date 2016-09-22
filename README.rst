
CUzebox emulator
==============================================================================

:Author:    Sandor Zsuga (Jubatian)
:License:   GNU GPLv3 (version 3 of the GNU General Public License)




Overview
------------------------------------------------------------------------------


This is a currently experimental emulator for the Uzebox game console written
entirely in C, using SDL 2.

It can be compiled with Emscripten as well for providing games embedded in web
sites. For games it supports it works better than the Uzem emulator.

For building, use the Make_config.mk file to specify target platform.
Currently it should build for Linux and Emscripten, and Windows as cross
compile target using mingw32-gcc (I can not test native compiles for that
platform since I don't have the OS).




Controls
------------------------------------------------------------------------------


Currently only Player 1's SNES controller is added, as follows:

- Arrow keys: D-Pad
- Q: Button Y
- W: Button X
- A: Button B
- S: Button A
- Enter: Start
- Space: Select
- Left shift: Left shift
- Right shift: Right shift

The emulator itself can be controlled with the following keys:

- ESC: Exit
- F2: Toggle "small" display (faster, but more blurry if resized)
- F3: Toggle debug informations (slightly faster with them off)
- F4: Toggle frame rate limiter
- F5: Toggle video capture (only if compiled in)
- F8: Toggle keymap between SNES and UZEM (Default: SNES)
- F11: Toggle full screen

The UZEM keymap maps the buttons of the SNES controller according to the Uzem
emulator with the addition that 'Y' also triggers an SNES Y button press (so
the mapping is useful on a QWERTZ keyboard). It is not recommended for
developing games with complex controls since its layout differs much to the
layout of the real controller.

If you want to see how fast the AVR core can possibly run, turn off debug
informations and use a small display while the frame rate limiter is off. On
computers where the AVR core can actually run very fast this will still have
some rendering (and notably blit to screen) overhead (since the frames are
still fully rendered even then).

The Emscripten build can be maximized to full screen with the browser's full
screen option.




Emulated components
------------------------------------------------------------------------------


Currently the following features are implemented:

- AVR core with cycle perfect emulation (except the SPM instruction).
- Core AVR peripherals as necessary to run Uzebox games.
- Sound and video output (frame rate synchronized to host if possible).
- SNES controllers (only Player 1's controller is exposed).
- EEPROM including saving its contents alongside the emulated game.
- SD Card read and write (writing is untested).
- SPI RAM.

Currently lacking but planned:

- SPM instruction (allowing bootloader emulation).
- Emulator state saves (snapshots).
- Mouse and keyboard controllers.
- Networking features as provided by the ESP8266 over the UART.
- More elaborate GUI.

Notes:

The SD card emulation is fairly capable, it has several compile time
constants in the cu_spisd.c file which you may adjust. You can set them to
emulate a strict SD card breaking several existing games, but which can help
you developing more robust SD code.

The SD write feature is sandboxed within the directory of the game. It doesn't
emulate subdirectories. You may only override existing file contents, expand
files or create new ones, it should be capable to track these operations if
you write the FAT first.




Video capture
------------------------------------------------------------------------------


By default the video capture feature is not compiled in. You can enable it in
Make_config.mk.

To use it, you need ffmpeg installed with mp3lame for audio and H.264 for
video.

You can toggle capturing with F5 during running the emulation: you may use it
multiple times to capture only sections of a session. During this phase the
emulator will write out large uncompressed video to allow running reasonably
well.

The video capture is independent of the frame rate management: you will get
perfect continuous 60 FPS video even if the emulator slows down or skips
frames due to being unable to keep up with the task.

When you exit the emulator, it will launch a slow video encoding step when it
produces proper 720p H.264 video from the material it recorded.
