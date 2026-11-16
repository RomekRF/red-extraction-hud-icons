// Copyright (c) 2026 RomekRF.
// SPDX-License-Identifier: MPL-2.0
//
// icon_render_lib.h -- render a Red Faction mesh (.v3m bytes) to a flat white RGBA icon, in-process.
//
// The same pipeline as src/icon_render.cpp (the command-line tool), wrapped for a game client:
// read the mesh from the player's own meshes.vpp into memory, call render_icon(), upload the pixels as a texture.
// No art has to ship with the client.
//
// Build: add src/v3m_model.cpp and src/mesh_render.cpp to your project, include this header. C++17 or later.
//
//   icon_render::Options opt;          // defaults = the kit's wide weapon icons (side view, muzzle left)
//   opt.width = 256; opt.height = 128;
//   icon_render::Image img;
//   if (icon_render::render_icon(bytes.data(), bytes.size(), opt, img)) { /* img.rgba: width*height*4, top row first */ }
//
// The view each kit icon was rendered with is in manifest.json ("render": mesh, yaw, pitch, zoom, rotate_ccw).
#pragma once

#include "src/mesh_render.h"
#include "src/v3m_model.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace icon_render
{
    struct Options
    {
        int width = 256;           // final size, after rotate_ccw (1 to 1024 before rotation)
        int height = 128;
        float yaw = 4.7124f;       // radians; 4.7124 = side view, muzzle to the left
        float pitch = -0.05f;      // radians; negative looks down
        float zoom = 0.95f;        // 1 = whole model just fits
        int supersample = 4;       // 1 to 8 samples per axis
        double floor = 0.78;       // darkest face shade (1 = pure flat white)
        int rotate_ccw = 0;        // 0, 90, 180 or 270: for meshes modelled upright (riot stick, fusion)
    };

    struct Image
    {
        int width = 0;
        int height = 0;
        std::vector<std::uint8_t> rgba;   // straight alpha, top row first; RGB is white where alpha is 0
        std::string error;
    };

    inline bool render_icon(const std::uint8_t* v3m, std::size_t size, const Options& opt, Image& out)
    {
        out = Image{};
        const bool swap = opt.rotate_ccw == 90 || opt.rotate_ccw == 270;
        const int width = swap ? opt.height : opt.width;    // size of the unrotated render
        const int height = swap ? opt.width : opt.height;
        if (width <= 0 || height <= 0 || width > 1024 || height > 1024) {
            out.error = "width and height must be 1 to 1024";
            return false;
        }
        if (opt.rotate_ccw % 90 != 0 || opt.rotate_ccw < 0 || opt.rotate_ccw > 270) {
            out.error = "rotate_ccw must be 0, 90, 180 or 270";
            return false;
        }
        red::v3m::Model m;
        if (!red::v3m::load(v3m, size, m)) {
            out.error = "load failed: " + m.error;
            return false;
        }
        const int ss = std::clamp(opt.supersample, 1, 8);
        const std::vector<const red::mesh_render::Texture*> untextured(m.chunks.size(), nullptr);
        red::mesh_render::View view;
        view.yaw = opt.yaw;
        view.pitch = opt.pitch;
        view.zoom = opt.zoom;
        const int bw = width * ss;
        const int bh = height * ss;
        // Coverage from two renders, over black and over white.
        std::vector<std::uint32_t> black(static_cast<std::size_t>(bw) * bh);
        std::vector<std::uint32_t> white(static_cast<std::size_t>(bw) * bh);
        red::mesh_render::render(m, untextured, view, black.data(), bw, bh, 0xFF000000u, 0xFF000000u);
        red::mesh_render::render(m, untextured, view, white.data(), bw, bh, 0xFFFFFFFFu, 0xFFFFFFFFu);

        std::vector<double> alpha(static_cast<std::size_t>(width) * height, 0.0);
        std::vector<double> lum(alpha.size(), 0.0);
        double lum_max = 0.0;
        bool covered = false;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                double a = 0.0;
                double premul = 0.0;
                for (int j = 0; j < ss; ++j) {
                    for (int i = 0; i < ss; ++i) {
                        const std::size_t at = static_cast<std::size_t>(y * ss + j) * bw + (x * ss + i);
                        const std::uint32_t k = black[at];
                        const std::uint32_t w = white[at];
                        const int kr = (k >> 16) & 255, kg = (k >> 8) & 255, kb = k & 255;
                        const int wr = (w >> 16) & 255, wg = (w >> 8) & 255, wb = w & 255;
                        a += std::clamp(1.0 - ((wr - kr) + (wg - kg) + (wb - kb)) / (3.0 * 255.0), 0.0, 1.0);
                        premul += (kr + kg + kb) / 3.0;
                    }
                }
                const double count = static_cast<double>(ss) * ss;
                a /= count;
                const std::size_t o = static_cast<std::size_t>(y) * width + x;
                alpha[o] = a;
                lum[o] = a > 1e-6 ? premul / count / a : 0.0;
                if (a > 0.5) {
                    lum_max = std::max(lum_max, lum[o]);
                }
            }
        }

        out.width = opt.width;
        out.height = opt.height;
        out.rgba.assign(static_cast<std::size_t>(out.width) * out.height * 4, 0);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::size_t o = static_cast<std::size_t>(y) * width + x;
                const auto a = static_cast<std::uint8_t>(std::clamp(alpha[o] * 255.0 + 0.5, 0.0, 255.0));
                const double shade = lum_max > 0.0 ? std::clamp(lum[o] / lum_max, 0.0, 1.0) : 1.0;
                const auto v = a == 0 ? std::uint8_t{255}
                                      : static_cast<std::uint8_t>(std::clamp(
                                            255.0 * (opt.floor + (1.0 - opt.floor) * shade), 0.0, 255.0));
                covered = covered || a > 0;
                // Destination of source pixel (x, y) after turning the picture counter-clockwise.
                int dx = x, dy = y;
                switch (opt.rotate_ccw) {
                    case 90: dx = y; dy = width - 1 - x; break;
                    case 180: dx = width - 1 - x; dy = height - 1 - y; break;
                    case 270: dx = height - 1 - y; dy = x; break;
                    default: break;
                }
                std::uint8_t* p = &out.rgba[(static_cast<std::size_t>(dy) * out.width + dx) * 4];
                p[0] = v;
                p[1] = v;
                p[2] = v;
                p[3] = a;
            }
        }
        if (!covered) {
            out.error = "nothing drawn: every triangle is facing away or outside the frame";
            return false;
        }
        return true;
    }
}
