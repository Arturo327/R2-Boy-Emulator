# R2-Boy - DMG Emulator

A Nintendo Game Boy (DMG-01) emulator written in C for educational purposes focusing on learning how the original hardware works.

---

## Features

- DMG (Original Game Boy) emulation
- CPU instruction emulation (including halt-bug, OAM bug)
- Memory bus emulation
- Cartridge loading. Supported mappings:
  - MBC1
  - MBC2
  - MBC3 + RTC
  - MBC5 + rumble
  - MBC6
  - MBC7 + acceleromter
  - MMM01
  - M161
  - HuC1
  - HuC-3
- Timer emulation
- PPU (Pixel Processing Unit) emulation
- APU (Audio Processing Unit) emulation
- SDL-based graphics output
- Keyboard and Gamepad input support
- Testing support (headless debug mode for mooneye/Blargg ROMs)
- Save game support with autosave on a background thread, so you don't loose your mewtwo even if you shutdown the computer
- M-Cycle accurate
- Link Cable support over TCP/IP
- Frontend UX:
  - Game title shown in the window title (`R2-Boy - <title>`)
  - Key remapping (`--remap` interactive prompt; persisted to `~/.config/r2boy/config.ini`)
  - Volume / mute control (runtime hotkeys + `--volume`/`--mute` flags)
  - Turbo / fast-forward (hold `Tab`; audio is muted while active)
  - Alternative color palettes (`--palette`, hotkey `P` to cycle)
- 2 Save State slots: save or load the Game Boy state just pressing a key
- Game Boy Printer support. It saves the printed image on a file.

---

## Current Status

R2-Boy works correctly and runs every DMG compatible game I tested.

This project is under active development.

Planned:
- Implement Game Boy Camera
- CGB (Game Boy Color) support (more in the future)

---

## Quick Start

### Requirements

- GCC
- Make
- SDL2
- SDL2_ttf

### Compile

Install dependencies (On Ubuntu/Debian like systems):

```bash
sudo apt install build-essential libsdl2-dev libsdl2-ttf-dev
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
./build/r2boy
```

### Usage

Run a Game Boy ROM:

```bash
./build/r2boy [OPTIONS] rom.gb
```

Common options:

| Flag | Purpose |
| :--- | :--- |
| `-d`, `--debug`              | Headless mode; verify CPU registers (test ROMs) |
| `-b`, `--bios <file>`        | Use a specific Game Boy boot ROM (default `roms/bios.bin`) |
| `--volume <0..100>`          | Set the audio output volume |
| `--mute`                     | Start with audio muted |
| `--palette <NAME>`           | Use a built-in palette: `DMG`, `pocket`, `BGB`, `choco`, `pocket_green` |
| `--remap`                    | Open the visual key-remap window and exit (saves on `S`, aborts on `ESC`/close) |
| `--link-host <PORT>`         | Host a Game Link session on the given TCP port |
| `--link-connect <IP>:<PORT>` | Connect to a Game Link host |
| `--printer`                  | Use the Game Boy Printer and save the printed images on a file |
| `-h`, `--help`               | Show help and exit |
| `-v`, `--version`            | Show version and exit |

---

## Controls

