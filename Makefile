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
OBJECTS += $(OBD)/cu_avrfg.o
OBJECTS += $(OBD)/cu_ctr.o
OBJECTS += $(OBD)/cu_spi.o
OBJECTS += $(OBD)/cu_spir.o
ifeq ($(FLAG_SELFCONT), 0)
OBJECTS += $(OBD)/cu_vfat.o
OBJECTS += $(OBD)/cu_spisd.o
OBJECTS += $(OBD)/filesys.o
endif
OBJECTS += $(OBD)/eepdump.o
OBJECTS += $(OBD)/guicore.o
OBJECTS += $(OBD)/audio.o
OBJECTS += $(OBD)/ginput.o
OBJECTS += $(OBD)/frame.o
OBJECTS += $(OBD)/chars.o
OBJECTS += $(OBD)/textgui.o
OBJECTS += $(OBD)/conout.o
ifneq ($(ENABLE_VCAP), 0)
OBJECTS += $(OBD)/avconv.o
endif
ifneq ($(FLAG_SELFCONT), 0)
OBJECTS += $(OBD)/gamefile.o
OBJECTS += $(OBD)/filesmin.o
endif
ifeq ($(TSYS),emscripten)
ifeq ($(FLAG_SELFCONT), 0)
ifeq ($(FLAG_NOGAMEFILE), 0)
ROMFILE  = gamefile.uze
else
ROMFILE  =
endif
else
ROMFILE  =
endif
else
ROMFILE  =
endif


DEPS     = *.h Makefile Make_defines.mk Make_config.mk

all: $(OUT)
clean:
	rm    -f $(OBJECTS) $(OUT)
	rm    -f gamefile.c
	rm    -f assets/$(CHCONV)
	rm    -f assets/$(BINCONV)
	rm -d -f $(OBD)

$(ROMFILE):
	$(error You need to supply a gamefile.uze for Emscripten build)

$(OUT): $(OBD) $(OBJECTS) $(ROMFILE)
	$(CC) -o $(OUT) $(OBJECTS) $(CFSPD) $(LINK)

$(OBD):
	mkdir $(OBD)

# Special: Generate character set from the charset data. On Windows, whatever
# may happen, so especially chars.c is never explicitly removed, and is
# included in a Git repository.
#
#chars.c: assets/$(CHCONV)
#	assets/$(CHCONV) >chars.c

assets/$(CHCONV): assets/chconv.c assets/charset.h
	$(CCNAT) $< -o $@ -Wall

# Special: Generate C source from a gamefile.uze for a self-contained build.

gamefile.c: assets/$(BINCONV) gamefile.uze
	assets/$(BINCONV) <gamefile.uze >gamefile.c

assets/$(BINCONV): assets/binconv.c
	$(CCNAT) $< -o $@ -Wall

# Objects

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

$(OBD)/cu_avrfg.o: cu_avrfg.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)

$(OBD)/cu_ctr.o: cu_ctr.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)

$(OBD)/cu_spi.o: cu_spi.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSPD)

$(OBD)/cu_spisd.o: cu_spisd.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSPD)

$(OBD)/cu_spir.o: cu_spir.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSPD)

$(OBD)/cu_vfat.o: cu_vfat.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)

$(OBD)/filesys.o: filesys.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSPD)

$(OBD)/filesmin.o: filesmin.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSPD)

$(OBD)/guicore.o: guicore.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSPD)

$(OBD)/audio.o: audio.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSPD)

$(OBD)/ginput.o: ginput.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)

$(OBD)/frame.o: frame.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSPD)

$(OBD)/eepdump.o: eepdump.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)

$(OBD)/avconv.o: avconv.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)

$(OBD)/textgui.o: textgui.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSPD)

$(OBD)/conout.o: conout.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)

$(OBD)/chars.o: chars.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)

$(OBD)/gamefile.o: gamefile.c $(DEPS)
	$(CC) -c $< -o $@ $(CFSIZ)


.PHONY: all clean
