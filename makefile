# Overwrite PI-version just when compiling:
# Set to 1 for pi 1, 2 for pi 2 OR 3, or 4 for pi 4
PI_VERSION ?= 2

CC = cc
CFLAGS = -O0 -W -Wall -std=c99 -D_XOPEN_SOURCE=500 -g -DPI_VERSION=$(PI_VERSION)
OBJECTS = eeprom_programmer.o gpio.o mailbox.o
EXE = eeprom_programmer

$(EXE): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(EXE)

eeprom_progammer.o: eeprom_programmer.c gpio.h
gpio.o: gpio.c gpio.h mailbox.h
mailbox.o: mailbox.c mailbox.h

.PHONY: help dump write clean

help:
	make $(EXE)
	sudo ./$(EXE) -h

dump:
	make $(EXE)
	sudo ./$(EXE) -d dump.bin
	hexdump -C dump.bin

write:
	make $(EXE)
	sudo ./$(EXE) -w rom.bin

clean:
	-rm -f $(EXE) $(OBJECTS)

