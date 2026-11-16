// Copyright (c) 2026 RomekRF.
// SPDX-License-Identifier: MPL-2.0
//
// Released under the MPL-2.0 rather than held with RED+'s proprietary files. This reader
// implements a documented file format -- a format is a fact and a reader must follow it --
// but a few identifiers in it also appear in rafalh's rf-reversed (GPL-3.0) and nowhere else
// in the Alpine or Dash trees. Opening the file settles the question instead of arguing it.
// See README.md, "Credits".
#include "v3m_model.h"

#include <cstdio>
#include <cstring>
#include <cctype>
#include <algorithm>

namespace red::v3m
{
    namespace
    {
        constexpr std::uint32_t SIG_V3M = 0x52463344u;   // 'RF3D'
        constexpr std::uint32_t SIG_V3C = 0x5246434Du;   // 'RFCM'
        constexpr std::uint32_t V3D_VERSION = 0x40000u;

        constexpr std::uint32_t SEC_END  = 0x00000000u;
        constexpr std::uint32_t SEC_SUBM = 0x5355424Du;  // 'SUBM'
        constexpr std::uint32_t SEC_CSPH = 0x43535048u;  // 'CSPH'
        constexpr std::uint32_t SEC_BONE = 0x424F4E45u;  // 'BONE'

        constexpr std::uint32_t LOD_FLAG_ORIG_MAP   = 0x01u;
        constexpr std::uint32_t LOD_FLAG_TRI_PLANES = 0x20u;
        constexpr std::uint16_t TRI_FLAG_DOUBLE_SIDED = 0x20u;

        // Bounds-checked cursor. Every read goes through it, so a truncated or
        // malformed file fails the load instead of walking off the buffer -- this
        // parser runs on whatever the user points it at.
        struct Reader
        {
            const std::uint8_t* d;
            std::size_t n;
            std::size_t p = 0;
            bool bad = false;

            bool need(std::size_t count)
            {
                if (bad || p + count > n) { bad = true; return false; }
                return true;
            }
            std::int32_t i32()
            {
                if (!need(4)) return 0;
                std::int32_t v; std::memcpy(&v, d + p, 4); p += 4; return v;
            }
            std::uint32_t u32() { return static_cast<std::uint32_t>(i32()); }
            std::uint16_t u16()
            {
                if (!need(2)) return 0;
                std::uint16_t v; std::memcpy(&v, d + p, 2); p += 2; return v;
            }
            std::uint8_t u8()
            {
                if (!need(1)) return 0;
                return d[p++];
            }
            float f32()
            {
                if (!need(4)) return 0.0f;
                float v; std::memcpy(&v, d + p, 4); p += 4; return v;
            }
            void skip(std::size_t count) { if (need(count)) p += count; }
            std::string fixed_str(std::size_t count)
            {
                if (!need(count)) return std::string();
                std::size_t end = p;
                const std::size_t stop = p + count;
                while (end < stop && d[end] != 0) ++end;
                std::string s(reinterpret_cast<const char*>(d + p), end - p);
                p = stop;
                return s;
            }
            std::string cstr()
            {
                std::size_t end = p;
                while (end < n && d[end] != 0) ++end;
                if (end >= n) { bad = true; return std::string(); }
                std::string s(reinterpret_cast<const char*>(d + p), end - p);
                p = end + 1;
                return s;
            }
            // Absolute reads for the packed data block, which is addressed by
            // running offset rather than sequentially.
            bool at_f32(std::size_t off, float& out) const
            {
                if (off + 4 > n) return false;
                std::memcpy(&out, d + off, 4); return true;
            }
            bool at_u16(std::size_t off, std::uint16_t& out) const
            {
                if (off + 2 > n) return false;
                std::memcpy(&out, d + off, 2); return true;
            }
            bool at_i32(std::size_t off, std::int32_t& out) const
            {
                if (off + 4 > n) return false;
                std::memcpy(&out, d + off, 4); return true;
            }
        };

        struct Material
        {
            std::string name;
            std::uint32_t flags = 0;
        };

        struct TexRef
        {
            int id = 0;
            std::string filename;
        };

