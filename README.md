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
- APU (Audio Processing Unit) emulation
- SDL-based graphics output
- Keyboard and Gamepad input support
- Testing support
- Save game support
- M-Cycle accurate
- Link Cable support

---

## Current Status

This project is under active development.

Planned:
- Link Cable preccision and make it more hardwre-accurate
- CGB support (more in the future)

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
./build/r2boy [-d|--debug] [-b|--bios bios_file] rom.gb
```

---

## BIOS

R2-Boy supports the Game Boy boot ROM, just use the option:

```bash
./build/r2boy --bios <biosfile> game.gb
```

If you don't use the option, it will use the default path "roms/bios.bin".

If no boot ROM is available, R2-Boy initializes the hardware registers to the same state produced by the original DMG boot ROM (DMG-ABC revision), allowing commercial games to start correctly.

---

## Debug Mode

For running tests like mooneye, use the option:

```bash
./build/r2boy -d test.gb
```

In debug mode, video output is disabled and the emulator automatically checks
the register values used by Mooneye test ROMs to determine whether the test passed.

## Link Cable

R2-Boy includes experimental Link Cable support over TCP/IP, allowing two emulator instances to communicate across the network.

### Host

```bash
./build/r2boy --link-host <PORT> game.gb
```

### Client
```bash
./build/r2boy --link-connect <IP>:<PORT> game.gb
```

### Example:

Host:

```bash
./build/r2boy --link-host 5000 tetris.gb
```

Client:

```bash
./build/r2boy --link-connect 192.168.1.25:5000 tetris.gb
```

The host must be started before the client connects.

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

It currently passes all Blargg tests (including oam bug!) and dmg-acid2. It also fails cgb sound exacly like a real DMG.

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
