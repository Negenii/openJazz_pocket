# OpenJazz Pocket

Jazz Jackrabbit 1 running natively on the Analogue Pocket through openfpgaOS.
This is a port of [OpenJazz](https://github.com/AlisterT/openjazz), built
with the [openfpgaOS SDK](https://github.com/openfpgaOS/openfpgaSDK).

## What you need

- An Analogue Pocket on firmware 2.2 or newer, with openFPGA enabled.
- An SD card.
- The data files of Jazz Jackrabbit 1 obtained legally. This
  core contains no game data.

Legal sources for the data files:

- Your own copy of the game, bought from GOG or Steam, or installed from
  original media. Point the tool below at the folder the game lives in.
- The shareware episode, which Epic MegaGames released for free
  distribution. `tools/fetch-shareware.sh` downloads and unpacks it.

Whichever you use, the folder must contain the game's own data files. The
packaging tool checks for `LEVEL0.000`, `MENU.000`, `FONTS.000` and
`PANEL.000` and refuses to build if they are missing, so a wrong folder
fails immediately rather than producing a broken image.

## Install

1. Unzip the release ZIP to the root of your Pocket's SD card. It adds
   `Cores/negenii.OpenJazz/`, `Assets/openjazz/` and
   `Platforms/openjazz.json`; merge with the folders already there.
2. Build the data image from your game files:
   ```
   tools/mkjazz.sh <folder-with-your-jazz-files> jazz.iso
   ```
   This reads your files and writes a single ISO image. Nothing leaves your
   machine, and the image is not something you should redistribute.
3. Copy `jazz.iso` to `Assets/openjazz/common/` on the SD card, next to
   `openjazz.elf`.
4. Eject the card, boot the Pocket, and launch OpenJazz from the openFPGA
   menu.

Saves appear by themselves on first use. If the game data is missing or the
image did not mount, the screen says so instead of failing silently.

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
and cutscene art looks slightly flatter here. I'm just not exactly keen on
black bars in game.

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

## About this repository

This is a fork of the openfpgaOS SDK, which is how cores for that runtime
are built: the SDK sits at the top level and the port lives in
`src/openjazz/`. OpenJazz itself is a submodule at
`src/openjazz/openjazz/`, on a branch carrying one added platform file.
The SDK's own documentation is in [docs/SDK-README.md](docs/SDK-README.md).

Directories worth knowing:

| Path | What it holds |
|---|---|
| `src/openjazz/` | the port: build rules and the SDL compatibility layer |
| `src/openjazz/openjazz/` | OpenJazz, as a submodule |
| `dist/openjazz/` | the core's static files: JSON, icon, platform art |
| `tools/mkjazz.sh` | builds `jazz.iso` from your game files |
| `tools/mkart.py` | converts the core icon and platform banner to and from PNG |
| `docs/RELEASING.md` | how a release and a catalogue listing are made |

## Licence

OpenJazz is GPL-2.0, so the built core is GPL-2.0 as well, and the sources
this port adds are offered under the same terms. The openfpgaOS SDK parts
of this tree remain under their own Apache-2.0 licence, and the prebuilt
runtime under `runtime/` carries the third-party terms described in
[NOTICE](NOTICE).

## Credits

- [OpenJazz](https://github.com/AlisterT/openjazz) by Alister Thomson and
  contributors, GPL-2.0.
- [openfpgaOS SDK](https://github.com/openfpgaOS/openfpgaSDK) by
  ThinkElastic, Apache-2.0.
- Jazz Jackrabbit is a trademark of Epic Games, and the game is (c) Epic
  MegaGames. This project is not affiliated with or endorsed by them, ships
  no game data, and uses the name only to say which game it plays.

## Important note

- RABBITS STINK
