# Status — Mazeworld Original, step 3 (in progress)

- Where the games differ, the C code now handles both: a shared function checks `mz_original`
  for Original's version (subs.txt `both`: the menus, screen sizes, loading the level's graphics,
  movement without sliding, the frame's timing — Original draws at most 15 frames a second —, the
  sprite pass with Original's table-driven projection, the play events with the mouse button),
  and Original's own routines are `fo_S<seg>_<off>` in `game/orig_*.c` (subs.txt `orig`): the
  wall outline, the 45 object models and their dispatcher, the panel (frames, the two gauges,
  the mood labels, the message box with the texts of segment 6), mouse steering and the mood,
  saving and restoring the level's objects and the player. Original's own globals are named by
  its offsets (`og16(0x56DA)`).
- `tools/orig_todo.py [--ready]` lists what Original's runs execute that no C covers yet (ready:
  everything it calls is C already); `tools/quick_original.sh [test ticks]` is a fast per-call
  check; `tools/verify_original.sh` the full one (per call and end to end).
- The map (`tools/map_builds.py`): globals that only near pairs show can be wrong; the ones found
  wrong are fixed in its MANUAL table (and the rig stops when C code reaches a Catacombs global
  Original lacks). Identical twin functions (the same code on other tables) are left unpaired.
- Left to do: the play loop (fn_S8_0E2A, fn_S8_0000), the creatures' behaviour (segments 9, 10,
  11), sound (Original's own, segment 4 / 9 driver calls), the level change and save files.
  For the port: Original's projection reads the word at low memory $30 (a bug: an exception
  vector, in the rig the fault gate's address), so the port's low memory must hold the same.

## Step 2

- Shared code: `tools/map_builds.py` pairs Catacombs' functions with Original's and, inside each
  pair, the A5 globals, jump-table slots and callees that only moved; it writes `game/orig_map.h`.
  For Original, `GADDR` translates the C code's global offsets (`mz_gmap`: each offset moves with
  the nearest global at or above it), `mz_jt` the jump-table addresses, and `mz_seg_of` loads the
  segment the called function is in. A translated function runs for Original when it and all its
  callees match exactly; `tools/verify_original.sh` checks them per call and end to end on
  Original's tests and recordings. `tools/diff_pair.py fn_Sx_xxxx` shows how a pair differs.

## Step 1

- Original runs in the rig through the shell and play: `tests/mwo_monkey_21..24.txt` (random play
  by mouse, 14400 ticks each) and `tests/mwo_shell.txt` (window sizes, Options, About, Read
  Instructions, Register with a wrong code, New Game, Save Game As, Load Game) run without a
  missing Toolbox call. Code reached: 42% (segments 4, 6, 9 and 11, creature behaviours of later
  areas, not yet).
- Shared rig and port additions: relative mouse movement in scripts (`mmove dx dy`, MTemp /
  RawMouse as the mouse driver moves them); menu choices (`menu M I`: a click on the menu bar,
  MenuSelect answers it; the port's native menu uses the same path, so the original's own menu
  code runs); real alerts (Alert / StopAlert / NoteAlert / CautionAlert from ALRT + DITL, waiting
  for a button); Tab between edit fields; DrawChar; SetWTitle; Standard File (CustomPutFile /
  CustomGetFile: the rig answers deterministically, the port will show the Windows Save / Open
  dialogs and mirror the chosen file).
- The playable test build, `mzplay` (`make -C harness mzplay` / `mzplay.exe`): the rig running the
  original game in a window you can play (F1 captures the mouse); everything is recorded as a test
  script with the tick it happened on (`<tick> key <code> <char> down|up|auto`, `mmove`, `move`,
  `mousedown` / `mouseup`), and closing the window ends the run at the next tick, so the recording
  replays exactly (checked: live and replayed globals, heap and screen identical, also with the
  Windows build). Script steps now take effect when their tick begins. Recordings go in
  `tests/recordings/`.
