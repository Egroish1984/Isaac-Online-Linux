# Isaac Online Modded for Linux (CLI, C port)

A rewrite of the Isaac Online Modded WPF tool, now in plain C so it runs on Linux. It patches **The Binding of Isaac** executable (`isaac-ng.exe`) so mods stay enabled during online co-op, and it can patch the External Item Descriptions (EID) mod for the same reason. There's no GUI here, just a small terminal program you build with a normal Makefile.

## Build

```sh
make
```

That produces one binary: `isaac-online-modded`.

## Run

```sh
./isaac-online-modded                         # tries to auto-detect the Steam install
./isaac-online-modded /path/to/isaac-ng.exe   # or point it at the exe yourself
```

Auto-detection checks the usual Steam spots (`~/.steam/steam`, `~/.local/share/Steam`, the Flatpak Steam path) plus any extra Steam libraries listed in `libraryfolders.vdf`. GOG and Epic detection didn't make the cut, since their Linux install layout isn't as predictable as Steam's. If you're on one of those, just pass the game's path as an argument or type it in when the program asks.

Once it's running, the menu lets you:
1. Patch co-op mods plus the analytics-crash fix
2. Patch co-op characters (needs step 1 done first)
3. Patch EID's `eid_api.lua` for co-op
4. Change the game path
5. Refresh the status display

Every write happens atomically (write to a temp file, then rename it into place), same as the original tool did.

## Files

- `patch.c` / `patch.h`: byte-pattern find and replace on the game executable, ported from `GamePatcher.cs`.
- `eid_patch.c` / `eid_patch.h`: text patch for `features/eid_api.lua`, ported from `EIDPatcher.cs`.
- `main.c`: the CLI menu, Steam path autodetection, and EID mod-folder discovery. This replaces `MainWindow.xaml(.cs)`.

## Notes

- This only edits the binary and lua files on disk. It doesn't run or inject into the game process.
- Re-run the patch after every game update, just like before.
- Same disclaimer as the original: this is meant for friends-only co-op, so use it responsibly.
