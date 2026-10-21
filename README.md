# mazeworld-duo

Mazeworld and Mazeworld Catacombs ported together for Windows (work in progress: Catacombs is playable).

Goal: tick-exact C reimplementations of Farfetch Software's two Mazeworld games for Windows,
built on one shared engine, verified against the original 68k code running in a test harness:

- *Mazeworld Catacombs* 1.0.5 (1994–97, freeware)
- *Mazeworld: The Ghost of Mordaine* ("Mazeworld Original") 1.0.3 (1994, shareware)

Both licenses allow free distribution "in its original, unmodified form, complete with any
accompanying documentation/files", so the port can ship the untouched archives and reads everything
from them at run time. A user can also point it at their own copy: `.hqx`, `.sit` (StuffIt 1.5–5,
including Arsenic compression) or MacBinary. Extracted or converted assets are never committed or
redistributed.

## Native, not emulated

The shipped game is native code: the game logic rewritten in C, compiled for x86-64 Windows,
with SDL2 for window, input and audio, plus `mactb/`, a native C implementation of the handful
of Toolbox routines the game uses (QuickDraw subset, Palette Manager, SANE floating point, text, sound).
It runs no 68k code, needs no Mac ROM and contains no emulator.

`harness/` is a development-only test rig. It runs the *original* 68k code under Musashi so the
rewrite can be checked against it tick by tick. The release build never links it.
Both the harness and the port call the same `mactb` code, so a mismatch is always a game-logic bug.

## Layout

| Path | What |
|---|---|
| `original/` | The untouched archives: `mazeworld-catacombs-105.hqx` (SHA-1 `933399020cd16bfce84651bbd82da77a0efb131f`), `Mazeworld_v.1.0.3.sit` (SHA-1 `d7bb9b7cb3513d4306a7ab24c77d08252d82e319`) |
| `archive/` | Native reader for `.hqx`, `.sit`, StuffIt 5 and MacBinary (`arcdump` tool) |
| `tools/` | Python tools: BinHex decoder, resource-fork parser, 68k disassembler, trap tables |
| `mactb/` | Native C Toolbox subset shared by the port and the harness |
| `game/` | The C reimplementation (`seg<N>.c`), its header `mz.h`, and `subs.txt` (what is done) |
| `harness/` | Test-only harness that runs the original 68k code under Musashi with a Mac OS HLE |
| `third_party/` | Musashi (68k CPU emulator for the test rig, its own MIT-style license); `sdl2-mingw/` is fetched by `tools/get_sdl2_mingw.sh` |
| `docs/` | Findings and plan (`docs/STATUS.md`) |
| `build/` | Generated: extracted forks, disassembly, traces, logs (not committed) |

## Quick start (Linux / WSL / MSYS2)

```sh
pip install capstone pyyaml        # disassembler + interface parser
tools/extract.sh                   # both archives -> build/games, build/res (project's own reader)
python3 tools/build_traps.py       # trap name table (once)
python3 tools/gen_trapsigs.py      # Pascal trap signatures (only to regenerate harness/gen/trapsigs.h;
                                   # needs git clone https://github.com/autc04/multiversal third_party/multiversal)
tools/run_harness.sh catacombs     # boot a game, trace it, refresh build/disasm
tools/run_harness.sh original -script tests/mwo_start.txt -ticks 400 -idle 100000   # -> build/disasm-original
# the harness takes the archive itself; e.g. play a scripted game, saving the screen every 3 frames:
harness/mzharness -o build/run -script tests/explore.txt -ticks 640 -shots 3 original/mazeworld-catacombs-105.hqx
```

Recording a test by playing: `make -C harness mzplay` (or `mzplay.exe` with MinGW), then
`harness/mzplay -o build/out -play tests/recordings/mine.txt original/Mazeworld_v.1.0.3.sit`
runs the original game in a window and records what you do; the last line of the recording says
which `-ticks` replays it exactly.

Input scripts (`tests/*.txt`) drive the harness: `<tick> down|up|tap <key>`, `<tick> type <text>`,
`<tick> move|click x y`, `<tick> mousedown|mouseup`. `tools/gen_monkey.py <seed> <ticks>` makes random-play
scripts for coverage; `tests/highscore.txt` ends a game with a new high score and types a name into the dialog.

Verify the C functions against the original (identical globals, heap, screen and audio):

```sh
tools/verify_subs.sh all tests/explore.txt 640
```

Outputs:

- `build/disasm/` (Catacombs) and `build/disasm-original/` — `seg<N>.s` annotated listing per CODE segment; `*` marks instructions executed in a
  recorded run. Column 1 marks provenance:
  blank = proven by control flow, `t` = proven by an emulator trace, `l` = reached only from a
  `LINK A6` scan, `?` = unproven linear sweep.
- `build/disasm/jumptable.txt`, `functions.json`, `globals.txt`
- `build/run/harness.log`, `build/run/traps.txt` — every trap called, implemented or stubbed

## The Windows game (`port/`)

```sh
tools/get_sdl2_mingw.sh   # once: SDL2 2.30.9 MinGW development package -> third_party/sdl2-mingw
make -C port win          # port/Mazeworld.exe (MinGW cross-compiler; on MSYS2: make -C port win WCC=gcc WINDRES=windres)
make -C port              # port/mzport (Linux, SDL2) and port/mzport-headless
tools/verify_port.sh      # the port against the original on all 11 scripted games (must be identical)
```

`Mazeworld.exe` is a single file. To play, put it next to the original archive (any `.hqx`,
`.sit` or `.bin` containing Mazeworld Catacombs), or drop the archive onto it.

- **Controls:** W / S forward and back, A / D sideways, the mouse turns (sideways movement), left
  click or Space fires; the arrow keys are the classic controls (up / down forward and back,
  left / right turn); Q / E turn. The original's keypad and letter keys (Q W E, A D, X) all still
  work. Escape leaves play. Ctrl stands in for the Mac's Command key (Ctrl+G play, Ctrl+N new
  game, Ctrl+1 to Ctrl+4 view size, Ctrl+Q quit). Alt+Enter toggles fullscreen.
- **Mazeworld Original** (once supported) gets the same keys and mouse turning; left click fires the
  chosen ability and 1 to 4 choose it. Options > Classic Mouse Controls brings back its own: the
  mouse moves and turns, and a drag with the button held picks the ability, fired on release.
- **Menus:** the original's (Help > About is its instructions), plus Options > Volume and
  Options > Legacy (the original Options dialog's three speed settings for 1990s Macs).
- **Preferences:** the game's preferences and high scores are kept in
  `%APPDATA%\Mazeworld\Mazeworld Catacombs\`, in the Mac's own format; the port's settings
  (volume, window) beside them in `port settings.txt`.

## Open source

The project is free and open source. Nothing from the original games is in this repository except the
two untouched archives, which their licenses allow anyone to redistribute; everything else (the
decompiled game, the Toolbox layer, the archive reader, the tools) is new code.

## Licensing

Project code is MIT. `archive/sit13.c`, `archive/sit13_tables.h` and `archive/arsenic.c` are ports of
The Unarchiver's XADMaster decoders and stay under LGPL-2.1 in their own files. Musashi and
multiversal are MIT-style; the StuffIt container code was written from Deark's (MIT) format notes.

## Naming rule

Functions are `fn_S<segment>_<offset>` and globals `G_<offset below A5>` until a name is certain.
Only exact, evidence-backed names get assigned.