- A port fix found this way: alerts never showed in the port (Clear High Scores silently
  answered Cancel). `tests/shell_menus.txt` covers Catacombs' menus and its alert.

# Step 7: the native port

- The whole of Mazeworld Catacombs now runs as C. `port/` builds it without any 68k code or
  emulator: the game's C code over the same Toolbox layer as the test rig (`harness/` traps, heap,
  resources, files, QuickDraw, dialogs, sound; `mactb/`), with a plain register file in place of
  the CPU (`port/rt.c`) and the VBL tasks and sound callbacks called directly (`port/vbl.c`).
- `port/mzport-headless` runs a test script and writes the same files as the rig.
  `tools/verify_port.sh` runs all 11 scripted games through it and through the original under the
  rig: globals, heap, screen and audio are byte-identical in every one. The Windows build of the
  same program (MinGW) gives the same bytes under Wine.
- `port/Mazeworld.exe` (`make -C port win`) is the Windows game: SDL2 window (largest whole
  multiple of 640x480 that fits; Alt+Enter fullscreen), keyboard (Ctrl is the Command key, Alt the
  Option key), mouse, sound (the game's 22254.5 Hz output), 60 Hz ticks, the preferences file in
  the user's application-data folder (same bytes as the Mac's), and a native Windows menu bar.
  `port/mzport` is the same for Linux.
- The menu bar (`port/shell.c`, `docs/SHELL.md`): every original item, greyed and checked as the
  original's menus would be; a chosen item reaches the game as an event, like a Mac menu choice,
  and runs through the game's own handlers. Quit (and the window's close box) is the 'quit' Apple
  event, so it also ends play the way the original does. Switching away from the window sends the
  suspend event (play stops, as on a Mac); coming back sends resume.
- Found while bringing it up, fixed in the shared Toolbox layer (so the rig has them too): file
  reference numbers are FCB offsets (`fn_S3_0334` divides by FSFCBLen), FSpExchangeFiles,
  high-level events need highLevelEventMask, ExitToShell stops the run.

- The instructions (Help > About, Ctrl+/) are translated (`game/seg8_help.c`: `fn_S8_190C`, `1A9C`,
  `1B0E`, `1D5C`, `1ED0`, `1F54`) and checked against the original by `tests/instructions.txt`
  (keys) and `tests/instructions_mouse.txt` (buttons). For them the rig gained a Menu Manager
  (`harness/menu.c`, from the game's MBAR / MENU resources, so Command-keys reach the original's
  menu code), PICT drawing (`mactb/pict.c`), dialog filter procedures and user items, Show /
  HideControl, and calls from a trap into the game (`hx_call_pascal`: a nested 68k call in the
  rig, the C function in the port). The per-call checker now also saves and restores the rig's
  loaded-resource table, and checks C functions called from inside a trap.
- The rig's FindWindow (a click's window and part), so the original's own mouse handling runs under
  test: `tests/shell_clicks.txt` (play started by a click, the hidden corner toggle G_22C9) and
  `tests/shell_keys.txt` (view size, Play Game and New Game by Command-key) match the original. This
  found a port bug: clicks in the high-score window were located with the wrong rectangle.
- Port front end: Options > Volume (100 / 75 / 50 / 25 % / mute), the window's size, position and
  fullscreen remembered, all in `port settings.txt` beside the game's preferences (which stay
  byte-for-byte the Mac's); a Game menu (Mazeworld Original listed, greyed until supported; an
  Original archive given to the program says so); version information in `Mazeworld.exe`.
- The shell keeps the game's menu records as the original does (`fn_S8_2E10`, `2E4C`, `2FB4`,
  `3190`, `3278` translated) and runs a command only if the record has it enabled.

# Step 6: dialogs, register-exact assembly, per-call checking

- Dialog Manager in the harness (high-score name entry works end to end; see "Where it is today").
- Hand-written assembly is translated register-exactly and every C function is checked call by call
  against the original (see "Decompilation workflow").
