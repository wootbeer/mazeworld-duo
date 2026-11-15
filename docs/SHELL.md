# The shell: menus, dialogs and preferences (Catacombs 1.0.5)

How the port's native menus map to the original's, and the preferences file. Mazeworld
Original's menus work the same way (`game/orig_shell.c`).

## Menus of the original (`MENU` 128-133, handlers `fn_S8_2D86` ...)

| Menu | Item | What it does in the original |
|---|---|---|
| Apple | About Mazeworld Catacombs... (cmd-/) | `fn_S8_190C(1)`: the instructions dialog (300), 24 pages of pictures (page in G_4BC4, Previous / Next / Exit; palette 300). On screens under 512x342 an alert (G_4BBA); under 384 high the menu bar is hidden while it is open. |
| Apple | desk accessories | `OpenDeskAcc` (not in the port) |
| File | Play Game (cmd-G) | G_4798 = 1: the shell starts play (`fn_S8_3C0C`). Space or Return in the high-score window, or a click in it, do the same. Disabled after a game over (G_21B3). |
| File | New Game (cmd-N) | `fn_S8_3476`: resets the player (`fn_S8_3492`) and redraws the high-score window. Enabled only while G_21B4 is clear (`fn_S8_2FB4`). |
| File | Clear High Scores | `fn_S8_394A`: confirmation alert 134; on "Clear High Scores" the 10 entries of G_2242 become empty (name from G_47E4, level 0, score 0), G_22A6 = -1, redraw. |
| File | Quit (cmd-Q) | G_223C = 1: the shell loop ends; `fn_S8_21AA` then saves the preferences (`fn_S8_295A`; an error is reported with 'STR#' 128 and `fn_S8_37E6`) and frees everything. |
| Edit | (all) | disabled; only for desk accessories |
| View | Tiny / Small / 12" Monitor / 13" Monitor (cmd-1..4) | `fn_S8_4ACE(n)`: view size n (`fn_S8_527A`; if it fails, back to the previous size, else size 1, with alerts G_3B94 / G_3B32), the window resized and recentred (`fn_S8_4BE2`). Sizes above what the screen allows (G_2168, `fn_S8_513C`) are disabled. Checked: G_22B6. |
| View | Lowest / Low / Normal Resolution | G_2392 = 3 / 2 / 1 (pixel doubling of the floor and ceiling runs, `fn_S2_0986`). Checked: G_2392. |
| Sound | No / Reduced / Normal / Full Sound | G_238E = 0..3; sounds are unloaded at 0 (`fn_S4_3AB2`) or loaded if needed (`fn_S8_237C`); channels `fn_S4_3896(G_238E, or 4 for Full)`. Checked: G_238E. |
| Options | More Power B'Elanna!... | `fn_S8_1FC4`: dialog 400 with three check boxes: "Draw Direct to Screen" (G_22B8 = 0 when checked), "Skip Screen Sync" (G_2394 = 0 when checked), "Distribute Mapping" (G_2393 = 1 when checked: the scanner is drawn every other frame). OK applies, Cancel does not. |

While the game window is in front every menu item is disabled (`fn_S8_2E4C`); play is left with
a key (the game loop, `fn_S8_124E`), not the menus.

