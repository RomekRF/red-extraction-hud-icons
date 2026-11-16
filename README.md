# Red Extraction HUD icons

Flat white HUD icons from Red Extraction (a Red Faction mod by RomekRF), packed for a Quake-3-style weapon bar and
a gamepad weapon wheel in any Red Faction client. Everything is white on transparent, so you tint it in code.

## What is in it

| Folder | What | Sizes | Files |
|---|---|---|---|
| `weapons/wide/` | the 15 player weapons, side view, muzzle to the left | 128x64, 256x128, 512x256 | `<icon>.png` + `.tga` |
| `weapons/square/` | the same silhouettes fitted and centred in a square (6% margin) | 64, 128, 256 | `<icon>.png` + `.tga` |
| `ammo/` | the 12 ammo pickups, three-quarter view, fitted (8% margin) | 64, 128, 256 | `<ammo_icon>.png` + `.tga` |
| `hud/slot/` | bar slot frame: `slot_normal`, `slot_selected`, `slot_empty` (disabled / no ammo) | square 64, 128, 256; wide 128x64, 256x128, 512x256 | `.png` + `.tga` |
| `hud/wheel/` | 8-segment wheel: `wheel_ring`, `wheel_segment` (highlight, top), `wheel_segment_0`..`_7` (pre-rotated), `wheel_disc` (centre), `wheel_pointer` | 512, 1024 | `.png` + `.tga` |
| `gear/` | 23 drawn gear icons (medkit, armour, backpacks, jetpacks, loot, attachments...) | 128, 256 | `.png` + `.tga` |
| `preview/` | the bar and wheel mock-ups, contact sheets of every set | 1920x1080 and sheets | `.png` |
| `renderer/` | the MPL-2.0 renderer that made the weapon and ammo icons, plus an in-process C++ header | | source only |
| `manifest.json`, `weapon_icons.h` | one weapon table: weapons.tbl name and index, display name, icon, ammo icon, wheel group, bar order | | |

Formats: PNG (RGBA) and 32-bit uncompressed TGA, bottom-up (descriptor 0x08), which RF's bitmap loader takes as is.
Alpha is straight (not premultiplied); RGB is white under every fully transparent pixel, so bilinear filtering gives no
dark fringe either way. Weapon and ammo icons keep a light face shading (grey 199-255) so boxes read; HUD pieces are
pure white with the strength in alpha. Edges are anti-aliased: renders are supersampled 4-8x, fitted icons are reduced
from a 1024 px render with an area filter in premultiplied space.

## Names and weapons.tbl

`manifest.json` and `weapon_icons.h` hold the same table, one row per weapon:

- `tbl_name`: the `$Name` string from stock `weapons.tbl` in `tables.vpp`, byte for byte (`"12mm handgun"`,
  `"rail_gun"`, `"scope_assault_rifle"`, `"shoulder_cannon"`, `"riot shield"`...). Match it case-insensitively, as RF does.
- `tbl_index`: 0-based position of that entry in stock `weapons.tbl` (the weapon type index). A mod's own table can
  reorder it, so look weapons up by name when you can (`find_weapon_icon()` in the header).
- `display_name`: the stock `$Display Name` text (with its XSTR id in the manifest).
- `icon`: base name under `weapons/`; `ammo_icon`: base name under `ammo/`, or null (riot shield has no ammo; for the
  grenade the weapon icon is the ammo). Machine pistol uses the 12mm ammo icon, like the game.
- `wheel_group` 1-8 and `bar_order` 1-15: a suggestion -
  1 melee (riot stick, riot shield), 2 sidearms (12mm handgun, machine pistol), 3 shotgun, 4 rifles (assault, precision),
  5 heavy (heavy machine gun, flamethrower), 6 long range (sniper rifle, rail driver), 7 launchers (rocket, fusion),
  8 explosives (grenade, remote charge). The detonator, the silenced 12mm and the AI/vehicle weapons have no icon.

## Weapon bar

