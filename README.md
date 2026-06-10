# R2-Boy - DMG Emulator

A Nintendo Game Boy emulator written in C for educational purposes focusing on learning how the original hardware works.

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

---

## Current Status

This project is under active development.

Implemented:
- CPU instruction set
- Memory bus
- Cartridge support
- Timers
- LCD controller and rendering
- Basic input handling

Planned:
- Try to pass the Blargg tests
- Audio emulation
- Save game support
- Additional hardware accuracy improvements

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
./build/r2boy path/to/rom.gb
```

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
Nowadays it passes the following blargg test ROMs:
- halt_bug
- instr_timing
- cpu_instr

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
