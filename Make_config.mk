############################
# Makefile - configuration #
############################
#
#  Copyright (C) 2016 - 2017
#    Sandor Zsuga (Jubatian)
#  Uzem (the base of CUzeBox) is copyright (C)
#    David Etherton,
#    Eric Anderton,
#    Alec Bourque (Uze),
#    Filipe Rinaldi,
#    Sandor Zsuga (Jubatian),
#    Matt Pandina (Artcfox)
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#
#
# Alter this file according to your system to build the thing
#
#
#
# Target operating system. This will define how the process will go
# according to the os's features. Currently supported:
#  linux
#  windows_mingw
#  emscripten
#
TSYS=linux
#
#
# A few paths in case they would be necessary. Leave them alone unless
# it is necessary to modify.
#
# For a Windows build, you might need locating SDL2 here. When doing a cross
# compile from (Debian) Linux to 32 bit Windows, the followings might work
# assuming that a development library was downloaded from libsdl.org:
#
#CC_INC=SDL2-2.0.4/i686-w64-mingw32/include
#CC_LIB=SDL2-2.0.4/i686-w64-mingw32/lib
#CCOMP=i686-w64-mingw32-gcc
#CCNAT=gcc
#
# Note that for Emscripten builds you might also have to define these to
# compile assets necessary to build the emulator.
#
CC_BIN=
CC_INC=
CC_LIB=
#
#
# Version number to use. It should be a BCD date of YYYYMMDD format.
#
VER_DATE=0x20170920
#
#
# In case a test build (debug) is necessary, give 'test' here. It enables
# extra assertions, and compiles the program with no optimizations, debug
# symbols enabled.
#
GO=
#
#
# An extra path to game controller config files for SDL2 builds. By default
# the emulator looks for such a file (gamecontrollerdb.txt) on the SDL app
# path (if there is any), you can supply an additional path here. You can get
# such a file from https://github.com/gabomdq/SDL_GameControllerDB , such a
# file may also be present on your system as part of some other application
# (for example gnome-games uses ~/.config/gnome-games/gamecontrollerdb.txt,
# the code won't recognize "~" though). You may provide that location here to
# "leech" that config, so CUzeBox's controller mappings update when you update
# that application.
#
PATH_GAMECONTROLLERDB=
#
#
# Should the video capture feature be built in? Note that it requires ffmpeg
# and it is not possible to have it in the Emscripten build.
#
FLAG_VCAP=0
#
#
# Initial display: Game only (1) or show the emulator interface (0) (currently
# memory occupation & sync signals). The F3 key may toggle it runtime.
#
FLAG_DISPLAY_GAMEONLY=0
#
#
# Initial display size: A "small" (1) display is faster (it can be resized,
# but quality is low). The F2 key may toggle it runtime.
#
FLAG_DISPLAY_SMALL=0
#
#
# Initial state of frame merging: Enabling merging (1) makes certain games
# flickering less (notably which use some sprite cycling algorithms to get
# around limitations) while making the emulation running somewhat slower.
# The F7 key may toggle it runtime.
#
FLAG_DISPLAY_FRAMEMERGE=1
#
#
# Perform a self-contained build without filesystem access. This can only be
# done for games which don't demand files from the SD card. It integrates the
# game (gamefile.uze) in the emulator executable, for Emscripten this is a
# smaller build.
#
FLAG_SELFCONT=0
#
#
# Disable all console output. On some systems for some reason console output
# can be very slow, this eliminates all such calls, also reducing the
# application size (but good bye, debug info!).
#
FLAG_NOCONSOLE=0
#
#
# For an Emscripten build, disable linking with a game (also keep
# FLAG_SELFCONT clear for this to work). If you do this, you will need to
# supply the game and any further files externally, injecting it into the
# Emscripten FS before starting the emulator. Note that the default html
# output doesn't include displaying the contents of the error channel, so you
# won't see any error! (Open the Web console, and check for logs / errors
# there to check the error output of the gamefile loading functions)
#
FLAG_NOGAMEFILE=0
#
#
# For an Emscripten build, request compiling with the AVR CPU opcode emulation
# intended for native use. This is likely slower for this target, but results
# in ~25Kb smaller output.
#
FLAG_NATIVE=0
#
#
# Force using SDL1 instead of SDL2. The latter is the default for all targets
# except Emscripten.
#
FLAG_USE_SDL1=0
