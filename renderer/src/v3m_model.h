// Copyright (c) 2026 RomekRF.
// SPDX-License-Identifier: MPL-2.0
//
// Released under the MPL-2.0 rather than held with RED+'s proprietary files. This reader
// implements a documented file format -- a format is a fact and a reader must follow it --
// but a few identifiers in it also appear in rafalh's rf-reversed (GPL-3.0) and nowhere else
// in the Alpine or Dash trees. Opening the file settles the question instead of arguing it.
// See README.md, "Credits".
#pragma once

// V3M / V3C static-mesh geometry reader.
//
// Deliberately free of RED/engine dependencies so it can be unit-tested headless
// (tools/v3m_dump) and reused by the mesh browser's preview renderer. The binary
// layout follows Romek's own VPPForge v3m.js reader, which is validated against
// the retail files -- same author, same format knowledge, so this is a port of a
// proven parser rather than a fresh guess at the format.

#include <cstdint>
#include <string>
#include <vector>

namespace red::v3m
{
    // One drawable run of triangles: its own vertex arrays, indices, and the
    // material slot (hence texture) the whole run uses.
    struct Chunk
    {
        std::vector<float> pos;          // 3 per vertex
        std::vector<float> norm;         // 3 per vertex
        std::vector<float> uv;           // 2 per vertex
        std::vector<std::uint16_t> idx;  // 3 per triangle
        std::vector<std::uint16_t> tri_flags;
        int material_index = 0;
        std::string texture_name;
        bool double_sided = false;
    };

    struct Model
    {
        std::vector<Chunk> chunks;                // requested LOD of every submesh
        std::vector<std::string> texture_names;   // unique, first-seen order
        float bbox_min[3] = {0.0f, 0.0f, 0.0f};
        float bbox_max[3] = {0.0f, 0.0f, 0.0f};
        int num_verts = 0;
        int num_tris = 0;
        int lod_levels = 1;                      // maximum across parts
        bool skeletal = false;                    // V3C
        bool partial = false;                     // file ended mid-model; chunks kept
        std::string error;                        // empty when load succeeded
    };

    // Parses the requested LOD of every submesh (default highest detail). Parts
    // with fewer LODs use their last available level. Returns false and fills
    // Model::error on malformed input; never throws and never reads out of range.
    bool load(const std::uint8_t* data, std::size_t size, Model& out, int requested_lod = 0);
    bool load_file(const char* path, Model& out, int requested_lod = 0);
}