        struct ChunkInfo
        {
            std::uint16_t nv = 0, nf = 0, vecs_alloc = 0, faces_alloc = 0;
            std::uint16_t same_alloc = 0, wi_alloc = 0, uvs_alloc = 0;
            std::uint32_t render_flags = 0;
        };

        std::string stem(const std::string& name)
        {
            std::string f = name;
            const std::size_t slash = f.find_last_of("\\/");
            if (slash != std::string::npos) f = f.substr(slash + 1);
            const std::size_t dot = f.find_last_of('.');
            if (dot != std::string::npos && dot > 0) f = f.substr(0, dot);
            std::transform(f.begin(), f.end(), f.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return f;
        }

        // Mirrors VPPForge/Redux ResolveChunkTextureIndex: a chunk's stored index
        // may address the LOD texture list or the material list, with an
        // off-by-one seen in real files, so try each in the proven order.
        int resolve_chunk_texture(int raw, const std::vector<TexRef>& textures,
                                  const std::vector<Material>& materials)
        {
            if (materials.empty()) return 0;
            auto via_ref = [&](int ref_idx) -> int {
                if (ref_idx < 0 || ref_idx >= static_cast<int>(textures.size())) return -1;
                const TexRef& t = textures[ref_idx];
                if (t.id >= 0 && t.id < static_cast<int>(materials.size())) return t.id;
                if (t.id > 0 && (t.id - 1) < static_cast<int>(materials.size())) return t.id - 1;
                const std::string wanted = stem(t.filename);
                for (std::size_t i = 0; i < materials.size(); ++i) {
                    if (stem(materials[i].name) == wanted) return static_cast<int>(i);
                }
                return -1;
            };
            int slot = via_ref(raw);
            if (slot >= 0) return slot;
            slot = via_ref(raw - 1);
            if (slot >= 0) return slot;
            if (raw >= 0 && raw < static_cast<int>(materials.size())) return raw;
            if (raw > 0 && (raw - 1) < static_cast<int>(materials.size())) return raw - 1;
            return 0;
        }
    }

