# Menu ring check

Off-device check for the mode menu's navigation table, `menuNav[]` in
`Software/src/Version 6 and newer/MorseMenu.cpp`. No device and no firmware
build are needed — PlatformIO (for the per-environment flags) and a host C/C++
preprocessor are enough. CI runs it on every push.

```sh
python3 check_menu_ring.py                  # every environment; exits 1 on a fault
python3 check_menu_ring.py pocketwroom      # just the named environment(s)
```

## What it guards

Each menu entry names its own left and right neighbour, its parent and its
first child. Every link is therefore written down twice, once at each end, and
nothing but care keeps the two in step. A link recorded at one end only
compiles without a murmur and shows up as a menu that behaves differently
turning left than turning right.

That shipped in V9.0: stepping back from **CW Generator** in the Koch Trainer
jumped over **Preview Char** to **Learn New Chr**. Preview Char had been moved
to the end of the enum (menu positions are stored raw, so new entries are
appended and spliced into their ring by editing their neighbours), and one of
its two back-links was not updated (PR #222).

Entries appended like that are exactly where the next mistake will happen, and
the table is full of `#ifdef`s — LoRa, games, QSO Bot, Practice Stats, the
Accessibility Edition — so a ring can be intact in one build and broken in
another. The script therefore checks all environments in `platformio.ini`,
each preprocessed with that environment's own `-D` flags (after
`build_unflags`), and reports for each:

- the enum `menuNo`, `menuN`, `menuNav[]` and `menuText[]` disagreeing on the
  number of entries (a short initializer is silently zero-filled);
- a left or right link that is not returned by the neighbour, or that joins
  entries of different levels or parents;
- a down link whose target does not point back up to it, one level deeper;
- an entry that cannot be reached from the top-level menu at all.

The idea, and the first version of this check, are Christian Konecny's
(OE1CKO), who wrote it to verify the PR #222 fix.
