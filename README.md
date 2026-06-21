# R2-Boy - DMG Emulator

A Nintendo Game Boy (DMG-01) emulator written in C for educational purposes focusing on learning how the original hardware works.

---

## Features

- DMG (Original Game Boy) emulation
- CPU instruction emulation
- Memory bus implementation
- Cartridge loading
- Timer emulation
- PPU (Pixel Processing Unit) emulation
- SDL-based graphics output
- Keyboard input support
- Testing support
- Save game support
- M-Cycle accurate

---

## Current Status

This project is under active development.

Planned:
- Audio emulation

---

## Quick Start

### Requirements

- GCC
- Make
- SDL2

### Compile

Install dependencies (On Ubuntu/Debian like systems):

```bash
sudo apt install build-essential libsdl2-dev
```

Create the build directory:
```bash
mkdir build
```

Build the emulator:

```bash
make
```

The executable will be generated in:

```bash
build/r2boy
```

### Usage

Run a Game Boy ROM:

```bash
./build/r2boy [-d/--debug] [-b/--bios bios_file] rom.gb
```

---

## BIOS

R2-Boy supports the Game Boy boot ROM, just use the option:

```bash
./build/r2boy --bios [biosfile] game.gb
```

If you don't use the option, it will use the default path "roms/bios.bin".

If the emulator cannot find a boot ROM, it will manually initialize the registers and directly run the game.

---

## Debug Mode

For running tests like mooneye, use the option:

```bash
./build/r2boy -d test.gb
```

In debug mode, video output is disabled and the emulator automatically checks
the register values used by Mooneye test ROMs to determine whether the test passed.

---

## Controls

| Key | Action |
| :---: | :---: |
| Arrow keys | D-Pad |
| X | A |
| Z | B |
| Enter | Start |
| Backspace | Select |

---

## Accuracy

The emulator aims to emulate the Game Boy hardware at the cycle level whenever possible.

Currently passes the following test ROMs:

Blargg:

- halt_bug
- instr_timing
- cpu_instrs
- mem_timing
- mem_timing-2
- oam_bug

- dmg_acid2

---

## License

MIT

---

## Project Structure
```
src/
├── bus/
├── cartucho/
├── cpu/
├── frontend/
├── ppu/
├── timer/
├── gb.c
├── gb.h
└── main.c
```

---
