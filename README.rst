
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


On the keybouard, you can control Player 1's SNES controller as follows:

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
- F2: Toggle low quality display (faster, but blurry and ugly)
- F3: Toggle debug informations (slightly faster with them off)
- F4: Toggle frame rate limiter
- F5: Toggle video capture (only if compiled in)
- F7: Toggle frame merging (slower, eliminates flicker in some games)
- F8: Toggle keymap between SNES and UZEM (Default: SNES)
- F9: Pause / Unpause
- F10: Advance a single frame
- F11: Toggle full screen
- F12: Toggle between 1 player and 2 player controller allocation

The UZEM keymap maps the buttons of the SNES controller according to the Uzem
emulator with the addition that 'Y' also triggers an SNES Y button press (so
the mapping is useful on a QWERTZ keyboard). It is not recommended for
developing games with complex controls since its layout differs much to the
layout of the real controller.

If you have two (physical) game controllers, the emulator will start with two
player controller allocation (so one of the controllers belong to Player 1,
the other to Player 2), otherwise it will start with one player (both the
keyboard and a single game controller will belong to Player 1). The keyboard
always belongs to Player 1.

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

- AVR core with cycle perfect emulation.
- Core AVR peripherals as necessary to run Uzebox games.
- Sound and video output (frame rate synchronized to host if possible).
- SNES controllers (keyboard for Player 1, game controllers for both).
- EEPROM including saving its contents alongside the emulated game.
- SPM instruction and related elements necessary for bootloader emulation.
- SD Card read and write (writing didn't see much testing yet).
- SPI RAM.

Currently lacking but planned:

- Emulator state saves (snapshots).
- Mouse and keyboard controllers (Uzebox's SNES mouse and PS/2 keyboard).
- Networking features as provided by the ESP8266 over the UART.

Notes:

The SD card emulation is fairly capable, it has several compile time
constants in the cu_spisd.c file which you may adjust. You can set them to
emulate a strict SD card breaking several existing games, but which can help
you developing more robust SD code.

The SD write feature is sandboxed within the directory of the game. It doesn't
emulate subdirectories. You may only override existing file contents, expand
files or create new ones, it should be capable to track these operations if
you write the FAT first.

A bootloader can be started simply by passing the bootloader's .hex file as
parameter to the emulator. The virtual SD card is composed from the files
existing in the same directory. Note that the emulator can not remember what
the bootloader wrote last time into the flash (so you can not start the last
selected game with the normal Uzebox bootloader as it recognizes the game by a
CRC written in EEPROM, not by flash content).




Emscripten notes
------------------------------------------------------------------------------


After setting up Emscripten so it is capable to compile examples, at least on
Linux compiling the emulator should require the following steps:

- In "Make_config.mk", adjust the target (TSYS) to "emscripten".
- Copy a game renamed as "gamefile.uze" in the source tree.
- Run "Make".

The game can also be a .hex file (but needs to be renamed to "gamefile.uze").
This game will be added to the build's virtual filesystem.

A "cuzebox_minimal.html" file is also provided to demonstrate the Emscripten
build, which contains the bare necessities to start the compiled emulator in a
browser.

To get the minimal size for your build, you can set the following flags in
"Make_config.mk":

- FLAG_NOCONSOLE: This removes all console output. Normally console output
  shouldn't be really necessary (but test whether the game can be loaded all
  right first with a native compile or an Emscripten build with console
  output on).

- FLAG_SELFCONT: Integrates the game within the emulator. This removes the
  Emscripten virtual filesystem saving more than 100 KBytes, but it is only
  capable to work with games which don't need the SD card.

A compiled game needs the "cuzebox.js", the "cuzebox.html.mem" and either the
"cuzebox.html" or "cuzebox_minimal.html" files to function. It also needs
"cuzebox.data" if it was built with FLAG_SELFCONT set zero (default).




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

Note that the state of frame merging (F7) notably affects the performance of
video captures and the output size. Having it on results in larger video
sizes, slower encoding, and worse emulation performance. It should be turned
off for games which don't need it, but should be kept on where necessary (if
the game in question uses some type of sprite rotation or effect based on
rapidly alternating between two images).
