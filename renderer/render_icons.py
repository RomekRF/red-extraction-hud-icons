# Copyright (c) 2026 RomekRF.
# SPDX-License-Identifier: MPL-2.0
"""Renders Red Faction's weapon and ammo icons from your own copy of the game: flat white silhouettes on a transparent
background, the way Red Extraction's HUD draws them.

    python render_icons.py --rf "C:\\Games\\Red Faction"            PNGs in .\\icons
    python render_icons.py --rf "C:\\Games\\Red Faction" --tga      TGAs as well (32-bit, what RF loads)
    python render_icons.py --rf "C:\\Games\\Red Faction" --views    every weapon from four sides, to pick your own views

The game's packfiles are only read. Each icon is rendered from the item's pickup mesh in meshes.vpp by icon_render.exe
(build it with build.bat). Needs Python 3 and Pillow (pip install pillow).
"""
import argparse
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL_EXE = os.path.join(HERE, "icon_render.exe")

VPP_MAGIC = 0x51890ACE
BLOCK = 0x800
NAME_BYTES = 60

# Views: yaw and pitch in radians (negative pitch looks down), zoom 1 fits the model.
SIDE = (4.7124, -0.05, 0.95)  # the muzzle to the left
THREE_QUARTER = (3.94, -0.42, 0.95)
WEAPON = (256, 128)
SQUARE = (128, 128)

# (output name, mesh in meshes.vpp, (width, height), view[, degrees to turn the render counter-clockwise afterwards,
# for a mesh modelled upright]). Meshes are each item's $V3D Filename in items.tbl.
ICONS = [
    ("remote_charge", "rmt_explosive.v3m", SQUARE, THREE_QUARTER),
    ("riot_stick", "weapon_riotstick.v3m", WEAPON, SIDE, 90),
    ("12mm_handgun", "weapon_ultorgun.v3m", WEAPON, SIDE),
    ("shotgun", "weapon_shotgun.v3m", WEAPON, SIDE),
    ("sniper_rifle", "Weapon_SniperRifle.v3m", WEAPON, SIDE),
    ("rocket_launcher", "Weapon_RocketLauncher.v3m", WEAPON, SIDE),
    ("assault_rifle", "Weapon_Aslt_Rifle.v3m", WEAPON, SIDE),
    ("machine_pistol", "weapon_machinep.v3m", WEAPON, SIDE),
    ("grenade", "Item_Grenade_ammo.v3m", SQUARE, THREE_QUARTER),
    ("flamethrower", "weapon_fthrower.v3m", WEAPON, SIDE),
    ("riot_shield", "weapon_riotshield.v3m", WEAPON, SIDE),
    ("rail_driver", "weapon_railrifle.v3m", WEAPON, SIDE),
    ("heavy_machine_gun", "Weapon_HeavyMGunB.v3m", WEAPON, SIDE),
    ("precision_rifle", "Weapon_Aslt_Rifle2.v3m", WEAPON, SIDE),
    ("fusion_rocket_launcher", "Weapon_shoulder.v3m", WEAPON, SIDE, 90),
    ("ammo_12mm_handgun", "Item_pistol_ammo.v3m", SQUARE, THREE_QUARTER),
    ("ammo_assault_rifle", "Item_556mm_ammo.v3m", SQUARE, THREE_QUARTER),
    ("ammo_heavy_machine_gun", "Item_hmac_ammo.v3m", SQUARE, THREE_QUARTER),
    ("ammo_precision_rifle", "Item_556mmEX_ammo.v3m", SQUARE, THREE_QUARTER),
    ("ammo_shotgun", "Item_Shotgun_ammo.v3m", SQUARE, THREE_QUARTER),
    ("ammo_sniper_rifle", "Item_50cal_ammo.v3m", SQUARE, THREE_QUARTER),
    ("ammo_flamethrower", "powerup_flamecan.v3m", SQUARE, THREE_QUARTER),
    ("ammo_rocket_launcher", "Item_rocket_ammo.v3m", SQUARE, THREE_QUARTER),
    ("ammo_rail_driver", "item_rail_ammo.v3m", SQUARE, THREE_QUARTER),
    ("ammo_riot_stick", "item_Riot_ammo.v3m", SQUARE, THREE_QUARTER),
    ("ammo_remote_charge", "Item_Explosive_ammo.v3m", SQUARE, THREE_QUARTER),
    ("ammo_fusion_rocket_launcher", "Weapon_shoulder.v3m", SQUARE, THREE_QUARTER, 90),
]


def vpp_entries(path):
    """(name, offset, size) of every file in a VPP v1."""
    with open(path, "rb") as f:
        magic, _, count, _ = struct.unpack("<IIII", f.read(16))
        if magic != VPP_MAGIC:
            raise SystemExit(f"not a VPP: {path}")
        f.seek(BLOCK)
        directory = f.read(count * 64)
    offset = BLOCK + (count * 64 + BLOCK - 1) // BLOCK * BLOCK
    out = []
    for i in range(count):
        raw = directory[i * 64:i * 64 + 64]
        name = raw[:NAME_BYTES].split(b"\0", 1)[0].decode("latin1")
        size = struct.unpack("<I", raw[60:64])[0]
        out.append((name, offset, size))
        offset += (size + BLOCK - 1) // BLOCK * BLOCK
    return out


