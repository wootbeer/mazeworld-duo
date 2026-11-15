# Changelog

## [0.1.0 Beta] - 2026-11-14

### Added
- Runs natively on Windows, no emulator
- Both games in one program, picked in the Game menu
- Plays straight from the original `.sit`, `.hqx` or `.bin` archive
- Modern controls: WASD, arrow keys, mouse turning, left click to fire
- Mazeworld Original: 1 to 4 pick the ability
- Options > Classic Mouse Controls for Mazeworld Original's own mouse controls
- Fullscreen with Alt+Enter
- Resizable window, remembered between runs
- Options > Volume
- Options > Classic Labels, for Mazeworld Original's labels as they were

### Changed
- Windows menus instead of the Mac menu bar; Ctrl is the Command key
- Catacombs' Options dialog is now Options > Legacy
- Mazeworld Original: the window shows only the game and scales it; Window Size resizes the window
- Load Game and Save Game As use the Windows file dialogs
- Settings and high scores are kept in `%APPDATA%\Mazeworld\Mazeworld Catacombs`
- Mazeworld Original's Print Order Form is left out

### Fixed
- Mazeworld Original: the ability labels at the bottom corners ("crt", "dst", "aff", "neg") were cut off