Default keyboard mapping (rebindable, see [Configuration](#configuration)):

| Key | Action |
| :---: | :---: |
| Arrow keys | D-Pad |
| X | A |
| Z | B |
| Enter | Start |
| Backspace | Select |
| Tab | Turbo (hold) |
| F1  | Save State 1 |
| 1 | Load State 1 |
| F2 | Save State 2 |
| 2 | Load State 2 |
| M | Mute toggle |
| 0 | Volume up |
| 9 | Volume down |
| P | Cycle color palette |

Gamepads are auto-detected via SDL GameController. The D-Pad, face buttons, Start/Back and analog stick map to the joypad by default; **all gamepad buttons are also rebindable** in the remap window. Rumble carts (`MBC5` with rumble) drive the controller's haptic. For the MBC7 cartridges with accelerometer, the controller's one will be used. In the case is not an accelerometer present, the right joystick or the WASD keys (also configurable) will simulate it.

---

## Palettes

R2-Boy ships five built-in DMG color palettes. The default reproduces the warm greenish tint of the original DMG LCD; the others mimic the Game Boy Pocket, the BGB emulator, a "chocolate" tint, and a cooler green pocket variant. Switch at startup with `--palette <NAME>` or at runtime by pressing `P` (the current palette is printed to stderr). The choice is persisted across runs in the config file.

| Name | Style |
| :--- | :--- |
| `DMG`           | Classic DMG (default) |
| `pocket`        | Game Boy Pocket monochrome |
| `BGB`           | BGB emulator palette |
| `choco`         | Warm brown tint |
| `pocket_green`  | Cool green pocket-like |
| `basic`         | Pure withe, black and grey |

---

## Configuration

R2-Boy keeps its config in `~/.config/r2boy/config.ini` (or `$XDG_CONFIG_HOME/r2boy/config.ini` if set). Four sections are written. It includes keyboard and gamepad mappings, volume control and palette.

Keyboard bindings support modifier chords written as `Ctrl+Shift+X` style tokens. Any combination of the prefixes `Ctrl`, `Shift`, `Alt`, `GUI` (also accepted: `Control`, `Option`, `Cmd`, `Super`, `Meta`) may precede a scancode name. A bare token like `X` parses as `mods=0` (matches any modifier state, the legacy behaviour). The special value `NONE` (or `—`) means no binding for that action.

The `--remap` flag opens a **visual SDL window** with a list of all 17 actions and their current keyboard + gamepad bindings. Keyboard keys are captured via SDL `SDL_KEYDOWN` (so layout-independent scancodes and modifier chords work); gamepad buttons are captured via `SDL_CONTROLLERBUTTONDOWN`.

```bash
./build/r2boy --remap
```

In the remap window:

| Key | Action |
| :---: | :--- |
| Up / Down | Select row |
| Enter | Capture keyboard binding for selected row |
| Tab | Capture gamepad binding for selected row |
| D | Reset selected row to default |
| S | Save and exit |
| ESC | Exit without saving |
| Window close | Exit without saving |

Runtime hotkey changes (volume, mute, palette) are written back to `config.ini` when the emulator exits, so the next launch picks them up.

---

## Save Games & Autosave

Battery-backed cartridges write their SRAM and RTC state to `<rom>.sav` next to the ROM, the same layout other DMG emulators use.

Saves run on a **dedicated background thread**:

- The emulator thread sets `cart->save_needed = 1` on every SRAM/RTC write and requests a save roughly every 5 seconds of in-game time (`AUTOSAVE_RATE_FRAMES`).
- A saver thread snapshots the SRAM and RTC under a short `pthread_mutex` critical section, then `fwrite`s the snapshot **outside** the lock so disk I/O never blocks emulation.
- The save file is written to `<rom>.tmp` first, `fflush`ed, `fclose`d, then atomically `rename`d over `<rom>.sav` — a crash or power loss mid-write never corrupts the previous save.
- A battery-less cart (MBC0, ROM only) skips starting the saver thread entirely.
- On shutdown, the saver thread is `pthread_join`ed and `cleanup_core` performs one final synchronous flush as a safety net.

Save games are interchangeable with other DMG emulators that follow the standard `.sav` layout.

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

In debug mode, video output is disabled and the emulator automatically checks the register values used by Mooneye test ROMs to determine whether the test passed.

---

## Link Cable

R2-Boy includes Link Cable support over TCP/IP, allowing two emulator instances to communicate across the network.

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
./build/r2boy --link-host 9999 tetris.gb
```

Client:

```bash
./build/r2boy --link-connect 192.168.1.25:9999 tetris.gb
```

Reconnection is supported; if the process terminates on either side, restarting it will re-establish the connection.

However, most games don't handle mid-game reconnection and will probably simply freeze.

---

## Accuracy

The emulator aims to emulate the Game Boy hardware at the cycle level whenever possible.

It currently passes all Blargg tests (including ```oam_bug```!), Mooneye and dmg-acid2. It also fails Blargg's ```cgb_sound``` and ```interrupt_time``` exacly like a real DMG.

---

## License

MIT

---

## Project Structure

```
src/
├── apu/
|   └── apu.c/.h
├── bus/
|   └── bus.c/.h
├── cartucho/
|   ├── mbc/
|   ├── cartucho.c/.h
|   ├── savestate.c/.h
|   └── save.c/.h
├── cpu/
|   ├── opcodes/
|   └── cpu.c/.h
├── frontend/
|   ├── frontend.c/.h
|   ├── lcd.c/.h
|   ├── audio.c/.h
|   ├── config.c/.h
|   ├── remap_ui.c/.h
|   ├── gamepad.c/.h
|   ├── printer.c/.h
|   └── link.c/.h
├── ppu/
|   ├── ppu.c/.h
|   ├── bg_fetcher.c/.h
|   ├── sp_fetcher.c/.h
|   └── types.h
├── serial/
|   └── serial.c/.h
├── timer/
|   └── timer.c/.h
├── gb.c
├── gb.h
└── main.c
```

---
