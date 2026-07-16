# R2-Boy - DMG Emulator

A Nintendo Game Boy (DMG-01) emulator written in C for educational purposes focusing on learning how the original hardware works.

---

## Features

- DMG (Original Game Boy) emulation
- CPU instruction emulation (including halt-bug, OAM bug)
- Memory bus implementation
- Cartridge loading (MBC1, MBC2, MBC3 + RTC, MBC5, rumble)
- Timer emulation
- PPU (Pixel Processing Unit) emulation
- APU (Audio Processing Unit) emulation
- SDL-based graphics output
- Keyboard and Gamepad input support
- Testing support (headless debug mode for mooneye/Blargg ROMs)
- Save game support with **autosave on a background thread** (atomic `.tmp` + `rename`)
- M-Cycle accurate
- Link Cable support over TCP/IP
- Frontend UX:
  - Game title shown in the window title (`R2-Boy - <title>`)
  - Key remapping (`--remap` interactive prompt; persisted to `~/.config/r2boy/config.ini`)
  - Volume / mute control (runtime hotkeys + `--volume`/`--mute` flags)
  - Turbo / fast-forward (hold `Tab`; audio is muted while active)
  - Alternative color palettes (`--palette`, hotkey `P` to cycle)
- Thread-safe autosave: SRAM/RTC writes are mutex-protected so the background saver never observes a torn snapshot

---

## Current Status

R2-Boy works correctly and runs every DMG compatible game I tested.

It won't run games with rare MBC types, due to MBC6, MBC7 and some others are not yet implemented

This project is under active development.

Planned:
- Implement more MBC types
- Save States
- CGB (Game Boy Color) support (more in the future)

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
| `--remap`                    | Run the interactive key-remap prompt at startup and exit |
| `--link-host <PORT>`         | Host a Game Link session on the given TCP port |
| `--link-connect <IP>:<PORT>` | Connect to a Game Link host |
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
| M | Mute toggle |
| 0 | Volume up |
| 9 | Volume down |
| P | Cycle color palette |

Gamepads are auto-detected via SDL GameController; the D-Pad, face buttons, Start/Back and analog stick map to the joypad. Rumble carts (`MBC5` with rumble) drive the controller's haptic.

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

---

## Configuration

R2-Boy keeps its config in `~/.config/r2boy/config.ini` (or `$XDG_CONFIG_HOME/r2boy/config.ini` if set). Three sections are written:

```ini
[audio]
volume = 75
muted  = 0

[video]
palette = pocket

[keymap]
right    = RIGHT
left     = LEFT
up       = UP
down     = DOWN
a        = X
b        = Z
start    = RETURN
select   = BACKSPACE
turbo    = TAB
mute     = M
palette  = P
vol_up   = EQUALS
vol_down = MINUS
```

The `--remap` flag enters an interactive prompt in the terminal (raw cbreak mode, no SDL window required) that walks through each key and writes the result back to `config.ini`:

```bash
./build/r2boy --remap
```

Runtime hotkey changes (volume, mute, palette) are written back to `config.ini` when the emulator exits, so the next launch picks them up.

---

## Save Games & Autosave

Battery-backed cartridges (MBC1/2/3/5 with battery; MBC3 + RTC) write their SRAM and RTC state to `<rom>.sav` next to the ROM, the same layout other DMG emulators use.

Saves run on a **dedicated background thread**:

- The emulator thread sets `cart->save_needed = 1` on every SRAM/RTC write and requests a save roughly every 5 seconds of in-game time (`AUTOSAVE_RATE_FRAMES`).
- A saver thread snapshots the SRAM and RTC under a short `pthread_mutex` critical section, then `fwrite`s the snapshot **outside** the lock so disk I/O never blocks emulation.
- The save file is written to `<rom>.tmp` first, `fflush`ed, `fclose`d, then atomically `rename`d over `<rom>.sav` — a crash or power loss mid-write never corrupts the previous save.
- A battery-less cart (MBC0, ROM only) skips starting the saver thread entirely.
- On shutdown, the saver thread is `pthread_join`ed and `cleanup_core` performs one final synchronous flush as a safety net.

Save games are interchangeable with other DMG emulators that follow the standard `.sav` layout (RAM followed by 5 bytes of RTC + 8-byte base timestamp for RTC carts).

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
./build/r2boy --link-host 5000 tetris.gb
```

Client:

```bash
./build/r2boy --link-connect 192.168.1.25:5000 tetris.gb
```

Reconnection is supported; if the process terminates on either side, restarting it will re-establish the connection.

However, most games don't handle mid-game reconnection and will simply freeze.

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
├── bus/
├── cartucho/
├── cpu/
├── frontend/
├── ppu/
├── serial/
├── timer/
├── gb.c
├── gb.h
└── main.c
```

---
