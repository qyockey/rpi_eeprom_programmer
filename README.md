# Raspberry Pi AT28C256 Programmer

## Overview

This project is designed to replace the need for buying an eeprom programmer machine and instead being able to run off of a relatively cheap raspberry pi. I have created it for use on my personal pi model 3B+, so users of other models might need to experiment changing the delay count in the functions `delay_400_ns()` and `pulse_write_enable()` to get accurate delays. This program is designed to be able to write a 32kB rom file to the rom chip, as well as the reverse, dumping the contents of the rom chip to a 32kB file.

## Getting Started

Standard procedure for cloning a git repo

```bash
$ git clone https://gitlab.com/qyockey/rpi_eeprom_programmer.git
$ cd rpi_eeprom_programmer
```

Connect pins on the AT28C256 to the Raspberry Pi GPIO pins as follows:

| AT28C256 Pins | GPIO Pins |
|---------------|-----------|
| A0 - A14      | 0 - 14    |
| OE            | 15        |
| I/O0 - I/O7   | 16 - 23   |
| WE            | 24        |

These pin mappings can be easily modified as needed at the top of `eeprom_programmer.c`

The remanig pins on the AT28C256 should be connected according to the following table:

| AT28C256 Pins | Connection |
|---------------|------------|
| CE            | 0V         |
| VCC           | 5V         |
| GND           | 0V         |

## Usage

```bash
# compile programmer
$ make
```

```bash
# print help
$ ./eeprom_programmer -h
# OR
$ make help
```

```bash
# write binary file to chip
$ ./eeprom_programmer -w <rom_file>
# OR
$ make write  # writes file rom.bin
```

```bash
# dump rom file from chip
$ ./eeprom_programmer -d <dump_output_file>
# OR
$ make dump  # dumps to file dump.bin
```

```bash
# remove build files
$ make clean
```

## Known Bugs

- When dumping, can occasionally read a garbage value if reading a byte 0xff immediately preceded by 0x00. Since this project is designed to be a programmer and not a rom dumper, this issue is not high on the priority list.
- If testing with valgrind, the screen is full of thousands of memory errors. Those are  not my creating, they are part of the library this is built off of, but the functionality of the eeprom programmer seems unaffected.