- 133 functions translated (about 13% of the code by size; most of what remains is the renderer's
  large assembly routines and the shell in segment 8).

# Step 5: two games, one engine; native archive reader

## Two games

| | Mazeworld Catacombs 1.0.5 | Mazeworld (Original) 1.0.3 |
|---|---|---|
| Code | 6 segments, 86 KB, 435 functions | 10 segments, 114 KB, 687 functions |
| Globals | 20,072 bytes | 29,246 bytes |
| Harness | plays (scripted keyboard) | boots, shareware notice, plays the first scene (scripted mouse/keys) |

Function-by-function comparison (`tools/compare_builds.py build/disasm build/disasm-original`):
46% of Catacombs' instructions are in Original exactly or nearly. By segment: runtime and library
glue ~98%, renderer ~68% (seg 2) and ~30% (seg 7), shell/init/events ~61% (seg 8); Catacombs' game
logic (seg 4: maze generation, monsters, sound, scoring) ~2%. Assets: all 14 `mazr` tables and 14 of
Catacombs' 16 `m2cv` sprite sets are byte-identical; Original adds 22 sprite sets and 14 colour tables.
Original-only Toolbox needs: printing (maps), Custom Get/PutFile (saved games), driver control calls,
`DrawChar`, `CopyPixMap`, `SetPortPix`, `InitCPort`, `SetWTitle`.

Plan: one engine (`engine/`: renderer, sprites, lookup tables, shell, Toolbox layer, archive reader)
plus per-game code (`games/catacombs/`, `games/original/`). A shared function is verified in the
Catacombs harness and then re-verified against Original. A launcher lists the games found and lets the
user pick one. Catacombs is finished first.

Your two Catacombs copies (`.hqx` from Info-Mac, and a 2005 StuffIt 5/Arsenic repack) contain the same
1.0.5 game: all 131 application and 13 sound resources are identical, and 640 ticks of scripted play
give byte-identical globals, heap, screen and audio.

## Archive reader (`archive/`)

BinHex 4.0, MacBinary, StuffIt 1.5–4 and StuffIt 5 containers, methods none/RLE90/LZ+Huffman
(13)/Arsenic (15), CRC-checked, nested archives unwrapped. All 22 forks of the three archives are
byte-identical to `unar`'s output. The harness now boots a game directly from its archive.

---

# Earlier steps

## The binary

- 68k only, 6 code segments, 85,950 bytes of code. No PowerPC code.
- Runtime: `ZERO`/`DATA`/`DREL` A5-world initialisation (THINK C / CodeWarrior 68k scheme, not MPW).
  CODE 1 is the runtime: start-up (JT 0), switch helpers (JT 1–3), 32-bit multiply/divide helpers (JT 4–8),
  exit (JT 9). The program's main function is JT 15 = `fn_S8_21AA`.
- A5 world: 20,072 bytes of globals, 129 jump-table entries.
- No MacsBug symbols: every function name has to be earned.
- Copyright string: "copyright © 1994 Michael R. Howard".

| Segment | Bytes | Functions found | Notes |
|---|---:|---:|---|
| 1 | 566 | 14 | Runtime library |
| 2 | 23,562 | 51 | Graphics setup (GWorlds, palettes), error alerts, wall/column drawing loops |
| 3 | 1,374 | 22 | Library glue: Gestalt, FindFolder, NumToString, Pack2 |
| 4 | 17,644 | 114* | |
| 7 | 17,702 | 98* | Mostly hand-written assembly: renderer, computed jumps into unrolled code |
| 8 | 25,094 | 136 | `main`, init, menus, event loop, preferences |

\* Counts in segments 4 and 7 include starts found by linear sweep; they will shrink as traces prove the real boundaries.

Static coverage is 99.7% of code bytes: 60,500 bytes proven by control flow (jump table, calls, branches,
switch tables), 4,300 bytes reached only from a `LINK A6` scan (`l`), and 20,900 bytes of linear-sweep code
(`?`) awaiting confirmation by traces. Decoded instructions never overlap.
Every instruction executed so far matches the static disassembly exactly (see below).