def extract(vpp_path, entries, name, dest):
    for entry, offset, size in entries:
        if entry.lower() == name.lower():
            with open(vpp_path, "rb") as f:
                f.seek(offset)
                data = f.read(size)
            with open(dest, "wb") as out:
                out.write(data)
            return
    raise SystemExit(f"{name} is not in {vpp_path}")


def render(mesh_path, out_path, size, view):
    width, height = size
    yaw, pitch, zoom = view
    result = subprocess.run([TOOL_EXE, mesh_path, out_path, str(width), str(height), str(yaw), str(pitch), str(zoom)],
                            capture_output=True, text=True)
    if result.returncode != 0:
        raise SystemExit(f"render failed for {mesh_path}: {result.stdout}{result.stderr}")
    return result.stdout.strip()


def write_tga(path, img):
    """32-bit uncompressed TGA, bottom-up, as RF loads it."""
    w, h = img.size
    header = bytearray(18)
    header[2] = 2
    header[12:14] = struct.pack("<H", w)
    header[14:16] = struct.pack("<H", h)
    header[16] = 32
    header[17] = 0x08
    data = img.tobytes()  # RGBA, top row first
    pixels = bytearray()
    for y in range(h - 1, -1, -1):
        row = data[y * w * 4:(y + 1) * w * 4]
        for x in range(w):
            r, g, b, a = row[x * 4:x * 4 + 4]
            pixels += bytes((b, g, r, a))
    with open(path, "wb") as f:
        f.write(bytes(header) + bytes(pixels))


def sheet(entries, path, columns=6):
    """A contact sheet: each icon over a dark and a light grey tile, with its name."""
    from PIL import Image, ImageDraw
    cell_w, cell_h = 280, 170
    rows = (len(entries) + columns - 1) // columns
    img = Image.new("RGB", (columns * cell_w, rows * cell_h * 2), (20, 20, 20))
    draw = ImageDraw.Draw(img)
    for i, (label, icon) in enumerate(entries):
        for band, grey in enumerate((46, 150)):
            x = (i % columns) * cell_w
            y = (i // columns) * cell_h * 2 + band * cell_h
            draw.rectangle([x + 4, y + 4, x + cell_w - 4, y + cell_h - 4], fill=(grey, grey, grey))
            img.paste(icon, (x + (cell_w - icon.width) // 2, y + 8), icon)
            draw.text((x + 8, y + cell_h - 22), label, fill=(255, 200, 120))
    img.save(path)
    print("sheet", path)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--rf", required=True, help="your Red Faction folder (the one with meshes.vpp)")
    ap.add_argument("--out", default=os.path.join(HERE, "icons"), help="output folder (default .\\icons)")
    ap.add_argument("--tga", action="store_true", help="also write 32-bit TGAs")
    ap.add_argument("--views", action="store_true", help="render each weapon from four sides instead")
    args = ap.parse_args()

    from PIL import Image
    if not os.path.exists(TOOL_EXE):
        raise SystemExit("icon_render.exe is missing: run build.bat first")
    vpp_path = os.path.join(args.rf, "meshes.vpp")
    if not os.path.exists(vpp_path):
        raise SystemExit(f"no meshes.vpp in {args.rf}")
    entries_in_vpp = vpp_entries(vpp_path)
    work = os.path.join(args.out, "_work")
    os.makedirs(work, exist_ok=True)

    entries = []
    for name, mesh, size, view, *rest in ICONS:
        if args.views and size != WEAPON:
            continue
        degrees = rest[0] if rest else 0
        mesh_path = os.path.join(work, mesh)
        extract(vpp_path, entries_in_vpp, mesh, mesh_path)
        yaws = (0.0, 1.5708, 3.1416, 4.7124) if args.views else (view[0],)
        for yaw in yaws:
            label = f"{name}_yaw{yaw:.2f}" if args.views else name
            tga = os.path.join(work, label + ".tga")
            render_size = (size[1], size[0]) if degrees in (90, 270) else size
            print(label, "->", render(mesh_path, tga, render_size, (yaw, view[1], view[2])))
            img = Image.open(tga).convert("RGBA")
            if degrees:
                img = img.rotate(degrees, expand=True)
            img.save(os.path.join(args.out, label + ".png"))
            if args.tga:
                write_tga(os.path.join(args.out, label + ".tga"), img)
            entries.append((label, img))
    sheet(entries, os.path.join(args.out, "views.png" if args.views else "sheet.png"), columns=4 if args.views else 6)
    print(f"{len(entries)} icon(s) in {args.out}")


if __name__ == "__main__":
    sys.exit(main())
