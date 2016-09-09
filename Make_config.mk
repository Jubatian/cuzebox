############################
# Makefile - configuration #
############################
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
TSYS=emscripten
#
#
# A few paths in case they would be necessary. Leave them alone unless
# it is necessary to modify.
#
CC_BIN=
CC_INC=
CC_LIB=
#
#
# In case a test build (debug) is necessary, give 'test' here. It enables
# extra assertions, and compiles the program with no optimizations, debug
# symbols enabled.
#
GO=
