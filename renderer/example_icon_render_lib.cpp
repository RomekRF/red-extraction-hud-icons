// Copyright (c) 2026 RomekRF.
// SPDX-License-Identifier: MPL-2.0
//
// Example for icon_render_lib.h: render a weapon icon straight from the game's packfile, in memory.
//
//   example_icon_render_lib <meshes.vpp> <mesh name> <out.tga> [width height yaw pitch zoom rotate_ccw]
//   example_icon_render_lib "C:\Games\Red Faction\meshes.vpp" Weapon_Aslt_Rifle.v3m assault_rifle.tga
//
// Inside a client you would get the mesh bytes from the engine's own file system instead of reading the VPP here.
// Build: build_example.bat (same compiler as build.bat).
#include "icon_render_lib.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>

// One file out of a VPP v1 packfile (Red Faction's .vpp), or an empty vector.
static std::vector<std::uint8_t> read_from_vpp(const char* vpp_path, const char* name)
{
    std::ifstream f(vpp_path, std::ios::binary);
    std::vector<std::uint8_t> vpp((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto u32 = [&](std::size_t at) {
        return at + 4 <= vpp.size() ? static_cast<std::uint32_t>(vpp[at] | vpp[at + 1] << 8 | vpp[at + 2] << 16 |
                                                                 static_cast<std::uint32_t>(vpp[at + 3]) << 24)
                                    : 0u;
    };
    constexpr std::size_t block = 0x800;
    if (u32(0) != 0x51890ACEu) {
        return {};
    }
    const std::size_t count = u32(8);
    std::size_t offset = block + (count * 64 + block - 1) / block * block;
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t entry = block + i * 64;
        if (entry + 64 > vpp.size()) {
            break;
        }
        char entry_name[61] = {};
        std::memcpy(entry_name, &vpp[entry], 60);
        const std::size_t size = u32(entry + 60);
        if (_stricmp(entry_name, name) == 0 && offset + size <= vpp.size()) {
            return {vpp.begin() + offset, vpp.begin() + offset + size};
        }
        offset += (size + block - 1) / block * block;
    }
    return {};
}

// 32-bit uncompressed TGA, bottom-up, as Red Faction loads it.
static bool write_tga(const char* path, const icon_render::Image& img)
{
    std::uint8_t header[18] = {};
    header[2] = 2;
    header[12] = img.width & 255;
    header[13] = (img.width >> 8) & 255;
    header[14] = img.height & 255;
    header[15] = (img.height >> 8) & 255;
    header[16] = 32;
    header[17] = 0x08;
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(header), sizeof(header));
    for (int y = img.height - 1; y >= 0; --y) {
        for (int x = 0; x < img.width; ++x) {
            const std::uint8_t* p = &img.rgba[(static_cast<std::size_t>(y) * img.width + x) * 4];
            const char bgra[4] = {static_cast<char>(p[2]), static_cast<char>(p[1]), static_cast<char>(p[0]),
                                  static_cast<char>(p[3])};
            f.write(bgra, 4);
        }
    }
    return static_cast<bool>(f);
}

int main(int argc, char** argv)
{
    if (argc < 4) {
        std::puts("usage: example_icon_render_lib <meshes.vpp> <mesh name> <out.tga> [width height yaw pitch zoom rotate_ccw]");
        return 2;
    }
    const std::vector<std::uint8_t> mesh = read_from_vpp(argv[1], argv[2]);
    if (mesh.empty()) {
        std::printf("%s not found in %s\n", argv[2], argv[1]);
        return 1;
    }
    icon_render::Options opt;   // defaults: 256x128 side view, muzzle left
    if (argc >= 10) {
        opt.width = std::atoi(argv[4]);
        opt.height = std::atoi(argv[5]);
        opt.yaw = static_cast<float>(std::atof(argv[6]));
        opt.pitch = static_cast<float>(std::atof(argv[7]));
        opt.zoom = static_cast<float>(std::atof(argv[8]));
        opt.rotate_ccw = std::atoi(argv[9]);
    }
    icon_render::Image img;
    if (!icon_render::render_icon(mesh.data(), mesh.size(), opt, img)) {
        std::printf("render failed: %s\n", img.error.c_str());
        return 1;
    }
    if (!write_tga(argv[3], img)) {
        std::printf("cannot write %s\n", argv[3]);
        return 1;
    }
    std::printf("wrote %s %dx%d\n", argv[3], img.width, img.height);
    return 0;
}