    bool load(const std::uint8_t* data, std::size_t size, Model& out, int requested_lod)
    {
        out = Model();
        if (requested_lod < 0 || requested_lod > 2) { out.error = "LOD must be 0, 1 or 2"; return false; }
        if (!data || size < 16) { out.error = "file too small"; return false; }

        Reader r{data, size};
        const std::uint32_t sig = r.u32();
        if (sig != SIG_V3M && sig != SIG_V3C) { out.error = "not a V3M/V3C file"; return false; }
        out.skeletal = (sig == SIG_V3C);
        const std::uint32_t version = r.u32();
        if (version != V3D_VERSION) { out.error = "unsupported V3D version"; return false; }

        r.i32();                       // num_submeshes -- sections are authoritative
        out.num_verts = r.i32();
        out.num_tris = r.i32();
        r.i32(); r.i32(); r.i32(); r.i32(); r.i32();

        bool have_bbox = false;
        while (!r.bad && r.p + 8 <= r.n) {
            const std::uint32_t sec_type = r.u32();
            if (sec_type == SEC_END) break;
            const std::int32_t sec_size = r.i32();
            const std::size_t body_start = r.p;

            // A submesh that runs off the end of the file must not sink the whole
            // model: several real assets (Blender exports on Romek's install) carry
            // a header claiming more submeshes than the file actually contains, and
            // a browser is far more useful showing the part that did read than
            // refusing the file outright. Whatever parsed is kept and marked partial.
            auto parse_submesh = [&]() -> bool {
                // ---- submesh header ----
                r.fixed_str(24);                       // name
                r.fixed_str(24);                       // parent
                r.i32();                               // version
                const std::int32_t num_lods = r.i32();
                if (r.bad || num_lods < 0 || num_lods > 64) { out.error = "bad LOD count"; return false; }
                out.lod_levels = (std::max)(out.lod_levels, num_lods);
                for (int i = 0; i < num_lods; ++i) r.f32();   // LOD distances
                // The part's centre is where RED puts the part: it draws c_m + v (STATUS EJ).
                // Everything below is moved into the model's frame with it.
                float off[3];
                off[0] = r.f32(); off[1] = r.f32(); off[2] = r.f32();
                r.f32();                               // radius
                float bmin[3], bmax[3];
                bmin[0] = r.f32(); bmin[1] = r.f32(); bmin[2] = r.f32();
                bmax[0] = r.f32(); bmax[1] = r.f32(); bmax[2] = r.f32();
                if (!r.bad) {
                    for (int k = 0; k < 3; ++k) {
                        out.bbox_min[k] = have_bbox ? (std::min)(out.bbox_min[k], bmin[k] + off[k]) : bmin[k] + off[k];
                        out.bbox_max[k] = have_bbox ? (std::max)(out.bbox_max[k], bmax[k] + off[k]) : bmax[k] + off[k];
                    }
                    have_bbox = true;
                }

                // ---- LODs: keep LOD 0's chunks, walk the rest to stay in sync ----
                struct PendingChunk { Chunk chunk; int raw_tex; };
                std::vector<PendingChunk> lod0;
                std::vector<TexRef> lod0_textures;

                for (int li = 0; li < num_lods && !r.bad; ++li) {
                    const std::uint32_t lod_flags = r.u32();
                    const std::int32_t lod_num_verts = r.i32();
                    const std::uint16_t num_chunks = r.u16();
                    const std::int32_t db_size = r.i32();
                    const std::size_t db_start = r.p;
                    if (db_size < 0) { out.error = "bad data block size"; return false; }
                    r.skip(static_cast<std::size_t>(db_size));
                    r.i32();                            // unknown

                    std::vector<ChunkInfo> infos(num_chunks);
                    for (std::uint16_t i = 0; i < num_chunks; ++i) {
                        infos[i].nv = r.u16();
                        infos[i].nf = r.u16();
                        infos[i].vecs_alloc = r.u16();
                        infos[i].faces_alloc = r.u16();
                        infos[i].same_alloc = r.u16();
                        infos[i].wi_alloc = r.u16();
                        infos[i].uvs_alloc = r.u16();
                        infos[i].render_flags = r.u32();
                    }
                    r.i32();                            // num prop points
                    const std::int32_t num_textures = r.i32();
                    if (r.bad || num_textures < 0 || num_textures > 4096) {
                        out.error = "bad texture count"; return false;
                    }
                    std::vector<TexRef> textures;
                    textures.reserve(static_cast<std::size_t>(num_textures));
                    for (int t = 0; t < num_textures; ++t) {
                        TexRef tr;
                        tr.id = r.u8();
                        tr.filename = r.cstr();
                        textures.push_back(tr);
                    }
                    if (r.bad) { out.error = "truncated texture list"; return false; }

                    if (li != (std::min)(requested_lod, num_lods - 1)) continue;
                    lod0_textures = textures;

                    // ---- unpack the data block (runs are 16-byte aligned) ----
                    const std::size_t db_base = db_start;
                    auto align16 = [db_base](std::size_t p) {
                        const std::size_t rel = (p - db_base) & 0xF;
                        return p + ((16 - rel) & 0xF);
                    };
                    for (std::uint16_t ci = 0; ci < num_chunks; ++ci) {
                        std::int32_t raw_tex = 0;
                        if (!r.at_i32(db_start + static_cast<std::size_t>(ci) * 56 + 0x20, raw_tex)) {
                            out.error = "truncated chunk header"; return false;
                        }
                        PendingChunk pc;
                        pc.raw_tex = raw_tex;
                        lod0.push_back(pc);
                    }
                    std::size_t dp = align16(db_start + static_cast<std::size_t>(num_chunks) * 56);
                    for (std::uint16_t ci = 0; ci < num_chunks; ++ci) {
                        const ChunkInfo& info = infos[ci];
                        // COUNTS COME FROM nv/nf, NOT FROM THE ALLOC SIZES. The allocs are the
                        // 16-byte ALIGNED run lengths, so dividing one back can yield one element
                        // MORE than the chunk holds, and that phantom element is assembled out of
                        // padding bytes: a stray vertex on 17% of stock chunks (harmless, nothing
                        // indexes it) and, on a chunk with an odd face count, a whole garbage
                        // TRIANGLE whose indices point anywhere -- which is how a perfectly valid
                        // 150k-vertex export first read back as corrupt. The engine has never had
                        // this problem: VifChunk carries num_vecs/num_faces and Alpine's renderer
                        // loops on those (gr_d3d11_mesh.cpp). Stride still comes from the allocs;
                        // only the counts change, and they are clamped so a malformed file cannot
                        // make us read past the run.
                        const int num_pos = (std::min)(static_cast<int>(info.nv), info.vecs_alloc / 12);
                        const int num_uvs = (std::min)(static_cast<int>(info.nv), info.uvs_alloc / 8);
                        const int num_faces = (std::min)(static_cast<int>(info.nf), info.faces_alloc / 8);
                        Chunk& c = lod0[ci].chunk;

                        c.pos.resize(static_cast<std::size_t>(num_pos) * 3);
                        for (std::size_t i = 0; i < c.pos.size(); ++i) {
                            if (!r.at_f32(dp + i * 4, c.pos[i])) { out.error = "truncated positions"; return false; }
                        }
                        for (std::size_t i = 0; i < c.pos.size(); ++i) c.pos[i] += off[i % 3];
                        dp = align16(dp + info.vecs_alloc);

                        c.norm.resize(static_cast<std::size_t>(num_pos) * 3);
                        for (std::size_t i = 0; i < c.norm.size(); ++i) {
                            if (!r.at_f32(dp + i * 4, c.norm[i])) { out.error = "truncated normals"; return false; }
                        }
                        dp = align16(dp + info.vecs_alloc);

                        c.uv.resize(static_cast<std::size_t>(num_uvs) * 2);
                        for (std::size_t i = 0; i < c.uv.size(); ++i) {
                            if (!r.at_f32(dp + i * 4, c.uv[i])) { out.error = "truncated uvs"; return false; }
                        }
                        dp = align16(dp + info.uvs_alloc);

                        c.idx.resize(static_cast<std::size_t>(num_faces) * 3);
                        c.tri_flags.resize(static_cast<std::size_t>(num_faces));
                        for (int i = 0; i < num_faces; ++i) {
                            std::uint16_t a = 0, b = 0, cc = 0, fl = 0;
                            const std::size_t fo = dp + static_cast<std::size_t>(i) * 8;
                            if (!r.at_u16(fo + 0, a) || !r.at_u16(fo + 2, b) ||
                                !r.at_u16(fo + 4, cc) || !r.at_u16(fo + 6, fl)) {
                                out.error = "truncated faces"; return false;
                            }
                            c.idx[i * 3 + 0] = a;
                            c.idx[i * 3 + 1] = b;
                            c.idx[i * 3 + 2] = cc;
                            c.tri_flags[i] = fl;
                            if (fl & TRI_FLAG_DOUBLE_SIDED) c.double_sided = true;
                        }
                        dp = align16(dp + info.faces_alloc);

                        if (lod_flags & LOD_FLAG_TRI_PLANES) {
                            dp = align16(dp + static_cast<std::size_t>(num_faces) * 16);
                        }
                        dp = align16(dp + info.same_alloc);
                        if (info.wi_alloc > 0) dp = align16(dp + info.wi_alloc);
                        if (lod_flags & LOD_FLAG_ORIG_MAP) {
                            dp = align16(dp + static_cast<std::size_t>(lod_num_verts) * 2);
                        }
                    }
                }

                // ---- materials ----
                const std::int32_t num_materials = r.i32();
                if (r.bad || num_materials < 0 || num_materials > 4096) {
                    out.error = "bad material count"; return false;
                }
                std::vector<Material> materials;
                materials.reserve(static_cast<std::size_t>(num_materials));
                for (int m = 0; m < num_materials; ++m) {
                    Material mat;
                    mat.name = r.fixed_str(32);
                    r.f32(); r.f32(); r.f32(); r.f32();   // emissive/specular/gloss/reflection
                    r.fixed_str(32);                      // reflection map
                    mat.flags = r.u32();
                    materials.push_back(mat);
                }
                const std::int32_t num_unknown1 = r.i32();
                if (num_unknown1 > 0) r.skip(static_cast<std::size_t>(num_unknown1) * 28);
                if (r.bad) { out.error = "truncated materials"; return false; }

                for (std::size_t i = 0; i < lod0.size(); ++i) {
                    PendingChunk& pc = lod0[i];
                    pc.chunk.material_index =
                        resolve_chunk_texture(pc.raw_tex, lod0_textures, materials);
                    if (!materials.empty()) {
                        pc.chunk.texture_name = materials[pc.chunk.material_index].name;
                    }
                    else if (pc.raw_tex >= 0 && pc.raw_tex < static_cast<int>(lod0_textures.size())) {
                        pc.chunk.texture_name = lod0_textures[pc.raw_tex].filename;
                    }
                    if (!pc.chunk.idx.empty()) out.chunks.push_back(pc.chunk);
                }
                return true;
            };
            if (sec_type == SEC_SUBM) {
                if (!parse_submesh()) {
                    out.partial = true;
                    break;
                }
            }
            else if (sec_type == SEC_CSPH) {
                r.fixed_str(24); r.i32(); r.f32(); r.f32(); r.f32(); r.f32();
            }
            else if (sec_type == SEC_BONE) {
                const std::int32_t nb = r.i32();
                if (nb < 0 || nb > 4096) { out.error = "bad bone count"; return false; }
                for (int b = 0; b < nb; ++b) {
                    r.fixed_str(24);
                    r.f32(); r.f32(); r.f32(); r.f32();
                    r.f32(); r.f32(); r.f32();
                    r.i32();
                }
            }
            else {
                if (sec_size > 0 && body_start + static_cast<std::size_t>(sec_size) <= r.n) {
                    r.p = body_start + static_cast<std::size_t>(sec_size);
                }
                else break;   // cannot skip safely
            }
        }

        if (out.chunks.empty()) {
            if (out.error.empty()) out.error = "no drawable geometry";
            return false;
        }
        if (out.partial) out.error.clear();   // partial is a warning, not a failure
        // The header's vertex/triangle counts are zero in real files (like the
        // SUBM size field), so report what was actually unpacked.
        out.num_verts = 0;
        out.num_tris = 0;
        for (std::size_t ci = 0; ci < out.chunks.size(); ++ci) {
            out.num_verts += static_cast<int>(out.chunks[ci].pos.size() / 3);
            out.num_tris += static_cast<int>(out.chunks[ci].idx.size() / 3);
        }

        // Unique texture names, first-seen order.
        for (std::size_t ci = 0; ci < out.chunks.size(); ++ci) {
            const std::string& name = out.chunks[ci].texture_name;
            if (name.empty()) continue;
            const std::string key = stem(name);
            bool seen = false;
            for (std::size_t t = 0; t < out.texture_names.size(); ++t) {
                if (stem(out.texture_names[t]) == key) { seen = true; break; }
            }
            if (!seen) out.texture_names.push_back(name);
        }
        // Some files carry a degenerate submesh bbox; fall back to the vertices.
        const bool bbox_empty = out.bbox_min[0] >= out.bbox_max[0] &&
                                out.bbox_min[1] >= out.bbox_max[1] &&
                                out.bbox_min[2] >= out.bbox_max[2];
        if (!have_bbox || bbox_empty) {
            bool first = true;
            for (std::size_t ci = 0; ci < out.chunks.size(); ++ci) {
                const Chunk& c = out.chunks[ci];
                for (std::size_t i = 0; i + 2 < c.pos.size(); i += 3) {
                    for (int k = 0; k < 3; ++k) {
                        const float v = c.pos[i + k];
                        out.bbox_min[k] = first ? v : (std::min)(out.bbox_min[k], v);
                        out.bbox_max[k] = first ? v : (std::max)(out.bbox_max[k], v);
                    }
                    first = false;
                }
            }
        }
        return true;
    }

    bool load_file(const char* path, Model& out, int requested_lod)
    {
        out = Model();
        FILE* fp = std::fopen(path, "rb");
        if (!fp) { out.error = "cannot open file"; return false; }
        std::fseek(fp, 0, SEEK_END);
        const long len = std::ftell(fp);
        std::fseek(fp, 0, SEEK_SET);
        if (len <= 0) { std::fclose(fp); out.error = "empty file"; return false; }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(len));
        const std::size_t got = std::fread(&bytes[0], 1, bytes.size(), fp);
        std::fclose(fp);
        if (got != bytes.size()) { out.error = "short read"; return false; }
        return load(&bytes[0], bytes.size(), out, requested_lod);
    }
}
