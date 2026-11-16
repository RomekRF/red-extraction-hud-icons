// Copyright (c) 2026 RomekRF.
// SPDX-License-Identifier: MPL-2.0
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// weapon_icons.h -- the red-extraction-hud-icons weapon table (same data as manifest.json). Plain C++17, no includes.
//
//   tbl_name      the weapon's $Name in stock weapons.tbl, byte for byte (match it case-insensitively, as RF does)
//   tbl_index     0-based position of that entry in stock weapons.tbl (the weapon type index; mods can change it,
//                 so prefer looking weapons up by tbl_name)
//   display_name  the $Display Name text in stock weapons.tbl (English)
//   icon          file base name in weapons/wide/<W>x<H>/ and weapons/square/<N>/  (.png and .tga)
//   ammo_icon     file base name in ammo/<N>/, or nullptr (riot shield: no ammo; grenade: the icon is the ammo)
//   wheel_group   suggested wheel segment group 1-8 (segment index = wheel_group - 1, clockwise from 12 o'clock)
//   bar_order     suggested position in a weapon bar, 1-15
#pragma once

namespace redex_hud_icons
{
    struct WeaponIcon
    {
        const char* tbl_name;
        int tbl_index;
        const char* display_name;
        const char* icon;
        const char* ammo_icon;
        int wheel_group;
        int bar_order;
    };

    // In bar order.
    inline constexpr WeaponIcon weapon_icons[] = {
        {"Riot Stick", 2, "Control Baton", "riot_stick", "ammo_riot_stick", 1, 1},
        {"riot shield", 13, "Riot Shield", "riot_shield", nullptr, 1, 2},
        {"12mm handgun", 3, "12mm pistol", "12mm_handgun", "ammo_12mm_handgun", 2, 3},
        {"Machine Pistol", 9, "Submachine Gun", "machine_pistol", "ammo_12mm_handgun", 2, 4},
        {"Shotgun", 5, "Automatic Shotgun", "shotgun", "ammo_shotgun", 3, 5},
        {"Assault Rifle", 8, "Assault Rifle", "assault_rifle", "ammo_assault_rifle", 4, 6},
        {"scope_assault_rifle", 16, "Precision Rifle", "precision_rifle", "ammo_precision_rifle", 4, 7},
        {"heavy_machine_gun", 15, "Heavy Machine Gun", "heavy_machine_gun", "ammo_heavy_machine_gun", 5, 8},
        {"Flamethrower", 12, "Flamethrower", "flamethrower", "ammo_flamethrower", 5, 9},
        {"Sniper Rifle", 6, "Sniper Rifle", "sniper_rifle", "ammo_sniper_rifle", 6, 10},
        {"rail_gun", 14, "Rail Driver", "rail_driver", "ammo_rail_driver", 6, 11},
        {"Rocket Launcher", 7, "Rocket Launcher", "rocket_launcher", "ammo_rocket_launcher", 7, 12},
        {"shoulder_cannon", 17, "Fusion Rocket Launcher", "fusion_rocket_launcher", "ammo_fusion_rocket_launcher", 7, 13},
        {"Grenade", 11, "Grenade", "grenade", nullptr, 8, 14},
        {"Remote Charge", 0, "Remote Charge", "remote_charge", "ammo_remote_charge", 8, 15},
    };
    inline constexpr int weapon_icon_count = static_cast<int>(sizeof(weapon_icons) / sizeof(weapon_icons[0]));

    // Wheel pieces (hud/wheel/<size>/): all share one square canvas, centred. Radii are fractions of the canvas side.
    inline constexpr int wheel_segments = 8;
    inline constexpr float wheel_segment_deg = 45.0f;     // segment k is centred at k * 45 deg, clockwise from up
    inline constexpr float wheel_outer_radius = 0.47f;
    inline constexpr float wheel_inner_radius = 0.25f;
    inline constexpr float wheel_disc_radius = 0.16f;
    inline constexpr float wheel_pointer_tip_radius = 0.238f;

    // Case-insensitive ASCII compare, usable in constant expressions.
    constexpr bool iequals(const char* a, const char* b)
    {
        for (;; ++a, ++b) {
            char ca = *a, cb = *b;
            if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
            if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
            if (ca != cb) return false;
            if (ca == '\0') return true;
        }
    }

    // The entry for a weapons.tbl $Name, or nullptr.
    constexpr const WeaponIcon* find_weapon_icon(const char* tbl_name)
    {
        for (const WeaponIcon& w : weapon_icons) {
            if (iequals(w.tbl_name, tbl_name)) return &w;
        }
        return nullptr;
    }

    // Segment (0-7) a stick direction points at. x right, y up, any length > 0; clockwise from 12 o'clock.
    // (Uses no <cmath>: compares against the segment borders at 22.5 + 45k degrees.)
    constexpr int wheel_segment_from_stick(float x, float y)
    {
        // tan(22.5 deg) and tan(67.5 deg)
        constexpr float t1 = 0.41421356f, t2 = 2.41421356f;
        const float ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
        if (ay >= ax * t2) return y > 0 ? 0 : 4;           // up / down
        if (ay <= ax * t1) return x > 0 ? 2 : 6;           // right / left
        if (x > 0) return y > 0 ? 1 : 3;                   // diagonals
        return y > 0 ? 7 : 5;
    }
}