Hidden: a click in the top-right 4x4 pixels of the high-score window toggles G_22C9 (the order of
the renderer's two floor passes) before play starts (`fn_S8_2D26`).

## Preferences file

"Mzwl Catacombs Prefs" ('STR#' 128) in the Preferences folder, $3D4 bytes, read by `fn_S8_26D6`
(defaults by `fn_S8_27CC`), written by `fn_S8_295A` through a temporary file and FSpExchangeFiles
(`fn_S8_3522`):

| Offset | Size | Contents |
|---|---|---|
| 0 | 2 | view size (G_22B6) |
| 2 | 8 | the screen's rectangle (top, left, bottom, right), to reopen on the same screen |
| $A | 1 | G_22B8 (draw into the offscreen world, i.e. not direct to screen) |
| $B | 1 | G_2394 (wait for screen sync) |
| $C | 1 | resolution (G_2391 written, read back into G_2392) |
| $D | 1 | G_2393 (distribute mapping) |
| $E | 2 | sound level (G_238E) |
| $10 | 9 | registration name area (8 spaces and a 0 by default) |
| $118 | 10 x $46 | high scores: Pascal name (64 bytes), level word at +$40, score long at +$42 |

The port keeps this layout in a file in `%APPDATA%`, so an original prefs file can be imported.

## The port's shell (`port/shell.c`)

A native Windows menu bar with the original's items. Menus and items keep their numbers inside the
port (MENU 128-133), so each choice is exactly the original's MenuSelect result:

- **How a choice reaches the game.** A menu choice is posted as a mouse-down event carrying menu
  and item. The shell loop takes it from the event queue like any event; `shell_mouse_down` brings
  the game's menu records up to date (`fn_S8_2E10`, as before the original's MenuSelect) and runs
  what `fn_S8_2D86` would if the record has the item enabled. Ctrl+key (Ctrl is the Command key)
  arrives as a key event: `shell_menu_key` does what `fn_S8_2C36` does, `fn_S8_2E10` and then the
  MenuKey trap (enabled items only: / G N Q 1 2 3 4).
- **Enabled and checked items.** The Windows menu is updated each time it opens, following the
  original's `fn_S8_2E10`, `2E4C`, `2FB4`, `3190` and `3278`. During play everything is greyed
  except Exit, as in the original. Command keys during play end play and are then handled by
  the shell, as in the original (`fn_S8_124E`).
- **Quit, Exit and the close box** post the 'quit' Apple event: play ends (`fn_S8_10EC`), then the
  shell loop sets G_223C (`fn_S8_3B6E`). The prefs file is saved on the way out, as on a Mac.
- **Options > Legacy.** The original Options dialog's three check boxes ("More Power B'Elanna!") are
  three check-mark items in a Legacy submenu, with the same effect. They were speed settings for
  1990s Macs and make no visible difference in the port: Draw Direct to Screen draws the view
  straight into screen memory instead of an offscreen world (G_22B8 clear); Skip Screen Sync does
  not wait for the next screen refresh before showing a frame (G_2394 clear); Distribute Mapping
  redraws the scanner map every other frame (G_2393 set).
- **Help > About** (Ctrl+/): the original instructions dialog (`fn_S8_190C`, `game/seg8_help.c`), drawn
  by the game on its own screen as on a Mac: Previous / Next / Exit, or the arrow keys and Escape.
- **Suspend and resume.** Leaving the window sends the suspend event; coming back sends resume.
- **Preferences file.** Kept in `%APPDATA%\Mazeworld\Mazeworld Catacombs\`, in the original's
  format and byte for byte the same, so a Mac preferences file can be dropped in.

Controls (`port/sdl_main.c`, always on). The game itself reads only the keypad and letters
Q W E / A D / X, plus Space, Return, Enter or Shift to fire. The port adds S (back), the arrow keys
(classic: forward, back, turn left, turn right) and the left mouse button (fire) by setting the
KeyMap bit of the key the game knows. The pressed key's own events are unchanged, so typing a name
is not affected. In play the mouse is captured, and its sideways movement adds to the turn counter
G_217E, which holding Q or E steps once per tick (2 units per direction, 64 directions; 8 pixels
per unit). The movement is collected and paid out one unit per tick, like a held key, and faster
only while the mouse is far ahead, so turning is as even as with the keys.

Not carried over (Mac-only, nothing to do on Windows): desk accessories, the Edit menu (always
disabled in the game), window dragging, black windows over other monitors, the system-version
checks, cursor hiding by trap, and the screen-refresh VBL task (the port's frame timing replaces
it).