Use the square icons in the square slots (or wide in wide slots). Per slot: draw `slot_normal` (or `slot_selected`
tinted with your accent colour, or `slot_empty` for a weapon you hold with no ammo), then the weapon icon on top, and
the ammo count as text under it. `preview/mockup_weapon_bar.png` shows the idea; the large selected weapon in it is
`weapons/wide/512x256` with the `ammo/64` icon beside the count.

## Weapon wheel

All wheel pieces share one square canvas with the wheel centre at the canvas centre, so they stack with no offsets.
Radii as fractions of the canvas side: ring outer 0.47, inner 0.25, centre disc 0.16, pointer tip 0.238.

- Segment k (0-7) is centred at k x 45 degrees, clockwise from 12 o'clock, and spans 45 degrees.
  `wheel_group` g goes in segment g - 1.
- Highlight: draw `wheel_segment_k`, or `wheel_segment` rotated by k x 45 degrees clockwise, tinted.
- Pointer: `wheel_pointer` points up; rotate it by the stick angle (or by k x 45 for snapping).
- Icons: put them around radius 0.36 of the canvas, at the segment's angle. In the mock-up the wide 256x128 icons are
  scaled to about 17% of the canvas width and stacked when a group has two.
- `wheel_segment_from_stick(x, y)` in `weapon_icons.h` maps a stick direction to a segment.

## Re-render at other sizes or views

The weapon and ammo icons come from each item's pickup mesh in `meshes.vpp` (the `$V3D Filename` in `items.tbl`).
`manifest.json` lists, per weapon, the mesh and the exact view (`yaw`, `pitch`, `zoom`, `rotate_ccw`).

- `renderer/build.bat` builds `icon_render.exe` (Visual Studio 2022, "Desktop development with C++").
  `icon_render <mesh.v3m> <out.tga> <width> <height> <yaw> <pitch> <zoom> [supersample] [floor]`, up to 1024 px a side.
- `renderer/render_icons.py --rf "C:\Games\Red Faction" [--tga] [--views]` renders the original Red Extraction set;
  `--views` renders every weapon from four sides to pick other angles. Needs Python 3 and Pillow.
- `renderer/icon_render_lib.h` does the same in-process: give it a `.v3m` file's bytes, get an RGBA buffer
  (`render_icon(bytes, size, Options, Image)`). `example_icon_render_lib.cpp` reads the mesh straight out of
  `meshes.vpp` and writes a TGA; `build_example.bat` builds it. With the manifest's view values it reproduces the kit's
  wide icons pixel for pixel. So a game client could render these icons at startup from the player's own game files instead of
  shipping any of them.
- `renderer/draw_gear_icons.py --size 512 --out DIR` redraws the gear icons at any size.

## Rights

- `gear/` and `hud/` are RomekRF's own drawings: CC BY 4.0 (`LICENSE-CC-BY-4.0.txt`). Credit line:
  "Icons by RomekRF (Red Extraction), CC BY 4.0".
- Code (`renderer/`, `weapon_icons.h`, `manifest.json`): Mozilla Public License 2.0 (`LICENSE-MPL-2.0.txt`).
- `weapons/` and `ammo/` are renders of Red Faction's own models. RomekRF gives away everything that is his in them
  (the rendering and styling) with no conditions (CC0 for his part). He cannot license the underlying designs, which
  belong to Volition / THQ Nordic: they stand on the same footing as any other Red Faction mod asset and are meant for
  use in Red Faction mods and clients that need the game. The clean alternative is `renderer/`: render them from the
  player's own files. See `NOTICE-weapon-renders.txt` in those folders.

This is a plain description of what is given, not legal advice.

## Credits

The V3M reader in `renderer/src/v3m_model.*` implements the Red Faction mesh format as documented in rf-reversed by
Rafal Harabien (https://github.com/rafalh/rf-reversed). Red Faction is a trademark of THQ Nordic GmbH; this kit is
unofficial and needs a lawfully obtained copy of the game.
