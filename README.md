# OpenJazz Pocket

Jazz Jackrabbit 1 running natively on the Analogue Pocket through openfpgaOS.
This is a port of [OpenJazz](https://github.com/AlisterT/openjazz), built
with the [openfpgaOS SDK](https://github.com/openfpgaOS/openfpgaSDK).

## Install

1. Unzip the release ZIP to the root of your Pocket's SD card.
2. Build `jazz.iso` from your own Jazz Jackrabbit 1 files:
   ```
   tools/mkjazz.sh <folder-with-your-jazz-files> jazz.iso
   ```
   Don't have the game? The shareware episode is freely distributable;
   `tools/fetch-shareware.sh` downloads and unpacks it for you.
3. Copy `jazz.iso` to `Assets/openjazz/common/` on the SD card.
4. Launch OpenJazz from the openFPGA menu.

This port ships no game data; see Credits below.

## Controls

| Pocket button | Action |
|---|---|
| D-Pad | Move / navigate menus |
| A | Fire / Enter / Yes |
| B | Jump / Swim / No |
| X | Change weapon |
| Y | Pause |
| L | Select blaster |
| R | Select toaster |
| Select | Stats screen |
| Start | Escape / menu |

## Display

Fixed 320x288, the Pocket's 1600x1440 panel at an exact 5x integer scale
with square pixels. The original ran at 320x200 on a 4:3 screen, so menu
and cutscene art looks slightly flatter here. The trade-off buys a
full-screen picture with no black bars.

## Saves

The game's own save slots work as in the original and live in the core's
nonvolatile save files.

## Build from source

Needs Docker (the RISC-V toolchain runs in a container) and, for
`make test`, SDL2.

```
git clone --recursive https://github.com/negenii/openJazz_pocket.git
cd src/openjazz
make            # build app.elf, stage build/pocket/openjazz/ (includes jazz.iso if present)
make copy       # copy to the Pocket SD card
make package    # zip a release under releases/pocket/ (no game data)
make test       # desktop SDL2 build, for fast iteration
```

See [docs/SDK-README.md](docs/SDK-README.md) for the SDK's own documentation.

## Credits

- [OpenJazz](https://github.com/AlisterT/openjazz) by Alister Thomson and
  contributors, GPL-2.0.
- [openfpgaOS SDK](https://github.com/openfpgaOS/openfpgaSDK) by
  ThinkElastic, Apache-2.0.
- Jazz Jackrabbit is (c) Epic MegaGames. This port ships no game data.
