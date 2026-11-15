# Mazeworld Duo: Mazeworld and Mazeworld Catacombs (1994 Mac) Ported

A native Windows port of Farfetch Software's two Mazeworld games for the Macintosh, by
Michael R. Howard:

- **Mazeworld: The Ghost of Mordaine** 1.0.3 (1994, shareware), called "Mazeworld Original" here
- **Mazeworld Catacombs** 1.0.5 (1994-97, freeware)

The games were rewritten in C from the original 68k code and checked against it frame by frame,
so they play exactly like the originals. No emulator, no Mac ROM.

Unofficial, and not affiliated with the rights holders.

## Game Files

The port plays straight from the original game archives. Both games' licenses allow sharing
them unchanged, so they are included in the `original` folder.

Mazeworld Original 1.0.3  
SHA-1:  
D7BB9B7CB3513D4306A7AB24C77D08252D82E319 (Mazeworld_v.1.0.3.sit)

Mazeworld Catacombs 1.0.5  
SHA-1:  
933399020CD16BFCE84651BBD82DA77A0EFB131F (mazeworld-catacombs-105.hqx)

Other copies of the same versions work too (`.sit`, `.hqx` or MacBinary `.bin`).

Put the archives next to `Mazeworld.exe`, or drop one onto it. With both there, switch games
in the Game menu.

## Playing

- W / S or Up / Down: forward and back. A / D: sideways. Left / Right or Q / E: turn.
- The mouse turns. Left click or Space fires.
- Mazeworld Original: 1 to 4 pick the ability. Options > Classic Mouse Controls brings back the
  original's mouse controls.
- Ctrl is the Mac's Command key (Ctrl+G play, Ctrl+N new game, Ctrl+Q quit).
- Alt+Enter: fullscreen.
- Settings and high scores go in `%APPDATA%\Mazeworld\Mazeworld Catacombs`. Saved games go
  where you choose.

## Building

See [BUILDING.md](BUILDING.md).

## Changelog

See [CHANGELOG.md](CHANGELOG.md).

## Issues and Limitations

Known:
- Beta status. Tested on Windows.
- Mazeworld Original's Print Order Form is left out.

Else:
- Please see the "Issues" section in GitHub.

## Credits

- Michael R. Howard / Farfetch Software, for the games
- [Musashi](https://github.com/kstenerud/Musashi) by Karl Stenerud, the 68k emulator the test rig
  runs the original code on (not part of the game)
- [The Unarchiver](https://theunarchiver.com/)'s XADMaster, which the StuffIt decoders in
  `archive/` are ported from
- "Geneva 9" by Kelsey Higham (CC BY 3.0), the 9 point font
- [SDL2](https://libsdl.org), for the window, input and sound

## License

The project's own code is MIT; see [LICENSE](LICENSE). The StuffIt decoders are LGPL-2.1.
The games themselves remain the property of their rights holders.