## Assets (all inside the original archive)

`m2cv` ×16 (sprite sets with frame offset tables, 145 KB), `mazr` ×14 (lookup tables: perspective,
colour remap, …), `clut` ×3, `PICT` ×2, `TEXT` ×23 (built-in help), dialogs/menus/windows,
and 12 `snd ` resources in the separate sound file.

## Harness (`harness/`)

Runs the original CODE resources under Musashi (68020) with a high-level Mac OS emulation:

- Segment Loader (real JT patching), Memory Manager (handles, master pointers, states),
  Resource Manager over the original forks, in-memory File Manager (FSSpec + parameter-block I/O),
  Folder Manager, Gestalt answers of a System 7.1 colour Mac, Pack7, alerts logged with their text.
- Every other Toolbox call is stubbed from a signature table generated from the multiversal
  interface definitions (583 traps, 639 selectors), logged on first use; register-based calls with
  no implementation stop the run instead of guessing.
- Deterministic: fixed clock, ticks derived from the instruction count, nothing persisted between runs.
- Writes an execution trace that feeds back into the disassembler.

### Where it is today

Driven by an input script, the original starts a game and plays: the 3D view (walls, the level 1
Evil Rat), HUD bars, scanner/auto-map, scores and level counters are all drawn by the game's own
code with correct colours. 640 ticks of play take about 35 million 68k instructions.

Coverage: 12,208 distinct instructions have executed (39 KB, 46% of all code). Every one lands on
an instruction the disassembler decoded; no disagreements.

Sound now works: the game finds its sound file through the Process Manager, opens 1/2/4 sampled-sound
channels (Reduced/Normal/Full), plays effects with `bufferCmd` and learns a channel is free from a
`callBackCmd` callback. The model (`mactb/sound.c`) advances in ticks, so callbacks land at fixed points
in game time; each run also writes the mixed audio to `sound.wav`.

Dialogs: modal dialogs from `DLOG`/`DITL` (push buttons, check boxes, static and edit text, automatic
centring), `ModalDialog` driven by the input script (typing goes into the edit field, Return presses the
default button), drawn by `mactb/dialog.c`. `ModalDialog` waits like everything else in the time model: a
tick passes and the trap runs again, so VBL tasks keep running. Closing a window erases what it covered
and sends update events to the windows behind. The game-over high-score dialog works end to end
(`tests/highscore.txt`). Not yet: filter procedures and user items (the instructions dialog), menus.

Still stubbed: menus, cursor, `PICT` drawing (about/registration screens only), Apple Events.

## Decompilation workflow

1. Write the C version in `game/seg<N>.c`.
2. Add its signature to `game/subs.txt`; `tools/gen_subs.py` makes the harness glue.
3. `tools/verify_subs.sh` (one script) or `tools/verify_all.sh quick|full` (all scripts) checks it twice:
   - **per call** (`-subcheck`): at every call the C version runs on the real inputs, its registers and
     every byte it wrote are recorded and undone, then the original runs and the two must agree;
   - **end to end** (`-sub`): a whole game with the C functions in place must end with globals, heap,
     screen and audio identical to the original's.
4. `tools/ready.py` lists the executed functions whose callees are all done (the next candidates).

Three kinds of function, and three formats in `game/subs.txt`:
- **Compiled C** (LINK A6 frame, stack arguments, result in D0): plain C with the original types (`int`
  was 16 bits). The check compares D0 as far as callers see it and the callee-saved registers.
- **Hand-written assembly** (no frame, arguments and results in registers and condition codes; much of
  segments 2, 4, 7 and 8): `void fn(Regs *r)` over a register block (`game/mzasm.h`), keeping partial
  writes (`move.b` changes one byte), and computing X N Z V C exactly. The check compares every data
  and address register and the CCR. Some are local steps that work inside their caller's A6 frame.
  A compiled function that assembly calls is written at register level too, with a C-facing wrapper
  (e.g. `fn_S2_3532` / `fn_S2_3532_value`), so its scratch registers stay exact for those callers.
