############
# Makefile #
############
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
# The main makefile of the program
#
#
# make all (or make): build the program
# make clean:         to clean up
#
#

include Make_defines.mk

CFLAGS  +=

OBJECTS  = $(OBD)/main.o
OBJECTS += $(OBD)/cu_ufile.o
OBJECTS += $(OBD)/cu_hfile.o
OBJECTS += $(OBD)/cu_avr.o
OBJECTS += $(OBD)/cu_avrc.o
OBJECTS += $(OBD)/cu_ctr.o
OBJECTS += $(OBD)/cu_spi.o
OBJECTS += $(OBD)/cu_spisd.o
OBJECTS += $(OBD)/guicore.o
OBJECTS += $(OBD)/audio.o
OBJECTS += $(OBD)/frame.o
OBJECTS += $(OBD)/eepdump.o
ifeq ($(TSYS),emscripten)
ROMFILE  = gamefile.uze
else
ROMFILE  =
endif


DEPS     = *.h Makefile Make_defines.mk Make_config.mk

all: $(OUT)
clean:
	rm    -f $(OBJECTS) $(OUT)
	rm -d -f $(OBD)

$(ROMFILE):
	$(error You need to supply a gamefile.uze for Emscripten build)

$(OUT): $(OBD) $(OBJECTS) $(ROMFILE)
	$(CC) -o $(OUT) $(OBJECTS) $(CFSPD) $(LINK)

$(OBD):
	mkdir $(OBD)

$(OBD)/main.o: main.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)

$(OBD)/cu_ufile.o: cu_ufile.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)

$(OBD)/cu_hfile.o: cu_hfile.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)

$(OBD)/cu_avr.o: cu_avr.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSPD)

$(OBD)/cu_avrc.o: cu_avrc.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)

$(OBD)/cu_ctr.o: cu_ctr.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)

$(OBD)/cu_spi.o: cu_spi.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)

$(OBD)/cu_spisd.o: cu_spisd.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)

$(OBD)/guicore.o: guicore.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSPD)

$(OBD)/audio.o: audio.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSPD)

$(OBD)/frame.o: frame.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSPD)

$(OBD)/eepdump.o: eepdump.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)


.PHONY: all clean
