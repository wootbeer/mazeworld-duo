# Building

## Windows game

Built with MinGW, from Linux, WSL or MSYS2.

```sh
tools/get_sdl2_mingw.sh   # once: SDL2 for MinGW -> third_party/sdl2-mingw
make -C port win          # port/Mazeworld.exe
```

On MSYS2: `make -C port win WCC=gcc WINDRES=windres`.

`Mazeworld.exe` is a single file. Put the game archives next to it (see the README).

## Linux

```sh
make -C port              # port/mzport (needs SDL2) and port/mzport-headless
```

## Tests

The test rig in `harness/` runs the original 68k code under Musashi, so the C code can be checked
against it. The game itself never uses the rig.

```sh
make -C harness
tools/verify_all.sh quick         # Catacombs: every C function, call by call and end to end
tools/verify_original.sh          # the same for Mazeworld Original
tools/verify_port.sh [quick|full] # the port against the original game (must be identical)
```

A test is a script of timed input (`tests/*.txt`):
`<tick> down|up|tap <key>`, `<tick> type <text>`, `<tick> move|click x y`, `<tick> mousedown|mouseup`,
`<tick> menu <menu> <item>`, and `setg` / `setr` to set a game up. `# level N` on a line of its own
starts Mazeworld Original on level N.

To record a test by playing: `make -C harness mzplay`, then
`harness/mzplay -o build/out -play tests/recordings/mine.txt original/Mazeworld_v.1.0.3.sit`.
The last line of the recording says which `-ticks` replays it.

## Layout

| Path | What |
|---|---|
| `game/` | The games in C. `seg*.c` is Catacombs (shared with Original where the code is the same), `orig_*.c` Original's own code |
| `mactb/` | The parts of the Mac Toolbox the games use (QuickDraw, palettes, text, sound, SANE), shared by the game and the test rig |
| `port/` | The Windows / Linux program: SDL2 window, input, sound and the native menus |
| `archive/` | Reads `.hqx`, `.sit` (StuffIt 1.5-5) and MacBinary |
| `harness/` | The test rig (development only) |
| `tests/` | Test scripts and recordings |
| `tools/` | Test scripts, the disassembler and other helpers (Python) |
| `docs/` | Notes on how it all works |

See [docs/NOTES.md](docs/NOTES.md) for how the code is checked and named.