- **Pascal-convention runtime glue** (segment 3: the callee pops its arguments, results go in a slot the
  caller reserved): `pascal` entries.

Details the checker forced into the open:
- Compiled C modifies its own parameters in place; the caller's argument area is dead after a call.
- The OS trap dispatcher returns with `TST.W D0`; the harness now does the same.
- The first call through the jump table into a segment loads it (allocating its code). C code marks its
  cross-segment calls with `mz_seg(n)`, which does the same load in the harness and nothing in the port.
- Where the original used a buffer in its stack frame (a string for `ParamText`), C code takes one with
  `mz_local`, which lives in the same place (just below the stack pointer).
- Register-level routines that call compiled C can list the registers that the compiled code leaves
  undefined (`ignore=`); routines calling Toolbox code whose state is outside emulated memory (window
  list, menus) are marked `nocheck` and covered by the end-to-end comparison instead.
- Host code in the harness that writes emulated memory directly (memcpy, QuickDraw drawing) reports the
  range, so the checker can undo C's writes exactly.

- Where the original spins until a VBL task changes something, C code calls `mz_idle()` once per
  pass: a tick passes and its VBL processing runs as a nested interrupt (returning to `IDLERET`).
  Toolbox traps that wait the same way (`ModalDialog`) do it inside the glue. In the per-call check
  such a call cannot be compared (the time would pass twice), so its C version is abandoned, its
  writes undone, and the original runs alone ("waits for a tick").
- VBL tasks and sound callbacks run at interrupt time; they are checked like any other call (their
  returns into the interrupt stub are detected, and only their own writes are compared).
- In the per-call check a `nocheck` function runs as the original, so everything it calls is still
  checked call by call; its C version is covered by the end-to-end run.
- Test set-up pokes are timed by tick (`30 setg 220A 4`), not by code address, so they land at the
  same moment whichever code (original or C) is running.

The checker was tested by planting bugs (a wrong flag, a register left unextended, a wrong field, a
stray write): it caught each one at the first call where it made a difference.

Progress is tracked by `tools/ready.py` and the summary in the conversation log; every translated
function passes both checks on all scripts (`tools/verify_all.sh full`) and the fuzz targets
(`tests/fuzz.txt`, `tools/fuzz_all.sh`; `-fuzz name:count:inputs` re-runs one function on random
register, frame or global inputs).

### Time model

The harness models an infinitely fast Mac: ticks advance only when the game is provably waiting
(a spin loop whose registers and memory do not change between iterations, `WaitNextEvent` with
sleep, `Delay`, repeated `TickCount` polling). Each tick runs the VBL tasks as a level-1 interrupt.
The game paces itself by busy-waiting on a VBL-driven counter (`G_217C` reaches 2), so this gives
a full-rate 30 fps game with no frame skipping, independent of how fast anything runs. The native
port's test mode advances time at the same waits, which is what makes tick-exact comparison possible.

### Harness options

`-script file` (timed key presses, see `tests/`), `-ticks N`, `-shots N` (screen every N frames),
`-watch lo-hi` (who writes a memory range), `-n`, `-idle`, `-v`. Each run writes `harness.log`,
`traps.txt`, `screen.ppm`, `gworld*.ppm`, `a5world.bin` (all globals) and `heap.bin`.

### Renderer, as far as seen

Per frame: the scene is turned into a byte command stream (`G_20A2`); `fn_S2_1CF6` and the span
builder in `fn_S2_0986` turn it into runs of (address, colour longword, length) in a span list
(`G_1DF4`); a span filler (`S2:0CBC`) writes them into the 344x186 view GWorld; `CopyBits`
puts it in the window. Colours come from `Entry2Index` over a 16-entry palette built from `clut` 200.

