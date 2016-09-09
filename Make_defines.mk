######################
# Make - definitions #
######################
#
#  Copyright (C) 2016
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
# This file holds general definitions used by any makefile, like compiler
# flags, optimization and such. OS - specific building schemes should also
# be written here.
#
#
include Make_config.mk
CFLAGS=
#
#
# The compiler
#
ifeq ($(TSYS),linux)
CCOMP?=gcc
endif
ifeq ($(TSYS),windows_mingw)
CCOMP?=gcc
endif
ifeq ($(TSYS),emscripten)
CCOMP?=emcc
endif
#
#
# Linux - specific
#
ifeq ($(TSYS),linux)
CFLAGS+= -DTARGET_LINUX
LINKB= -lSDL2
endif
#
#
# Windows - MinGW specific
#
ifeq ($(TSYS),windows_mingw)
OUT=cuzebox.exe
CFLAGS+= -DTARGET_WINDOWS_MINGW -Dmain=SDL_main
LINKB= -lmingw32 -lSDL2main -lSDL2 -mwindows
endif
#
#
# Emscripten - specific
#
ifeq ($(TSYS),emscripten)
OUT=cuzebox.html
EFLAGS= -s USE_SDL=2 -s NO_EXIT_RUNTIME=1 -s NO_DYNAMIC_EXECUTION=1
CFLAGS+= -DTARGET_EMSCRIPTEN $(EFLAGS)
LINKB=$(EFLAGS) --preload-file gamefile.uze
endif
#
#
# When asking for debug edit
#
ifeq ($(GO),test)
CFSPD=-O0 -g
CFSIZ=-O0 -g
CFLAGS+= -DTARGET_DEBUG
endif
#
#
# 'Production' edit
#
ifeq ($(TSYS),emscripten)
CFSPD?=-O3 --llvm-lto 3 -s ASSERTIONS=0 -s AGGRESSIVE_VARIABLE_ELIMINATION=1
CFSIZ?=-Os --llvm-lto 3 -s ASSERTIONS=0
else
CFSPD?=-O3 -s -flto
CFSIZ?=-Os -s -flto
endif
#
#
# Now on the way...
#

LINKB?=
LINK= $(LINKB)
OUT?=cuzebox
CC=$(CCOMP)

OBD=_obj_

CFLAGS+= -Wall -pipe -pedantic
ifneq ($(CC_BIN),)
CFLAGS+= -B$(CC_BIN)
endif
ifneq ($(CC_LIB),)
CFLAGS+= -L$(CC_LIB)
endif
ifneq ($(CC_INC),)
CFLAGS+= -I$(CC_INC)
endif

CFSPD+= $(CFLAGS)
CFSIZ+= $(CFLAGS)

