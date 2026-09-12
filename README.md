# LUDO 3D

Classic Ludo with two ways to play: a colorful terminal version and a full 3D/GUI
version built on SDL2. The board, pawns, dice, and even the HUD font are drawn
procedurally — there are no image files to download.

The 3D version gives you an orbitable camera, dice that actually tumble and land,
pawns that hop from tile to tile, and bots to play against. The rules are lifted
straight from `ludo.c`, so if you know the terminal version, this feels exactly
the same.

---

## Table of contents

- [Two versions](#two-versions)
- [3D version features](#3d-version-features)
- [Requirements](#requirements)
- [Build & run](#build--run)
  - [macOS](#macos)
  - [Linux](#linux)
  - [Windows (MSYS2)](#windows-msys2)
  - [Windows (WSL2)](#windows-wsl2)
- [Controls](#controls)
- [Extra modes for tinkering](#extra-modes-for-tinkering)
- [Quick rules](#quick-rules)
- [Project structure](#project-structure)
- [Troubleshooting](#troubleshooting)
- [Technical notes](#technical-notes)

---

## Two versions

| Binary | Source | Look | Platforms |
|---|---|---|---|
| `build/ludo` | `ludo.c` | Color terminal (ANSI) | macOS & Linux |
| `build/ludo-gui` | `ludo_gui.c` | 3D + SDL2 GUI | macOS, Linux, Windows |

The terminal version uses `termios.h` and `unistd.h`, which are POSIX-only, so it
won't compile on Windows. If you're on Windows, just use the 3D version — it has
everything the terminal one does.

---

## 3D version features

- Software 3D renderer on top of `SDL_Renderer` (`SDL_RenderGeometry` + painter's
  algorithm + flat shading). No OpenGL, no asset files.
- Orbit camera: drag to rotate, scroll to zoom.
- A 15x15 board matching the `map` array in `ludo.c`, complete with each team's
  yard, the finish area, and safe tiles.
- 3D pawns that hop along their path, animate out of the base, and fly back home
  when they get captured.
- A 3D die that spins and lands exactly on the rolled value.
- Bots for the other three teams, with the same turn flow as the terminal
  version (roll a 6 to leave base or roll again, capture for a bonus turn, etc).
- HUD: turn indicator, `n/4` score, match log, message banner, menu screen, exit
  confirmation, and a victory screen with confetti.
- A built-in 5x7 bitmap font, so SDL2_ttf is not required.
- Built-in test modes (see [Extra modes](#extra-modes-for-tinkering)).

---

## Requirements

Needed on every platform:

- A C compiler (gcc or clang)
- GNU make
- SDL2 with `sdl2-config` available

To confirm SDL2 is set up:

```sh
sdl2-config --version
```

If that prints a version number (say `2.32.70`), you're good to go. If you get
`command not found`, jump to [Troubleshooting](#troubleshooting).

---

## Build & run

### macOS

1. Install Homebrew if you don't have it yet (skip if you do):

   ```sh
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```

2. Install SDL2 and the toolchain:

   ```sh
   brew install sdl2
   xcode-select --install   # only if you don't have a compiler yet
   ```

3. From the project folder, build:

   ```sh
   make ludo-gui
   ```

4. Run it:

   ```sh
   make run-gui
   ```

If `sdl2-config` isn't found even though brew finished, make sure
`/opt/homebrew/bin` (Apple Silicon) or `/usr/local/bin` (Intel) is on your `PATH`:

```sh
echo 'export PATH="/opt/homebrew/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

The terminal version builds on macOS too:

```sh
make run      # builds + runs build/ludo
```

### Linux

Install the packages for your distro:

**Debian / Ubuntu / Linux Mint**

```sh
sudo apt update
sudo apt install build-essential libsdl2-dev
```

**Fedora**

```sh
sudo dnf install gcc make SDL2-devel
```

**Arch / Manjaro**

```sh
sudo pacman -S base-devel sdl2
```

**openSUSE**

```sh
sudo zypper install gcc make libSDL2-devel
```

Then just:

```sh
make ludo-gui
make run-gui
```

Terminal version:

```sh
make run
```

### Windows (MSYS2)

The easiest, least painful way on Windows is MSYS2. Don't use Command Prompt or
PowerShell directly — use the MSYS2 shell.

1. Download and install MSYS2 from <https://www.msys2.org>.
2. Open **MSYS2 MINGW64** from the Start Menu (important: MINGW64, not the plain
   MSYS2 shell, so the compiler and SDL2 match).
3. Update packages and install what you need:

   ```sh
   pacman -Syu
   # if it asks you to close the terminal, close it and reopen MINGW64
   pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 make
   ```

4. `cd` into the project folder. For example, if it lives in `D:\code\ludo`:

   ```sh
   cd /d/code/ludo
   ```

5. Build the 3D version (don't run plain `make`, since the terminal version can't
   be built on Windows):

   ```sh
   make ludo-gui
   ```

6. Run it from the same shell:

   ```sh
   ./build/ludo-gui.exe
   ```

Windows notes:

- Run the game from the **MSYS2 MINGW64 shell** so it can find `SDL2.dll`. If you
  want to double-click from Explorer, copy `C:\msys64\mingw64\bin\SDL2.dll` into
  the `build\` folder next to the `.exe`.
- MinGW automatically appends `.exe` to the output name, so the result is
  `build/ludo-gui.exe`.
- If `make` insists on rebuilding even when nothing changed, force the explicit
  target:

  ```sh
  make ludo-gui GUI_TARGET=build/ludo-gui.exe
  ```

- `--selftest` output won't show on Windows because the SDL2 build uses GUI mode
  (`-mwindows`) and has no console. The game itself runs fine; selftest is just
  more comfortable on macOS/Linux.

### Windows (WSL2)

If you'd rather stay in Linux while on Windows:

1. Install WSL2 + an Ubuntu distro, then open its terminal.
2. Make sure you're on Windows 11 (or Windows 10 with WSLg) so GUI windows can
   appear without an extra X server.
3. Install dependencies and build like on Linux:

   ```sh
   sudo apt update
   sudo apt install build-essential libsdl2-dev
   make ludo-gui
   make run-gui
   ```

If no window shows up, first check whether WSLg is active with `echo $DISPLAY`.
If it's empty, run `wsl --update` from PowerShell and restart WSL.

---

## Controls

| Action | Keyboard / Mouse |
|---|---|
| Rotate camera | Drag the mouse |
| Zoom in/out | Mouse scroll |
| Roll the dice | `R` or `Space`, or click **ROLL DICE** |
| Pick a pawn | Click a glowing pawn, or press `1`–`4` |
| Set pawn free / Move | Click the button, or `F` (free) / `M` (move) |
| Menu / cancel | `ESC` |
| Confirm exit | `Y` / `N` or click a button |

Tip: selectable pawns glow and bob up and down. Hover the mouse over one and it
scales up so you know exactly what you're about to click.

Terminal version controls: `W`/`S` to move through the menu, `Enter` to select,
`1`–`4` to choose a pawn, `R` to roll.

---

## Extra modes for tinkering

All of these belong to the 3D binary (`build/ludo-gui`). Run it with `--help` for
the full list.

```sh
./build/ludo-gui --help
```

| Option | What it does |
|---|---|
| `--selftest [N]` | Play N matches (default 200) with 4 bots, no window. Great for testing the logic. |
| `--selftest-human [N]` | Same as above, but the human input path is exercised automatically too. |
| `--autoplay` | Watch 4 bots play in the 3D window. |
| `--speed X` | Speed up time by X (e.g. `--speed 8`). |
| `--frames N` | Quit automatically after N frames. |
| `--fixed-dt MS` | Use a fixed timestep per frame; handy for consistent screenshots. |
| `--screenshot FILE` | Save a BMP screenshot and exit. |
| `--shot-frame N` | Choose which frame to capture. |

Examples:

```sh
# test the logic over 300 matches
./build/ludo-gui --selftest 300

# test the human flow over 1000 matches
./build/ludo-gui --selftest-human 1000

# watch bots play at 8x speed
./build/ludo-gui --autoplay --speed 8

# capture a mid-game screenshot (deterministic)
SDL_VIDEODRIVER=dummy ./build/ludo-gui --autoplay --fixed-dt 16 --speed 8 \
  --screenshot game.bmp --shot-frame 900
```

> `SDL_VIDEODRIVER=dummy` is only used when you don't want a window to appear
> (on a server or in CI, for example). For normal use, just leave it out.

---

## Quick rules

The rules follow classic Ludo exactly:

- Roll a **6** to bring a pawn out of the base, and you get to roll again.
- Pawns already on the board move by the rolled amount.
- Tiles with a special mark are safe — pawns there can't be captured.
- Land on a tile with an opponent's pawn and it goes back to base while you get a
  bonus turn (roll again).
- A pawn that has entered the finish area can't be moved anymore.
- The first player to get **4 pawns** home wins.

Your color and who goes first are randomized each game. Bots play the other three
teams.

---

## Project structure

```
ludo/
├── ludo.c        # terminal version (POSIX, text-only)
├── ludo_gui.c    # full 3D/GUI SDL2 version
├── Makefile      # two targets: ludo and ludo-gui
├── build/        # build output (not tracked by git)
└── README.md
```

Makefile commands you'll use most:

```sh
make            # build both
make ludo-gui   # build the 3D version only
make ludo       # build the terminal version only (macOS/Linux)
make run        # build + run the terminal version
make run-gui    # build + run the 3D version
make clean      # remove the build folder
```

---

## Troubleshooting

**`sdl2-config: command not found`**

- macOS: `brew install sdl2`, then check your `PATH` (see the macOS section).
- Debian/Ubuntu: `sudo apt install libsdl2-dev`.
- Fedora: `sudo dnf install SDL2-devel`.
- Arch: `sudo pacman -S sdl2`.
- Windows: make sure you're in the **MSYS2 MINGW64** shell and have installed
  `mingw-w64-x86_64-SDL2`.

**`gcc: command not found` / `make: command not found`**

- macOS: `xcode-select --install`.
- Debian/Ubuntu: `sudo apt install build-essential`.
- Windows: install the `mingw-w64-x86_64-gcc` and `make` packages in MSYS2.

**The terminal version fails to build on Windows**

That's expected. `ludo.c` uses `termios.h`/`unistd.h`, which are POSIX-only. Use
the 3D version instead (`make ludo-gui`).

**The game runs but the window is black / doesn't show on WSL**

Make sure WSLg is active (Windows 11 or an updated WSL). Try `wsl --update` from
PowerShell, then restart. Alternatively, run through an X server like VcXsrv and
set `DISPLAY` to `:0`.

**The window opens but I can't see the whole board**

Drag to rotate and scroll to zoom. If you resize the window, the image adapts
automatically because the renderer uses a logical resolution with
`SDL_RenderSetLogicalSize`.

**It feels a bit heavy on an old laptop**

This renderer is pure software 3D. On modern machines it's smooth. If you need
to, lower the pawn segment counts or buffer sizes near the top of `ludo_gui.c`,
or turn off VSync (remove `SDL_RENDERER_PRESENTVSYNC`).

**The screenshot comes out empty**

Run with `--fixed-dt` and a large enough `--shot-frame`, for example:

```sh
SDL_VIDEODRIVER=dummy ./build/ludo-gui --autoplay --fixed-dt 16 \
  --screenshot shot.bmp --shot-frame 900
```

**`'SDL2/SDL.h' file not found` when building from the Makefile**

The code includes SDL portably:

```c
#if __has_include(<SDL2/SDL.h>)
#  include <SDL2/SDL.h>
#else
#  include <SDL.h>
#endif
```

`make` uses the flags from `sdl2-config` (`-I.../include/SDL2`), while editors
like VSCode/clangd often use `-I.../include`. Both work. If you added a custom
`compile_flags.txt`, point it at your SDL2 include directory.

---

## Technical notes

- **Renderer.** Every frame collects geometry, sorts triangles by depth
  (painter's algorithm), and sends them to `SDL_RenderGeometry` with per-vertex
  colors. Lighting is simple: each triangle's normal is dotted with a light
  direction, plus an ambient term.
- **No assets.** The board is generated from the same 15x15 `map` array as the
  terminal version. Pawns are combinations of cylinders and a sphere; the die is
  a cube with pip discs attached to each face.
- **Font.** A 5x7 bitmap font is written directly in the source as ASCII art and
  parsed at startup. Lowercase letters map to uppercase automatically.
- **Game logic.** Functions like `pergerakan`, `bidakMenang`, `eliminasi`, and
  `cekKemenangan` are ported as-is. One inherited bug from the terminal version
  is fixed: when a 6 is rolled but every pawn has already finished, the old
  version hung forever inside `ambilPawn`, while the 3D version safely skips the
  turn.
- **State machine.** The terminal turn flow used to be blocking (`getch` and
  `Sleep`). It's now a time-based state machine, so animations, mouse input, and
  bots can all run together.

---

## License

This project was built for learning. Use it, modify it, and share it freely. If
you build something cooler on top of it, let me know — you might inspire someone
else.