## mactb (native, shared)

- `text.c` + `font_data.c`: an original bitmap font (generated by `tools/fontgen.py`) at the four
  sizes the game uses (9, 12, 14, 18), DrawString and TETextBox layout. Swappable later.
- `color.c`: standard colour tables (clut 1/2/4/8, grey ramps), nearest-colour matching,
  a deterministic Palette Manager model, QuickDraw `Random`.
- `pix8.c`: 8-bit rectangles, frames, lines, CopyBits with colour translation and scaling.
- `sane.c`: FP68K (Pack4) on 80-bit extended via SoftFloat: add/sub/mul/div/rem, compare,
  conversions to and from extended/double/single/integer/long/comp, sqrt, round, truncate,
  negate, abs, environment.

FIDELITY notes in `mactb.h` mark the places where behaviour is modelled rather than copied from
Apple (colour matching, palette slot allocation, Random until confirmed). They affect what is on
screen, never game logic, and they are the same code in the port and the harness.

## Things the matching work depends on

- **SANE floating point** (`Pack4`) is used: add/sub/mul/div and conversions between extended,
  double, single, integer and long. It must be bit-exact; Musashi's bundled softfloat has 80-bit extended.
- **QuickDraw `Random`** is called from 15 places, so the maze generator's output depends on it being
  exact. The harness implements the published algorithm; this still needs confirming against a real ROM.
- **VBL tasks** (`VInstall`, `SlotVInstall`) and Sound Manager callbacks run 68k code at interrupt time;
  the harness must deliver them at deterministic points.
- **Registration**: "Energies" only appear in registered copies; the freeware build ships unregistered
  (`Rgst` 128 is all zero).

## Architecture

The Toolbox subset is written once, as a native C library (`mactb/`), and used from both sides:
the harness calls it through thin 68k trap wrappers, and the port calls it directly.
Then any difference found by tick-by-tick comparison is a game-logic difference, never a
platform-layer one, and the port's QuickDraw/text/sound behaviour is the same code that the
original was validated against.

Done: Color QuickDraw subset, palettes, GWorlds, windows, SANE, update events, text, scripted input,
VBL tasks, the deterministic time model, dialogs, the register-exact assembly format, per-call checking.
Next: keep decompiling upward through the call graph in verified batches; menus (for settings coverage). The native Windows build (SDL2 window, input, audio, VBL timing,
and the archive reader) comes together once the game code is complete in C; until then every
piece is proven against the original in the harness.

## Left out on purpose

Code paths that only run on Mac set-ups the Windows port never presents are not translated. The
port always gives the game one 8-bit 640x480 screen, so they cannot run there, and they would add
nothing visible. The same rule applies to Mazeworld Original.

- **Other screen depths**: the floor/ceiling runs drawn straight into 1-, 4-, 16- and 32-bit
  screens (`fn_S2_195C`, `0D98`, `1276`, `15FC`; `mz_unsupported` in `game/seg2_frame.c`). They draw
  the same 16 palette colours in another pixel format; on an 8-bit screen they are never called.
- **Mac-only shell paths**: desk accessories, Apple events other than 'quit', black windows over
  other monitors, system-version checks, the menu bar hiding on screens under 384 rows (translated,
  `fn_S8_1ED0` / `1F54`, but never reached at 640x480).

## To do (not now)

- **For Mazeworld Original**: the Toolbox calls only Original uses (DrawChar, CopyPixMap,
  SetPortPix, InitCPort, SetWTitle, Standard File, printing) are added when its recorded games reach
  them, so each is tested as it is written.

- **The port's own options** in the Options menu, off by default: 32-bit output, other graphics
  options (WASD, mouse turning and classic arrow keys are done; for Original too, with click-to-fire
  and abilities on 1-4 — `mz_alt_controls`, harness `-alt` — or its classic mouse by option).
- **Switching between Catacombs and Original** from the menu, once Original is supported.
