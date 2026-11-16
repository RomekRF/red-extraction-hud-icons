// Copyright (c) 2026 RomekRF.
// SPDX-License-Identifier: MPL-2.0
//
// Renders one Red Faction mesh (.v3m) as a flat white silhouette icon with a transparent background.
//
// Built and driven by render_icons.py, which extracts the meshes from your own Red Faction packfiles.
//
//   icon_render <mesh.v3m> <out.tga> <width> <height> <yaw> <pitch> <zoom> [supersample] [floor]
//
// Coverage comes from rendering twice, over black and over white. The colour is white, darkened toward `floor` where
// the untextured, lit render is darker, so a box still shows its faces. The file is an uncompressed 32-bit TGA,
// bottom-up (byte 17 = 0x08).
#include "v3m_model.h"
#include "mesh_render.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace red;

int main(int argc, char** argv)
{
    if (argc < 8) {
        std::puts("usage: icon_render <mesh.v3m> <out.tga> <width> <height> <yaw> <pitch> <zoom> [supersample] [floor]");
        return 2;
    }
    const int width = std::atoi(argv[3]);
    const int height = std::atoi(argv[4]);
    const int ss = argc > 8 ? std::clamp(std::atoi(argv[8]), 1, 8) : 4;
    const double floor_level = argc > 9 ? std::atof(argv[9]) : 0.78;
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) {
        std::puts("width and height must be 1 to 1024");
        return 2;
    }
    v3m::Model m;
    if (!v3m::load_file(argv[1], m)) {
        std::printf("load failed: %s\n", m.error.c_str());
        return 1;
    }
    const std::vector<const mesh_render::Texture*> untextured(m.chunks.size(), nullptr);
    mesh_render::View view;
    view.yaw = static_cast<float>(std::atof(argv[5]));
    view.pitch = static_cast<float>(std::atof(argv[6]));
    view.zoom = static_cast<float>(std::atof(argv[7]));
    const int bw = width * ss;
    const int bh = height * ss;
    std::vector<std::uint32_t> black(static_cast<std::size_t>(bw) * bh);
    std::vector<std::uint32_t> white(static_cast<std::size_t>(bw) * bh);
    mesh_render::render(m, untextured, view, black.data(), bw, bh, 0xFF000000u, 0xFF000000u);
    mesh_render::render(m, untextured, view, white.data(), bw, bh, 0xFFFFFFFFu, 0xFFFFFFFFu);

    // Per output pixel: coverage, and the lit grey where covered (un-premultiplied from the black render).
    std::vector<double> alpha(static_cast<std::size_t>(width) * height, 0.0);
    std::vector<double> lum(static_cast<std::size_t>(width) * height, 0.0);
    double lum_max = 0.0;
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
                    const double al = std::clamp(1.0 - ((wr - kr) + (wg - kg) + (wb - kb)) / (3.0 * 255.0), 0.0, 1.0);
                    a += al;
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

    std::vector<unsigned char> out(18 + static_cast<std::size_t>(width) * height * 4, 0);
    out[2] = 2;
    out[12] = width & 255;
    out[13] = (width >> 8) & 255;
    out[14] = height & 255;
    out[15] = (height >> 8) & 255;
    out[16] = 32;
    out[17] = 0x08; // bottom-up, 8 alpha bits
    int opaque = 0;
    int covered = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t o = static_cast<std::size_t>(y) * width + x;
            const double shade = lum_max > 0.0 ? std::clamp(lum[o] / lum_max, 0.0, 1.0) : 1.0;
            const auto v = static_cast<unsigned char>(std::clamp(255.0 * (floor_level + (1.0 - floor_level) * shade), 0.0, 255.0));
            const auto a = static_cast<unsigned char>(std::clamp(alpha[o] * 255.0 + 0.5, 0.0, 255.0));
            opaque += a == 255 ? 1 : 0;
            covered += a > 0 ? 1 : 0;
            unsigned char* p = &out[18 + (static_cast<std::size_t>(height - 1 - y) * width + x) * 4];
            p[0] = v;
            p[1] = v;
            p[2] = v;
            p[3] = a;
        }
    }
    if (covered == 0) {
        std::printf("nothing drawn: every triangle is facing away or outside the frame\n");
        return 3;
    }
    FILE* f = std::fopen(argv[2], "wb");
    if (!f) {
        std::printf("cannot write %s\n", argv[2]);
        return 1;
    }
    std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    std::printf("wrote %s %dx%d, %d chunk(s), %d%% opaque\n", argv[2], width, height, static_cast<int>(m.chunks.size()),
                opaque * 100 / std::max(1, width * height));
    return 0;
}
