// Copyright (c) 2026 RomekRF.
// SPDX-License-Identifier: MPL-2.0
#pragma once

// Software rasteriser for the mesh browser's 3D preview.
//
// Deliberately self-contained: no Direct3D, no RED globals, no engine calls. The
// editor's device is fragile (a stray gr_d3d_lock on an unpaged bitmap crashes
// RED outright), and a modal browser dialog has no business touching it mid-frame.
// A single mesh at preview size is trivial work for a CPU rasteriser, and this
// way the preview behaves identically on the d3d8to11 and d3d8to9 backends.
//
// Output is top-down 32-bit BGRA, which is what a Win32 DIB section wants.

#include <cstdint>
#include <vector>

#include "v3m_model.h"

namespace red::mesh_render
{
    struct Texture
    {
        int w = 0;
        int h = 0;
        std::vector<std::uint32_t> argb;   // 0xAARRGGBB, row-major, top row first
        bool valid() const { return w > 0 && h > 0 && argb.size() >= static_cast<std::size_t>(w) * h; }

        // Does this texture contain partially transparent texels? -1 unknown, 0 no,
        // 1 yes. Cached because render() needs it to decide whether a blend pass is
        // required at all, and recomputing it per frame meant scanning every texel of
        // every texture on every frame of an animated effect -- 2 million reads a frame
        // for a 2048x1024 texture with no early exit when the answer is "no".
        mutable signed char soft_alpha = -1;
    };

    struct View
    {
        // Romek 2026-08-27: "the perspective is low in a weird angle". It was --
        // pitch 0.35 rad is 20 degrees above the horizon, which looks up the skirt
        // of most props and hides the top faces entirely. 0.50 rad (29 deg) with a
        // wider yaw is the conventional three-quarter view a model browser wants:
        // top, front and side all readable at once.
        // Romek 2026-08-27, twice: "the perspective is low in a weird angle", then
        // "still low and behind the object". Both were real and they were different
        // problems. BEHIND: yaw 0.80 put the camera on the model's back -- verified on
        // s4_fridgeMED_01.v3m, which showed its condenser coils rather than its door.
        // Adding half a turn faces the camera at the front. LOW: that was the 52-degree
        // field of view (see fov_y in the .cpp), not the elevation -- wide-angle
        // perspective makes verticals converge hard and reads as a dramatic upshot even
        // from above. Pitch is nudged up slightly on top of that.
        // PITCH IS NEGATIVE-IS-ABOVE. The transform rotates the model, not the
        // camera: vz = ry*sin(pitch) + rz*cos(pitch), so with pitch > 0 the model's
        // BOTTOM (ry < 0) gets the smaller z and sits nearer the lens -- i.e. the
        // camera is underneath, looking up. The old comment here claimed the
        // opposite, which is why raising this number twice kept making Romek's
        // complaint worse instead of better.
        float yaw = 3.94f;       // radians, orbit around the model (0.80 + pi)
        float pitch = -0.42f;    // radians, NEGATIVE looks down (24 deg above)
        float zoom = 1.0f;       // 1 = whole model just fits
        float pan_x = 0.0f;      // fraction of frame height
        float pan_y = 0.0f;
    };

    // per_chunk_tex must be the same length as model.chunks; a null entry draws
    // that chunk untextured (flat grey), which is what happens when the texture
    // is missing from the user's folders -- the shape is still worth seeing.
    void render(const v3m::Model& model,
                const std::vector<const Texture*>& per_chunk_tex,
                const View& view,
                std::uint32_t* dst, int width, int height,
                std::uint32_t bg_top, std::uint32_t bg_bottom);
}
