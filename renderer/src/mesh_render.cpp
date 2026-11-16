// Copyright (c) 2026 RomekRF.
// SPDX-License-Identifier: MPL-2.0
#include "mesh_render.h"

#include <cmath>
#include <cstring>
#include <algorithm>

namespace red::mesh_render
{
    namespace
    {
        struct Vtx
        {
            float x, y, z;      // view space
            float sx, sy;       // screen space
            float inv_w;        // 1/view-z, for perspective-correct interpolation
            float u, v;         // texture coords, already divided by w
            float light;        // diffuse term, 0..1
            float light_back;   // same, for the flipped normal (two-sided lighting)
        };

        inline std::uint32_t blend_bg(std::uint32_t top, std::uint32_t bottom, float t)
        {
            const int r0 = (top >> 16) & 0xFF, g0 = (top >> 8) & 0xFF, b0 = top & 0xFF;
            const int r1 = (bottom >> 16) & 0xFF, g1 = (bottom >> 8) & 0xFF, b1 = bottom & 0xFF;
            const int r = static_cast<int>(r0 + (r1 - r0) * t);
            const int g = static_cast<int>(g0 + (g1 - g0) * t);
            const int b = static_cast<int>(b0 + (b1 - b0) * t);
            return 0xFF000000u | (static_cast<std::uint32_t>(r) << 16) |
                   (static_cast<std::uint32_t>(g) << 8) | static_cast<std::uint32_t>(b);
        }

        // Below kClipAlpha a texel contributes nothing; at or above kOpaqueAlpha it
        // is solid and can own the depth buffer. Everything between is soft and has
        // to be blended -- see the two-pass note in render().
        constexpr int kClipAlpha = 8;
        constexpr int kOpaqueAlpha = 248;

        inline std::uint32_t sample(const Texture& t, float u, float v)
        {
            // RF UVs tile; wrap rather than clamp or the seams on tiling props
            // (crates, pipes, canal walls) smear along the edge.
            int x = static_cast<int>(u * t.w);
            int y = static_cast<int>(v * t.h);
            x %= t.w; if (x < 0) x += t.w;
            y %= t.h; if (y < 0) y += t.h;
            return t.argb[static_cast<std::size_t>(y) * t.w + x];
        }
    }

    void render(const v3m::Model& model,
                const std::vector<const Texture*>& per_chunk_tex,
                const View& view,
                std::uint32_t* dst, int width, int height,
                std::uint32_t bg_top, std::uint32_t bg_bottom)
    {
        if (!dst || width <= 0 || height <= 0) return;

        for (int y = 0; y < height; ++y) {
            const float t = height > 1 ? static_cast<float>(y) / (height - 1) : 0.0f;
            const std::uint32_t c = blend_bg(bg_top, bg_bottom, t);
            std::uint32_t* row = dst + static_cast<std::size_t>(y) * width;
            for (int x = 0; x < width; ++x) row[x] = c;
        }
        if (model.chunks.empty()) return;

        std::vector<float> zbuf(static_cast<std::size_t>(width) * height, 1e30f);

        // Frame the model: orbit a camera around the bbox centre at a distance
        // that makes the bounding sphere just fill the view.
        const float cx = (model.bbox_min[0] + model.bbox_max[0]) * 0.5f;
        const float cy = (model.bbox_min[1] + model.bbox_max[1]) * 0.5f;
        const float cz = (model.bbox_min[2] + model.bbox_max[2]) * 0.5f;
        float radius = 0.0f;
        for (int k = 0; k < 3; ++k) {
            const float half = (model.bbox_max[k] - model.bbox_min[k]) * 0.5f;
            radius += half * half;
        }
        radius = std::sqrt(radius);
        if (!(radius > 1e-6f)) radius = 1.0f;

        // 52 degrees is a wide-angle lens. On a prop that fills the frame it bends the
        // verticals and makes every model look like it is being photographed from the
        // floor; 36 is closer to a product shot and the exact-fit solve below just moves
        // the camera back to compensate, so the model still fills the pane.
        const float fov_y = 0.62f;                        // ~36 degrees
        const float focal = 1.0f / std::tan(fov_y * 0.5f);
        const float zoom = view.zoom > 0.05f ? view.zoom : 0.05f;

        const float cyaw = std::cos(view.yaw), syaw = std::sin(view.yaw);
        const float cpit = std::cos(view.pitch), spit = std::sin(view.pitch);

        const float aspect = static_cast<float>(width) / static_cast<float>(height);

        // Frame it EXACTLY, by solving for the distance each bbox corner needs.
        //
        // This used to be dist = radius * 1.9, with radius the AABB half-diagonal --
        // i.e. the bounding sphere. That never actually fit: a sphere of radius R at
        // 1.9R subtends asin(1/1.9) = 0.554 rad per side against a half-FOV of 0.45,
        // so it overflows by about 23%. Compact models got away with it because their
        // silhouette is much smaller than their half-diagonal; long ones (a 30 m gate,
        // a corridor section) hung off the edge. Perspective made it worse, because
        // the near end of a deep model projects larger than the centre does.
        //
        // Screen space here is  ndc_x = x * focal / (aspect * z),  ndc_y = y * focal / z
        // with z = vz + dist, so a corner fits when
        //     dist >= |x| * focal / aspect - vz     and     dist >= |y| * focal - vz
        // Taking the max over all eight corners fits the whole box at any orientation,
        // with no per-model tuning and no reliance on the shape being roundish.
        float need = 0.0f;
        for (int corner = 0; corner < 8; ++corner) {
            const float mx = (corner & 1) ? model.bbox_max[0] : model.bbox_min[0];
            const float my = (corner & 2) ? model.bbox_max[1] : model.bbox_min[1];
            const float mz = (corner & 4) ? model.bbox_max[2] : model.bbox_min[2];
            const float px = -(mx - cx), py = my - cy, pz = mz - cz;
            const float rx = px * cyaw + pz * syaw;
            const float rz = -px * syaw + pz * cyaw;
            const float vy = py * cpit - rz * spit;
            const float vz = py * spit + rz * cpit;
            const float dh = std::fabs(rx) * focal / (aspect > 1e-4f ? aspect : 1e-4f) - vz;
            const float dv = std::fabs(vy) * focal - vz;
            if (dh > need) need = dh;
            if (dv > need) need = dv;
        }
        if (!(need > 1e-4f)) need = radius * 1.9f;        // degenerate bbox
        const float dist = (need * 1.06f) / zoom;         // 6% breathing room
        const float half_w = width * 0.5f;
        const float half_h = height * 0.5f;
        const float pan_px = view.pan_x * height;
        const float pan_py = view.pan_y * height;

        // Light from over the viewer's shoulder so detail reads at any angle.
        const float lx = 0.35f, ly = 0.72f, lz = -0.6f;
        const float llen = std::sqrt(lx * lx + ly * ly + lz * lz);

        std::vector<Vtx> vt;
        // Soft alpha needs TWO passes, not an alpha test.
        //
        // This used to drop any texel under alpha 24 and draw everything else solid.
        // That is fine for a grate or a chain-link fence, whose alpha is genuinely
        // 1-bit, and wrong for anything authored with soft edges: RF's effect
        // textures are 4-bit alpha (algaescum_medeaswamp01.tga has 16 distinct alpha
        // values, and 17.6% of its texels sit between the two extremes). Drawing all
        // of those fully opaque is what made VFX previews look grainy -- the speckle
        // was the alpha ramp being rounded to on/off.
        //
        // Pass 0 draws the solid texels and owns the depth buffer. Pass 1 blends the
        // soft ones over that result, depth-TESTED so geometry in front still hides
        // them, but without writing depth so they do not occlude each other. One
        // ordering case stays wrong -- soft over soft blends in draw order rather
        // than back to front -- which needs a per-fragment sort to fix and is not
        // worth it for a preview pane.
        //
        // Pass 1 is skipped entirely when no texture on the model has a soft texel,
        // so ordinary opaque props cost exactly what they did before.
        bool any_soft = false;
        for (const Texture* t : per_chunk_tex) {
            if (!t || !t->valid()) continue;
            if (t->soft_alpha < 0) {
                signed char found = 0;
                for (std::uint32_t px : t->argb) {
                    const int av = static_cast<int>(px >> 24);
                    if (av >= kClipAlpha && av < kOpaqueAlpha) { found = 1; break; }
                }
                t->soft_alpha = found;      // answered once per texture, not per frame
            }
            if (t->soft_alpha) { any_soft = true; break; }
        }

        for (int pass = 0; pass < (any_soft ? 2 : 1); ++pass) {
            for (std::size_t ci = 0; ci < model.chunks.size(); ++ci) {
                const v3m::Chunk& c = model.chunks[ci];
                const Texture* tex = ci < per_chunk_tex.size() ? per_chunk_tex[ci] : nullptr;
                if (tex && !tex->valid()) tex = nullptr;
                const std::size_t nverts = c.pos.size() / 3;
                if (nverts == 0 || c.idx.empty()) continue;
                const bool have_uv = c.uv.size() / 2 >= nverts;
                const bool have_norm = c.norm.size() / 3 >= nverts;

                vt.assign(nverts, Vtx());
                for (std::size_t i = 0; i < nverts; ++i) {
                    // Model -> view. RF is right-handed with X mirrored relative to
                    // screen space, matching the negate-X the VPPForge viewer applies.
                    float px = -(c.pos[i * 3 + 0] - cx);
                    float py = c.pos[i * 3 + 1] - cy;
                    float pz = c.pos[i * 3 + 2] - cz;

                    float rx = px * cyaw + pz * syaw;
                    float rz = -px * syaw + pz * cyaw;
                    float ry = py;

                    float vy = ry * cpit - rz * spit;
                    float vz = ry * spit + rz * cpit;

                    Vtx& o = vt[i];
                    o.x = rx;
                    o.y = vy;
                    o.z = vz + dist;
                    o.inv_w = o.z > 1e-4f ? 1.0f / o.z : 0.0f;
                    o.sx = half_w + (o.x * focal / aspect) * o.inv_w * half_w + pan_px;
                    o.sy = half_h - (o.y * focal) * o.inv_w * half_h + pan_py;

                    if (have_uv) {
                        o.u = c.uv[i * 2 + 0];
                        o.v = c.uv[i * 2 + 1];
                    }
                    float light = 0.75f;
                    float light_back = 0.75f;
                    if (have_norm) {
                        float nx = -c.norm[i * 3 + 0];
                        float ny = c.norm[i * 3 + 1];
                        float nz = c.norm[i * 3 + 2];
                        const float nrx = nx * cyaw + nz * syaw;
                        const float nrz = -nx * syaw + nz * cyaw;
                        const float nvy = ny * cpit - nrz * spit;
                        const float nvz = ny * spit + nrz * cpit;
                        const float nlen = std::sqrt(nrx * nrx + nvy * nvy + nvz * nvz);
                        if (nlen > 1e-6f && llen > 1e-6f) {
                            const float d = (nrx * lx + nvy * ly + nvz * lz) / (nlen * llen);
                            light = 0.32f + 0.68f * (d > 0.0f ? d : 0.0f);
                            // A double-sided triangle seen from behind has its normal
                            // pointing away, which would light it as if it were in
                            // shadow. Keep the flipped term so the back of a leaf card
                            // is lit like its front instead of going flat dark.
                            light_back = 0.32f + 0.68f * (-d > 0.0f ? -d : 0.0f);
                        }
                    }
                    o.light = light;
                    o.light_back = light_back;
                }

                const std::size_t ntris = c.idx.size() / 3;
                for (std::size_t t = 0; t < ntris; ++t) {
                    const std::uint16_t ia = c.idx[t * 3 + 0];
                    const std::uint16_t ib = c.idx[t * 3 + 1];
                    const std::uint16_t ic = c.idx[t * 3 + 2];
                    if (ia >= nverts || ib >= nverts || ic >= nverts) continue;
                    const Vtx& a = vt[ia];
                    const Vtx& b = vt[ib];
                    const Vtx& cc = vt[ic];
                    // Near-plane reject. The camera sits outside the bounding sphere,
                    // so a straddling triangle means a degenerate model, not a view
                    // the user is trying to get inside of.
                    if (a.z <= 0.05f || b.z <= 0.05f || cc.z <= 0.05f) continue;

                    const float area = (b.sx - a.sx) * (cc.sy - a.sy) - (cc.sx - a.sx) * (b.sy - a.sy);
                    const bool double_sided = (t < c.tri_flags.size() && (c.tri_flags[t] & 0x20)) ||
                                              c.double_sided;
                    if (area == 0.0f) continue;
                    if (area > 0.0f && !double_sided) continue;   // back face
                    const bool back_face = area > 0.0f;
                    const float inv_area = 1.0f / area;

                    int min_x = static_cast<int>(std::floor((std::min)((std::min)(a.sx, b.sx), cc.sx)));
                    int max_x = static_cast<int>(std::ceil((std::max)((std::max)(a.sx, b.sx), cc.sx)));
                    int min_y = static_cast<int>(std::floor((std::min)((std::min)(a.sy, b.sy), cc.sy)));
                    int max_y = static_cast<int>(std::ceil((std::max)((std::max)(a.sy, b.sy), cc.sy)));
                    if (min_x < 0) min_x = 0;
                    if (min_y < 0) min_y = 0;
                    if (max_x > width - 1) max_x = width - 1;
                    if (max_y > height - 1) max_y = height - 1;
                    if (min_x > max_x || min_y > max_y) continue;

                    for (int y = min_y; y <= max_y; ++y) {
                        const float py = y + 0.5f;
                        std::uint32_t* row = dst + static_cast<std::size_t>(y) * width;
                        float* zrow = &zbuf[static_cast<std::size_t>(y) * width];
                        for (int x = min_x; x <= max_x; ++x) {
                            const float px = x + 0.5f;
                            float w0 = ((b.sx - a.sx) * (py - a.sy) - (px - a.sx) * (b.sy - a.sy)) * inv_area;
                            float w1 = ((px - a.sx) * (cc.sy - a.sy) - (cc.sx - a.sx) * (py - a.sy)) * inv_area;
                            const float w2 = 1.0f - w0 - w1;
                            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
                            // w1 weights b, w0 weights c, w2 weights a
                            const float inv_w = w2 * a.inv_w + w1 * b.inv_w + w0 * cc.inv_w;
                            if (inv_w <= 0.0f) continue;
                            const float depth = 1.0f / inv_w;
                            // Coplanar surfaces must not interleave per pixel. RF effect
                            // meshes carry BOTH windings of the same quad and flag them
                            // double-sided, so front and back land on identical depths and
                            // float noise decides each pixel independently -- which is what
                            // made VFX previews look like static (algae_medeaswamp03.vfx
                            // rendered as two grey levels of pure speckle, mean RGB
                            // 64.8/65.1/65.7, only 20 distinct colours in the whole quad).
                            // Requiring a fragment to be meaningfully nearer makes the first
                            // one drawn win every pixel, and two-sided lighting above means
                            // it does not matter which of the pair that is.
                            if (depth >= zrow[x] - depth * 1e-4f) continue;

                            float shade = back_face
                                ? (w2 * a.light_back + w1 * b.light_back + w0 * cc.light_back)
                                : (w2 * a.light + w1 * b.light + w0 * cc.light);
                            std::uint32_t texel = 0xFFB4B4B4u;
                            int alpha = 255;
                            if (tex) {
                                const float u = (w2 * a.u * a.inv_w + w1 * b.u * b.inv_w + w0 * cc.u * cc.inv_w) / inv_w;
                                const float v = (w2 * a.v * a.inv_w + w1 * b.v * b.inv_w + w0 * cc.v * cc.inv_w) / inv_w;
                                texel = sample(*tex, u, v);
                                alpha = static_cast<int>((texel >> 24) & 0xFF);
                            }
                            if (pass == 0) {
                                if (alpha < kOpaqueAlpha) continue;      // soft: pass 1
                            }
                            else if (alpha >= kOpaqueAlpha || alpha < kClipAlpha) {
                                continue;                                // solid, or nothing
                            }
                            if (shade < 0.0f) shade = 0.0f;
                            if (shade > 1.6f) shade = 1.6f;
                            int r = static_cast<int>(((texel >> 16) & 0xFF) * shade);
                            int g = static_cast<int>(((texel >> 8) & 0xFF) * shade);
                            int bl = static_cast<int>((texel & 0xFF) * shade);
                            if (r > 255) r = 255;
                            if (g > 255) g = 255;
                            if (bl > 255) bl = 255;
                            if (pass == 0) {
                                row[x] = 0xFF000000u | (static_cast<std::uint32_t>(r) << 16) |
                                         (static_cast<std::uint32_t>(g) << 8) | static_cast<std::uint32_t>(bl);
                                zrow[x] = depth;
                            }
                            else {
                                const std::uint32_t dst = row[x];
                                const int ia = 255 - alpha;
                                r  = (r  * alpha + static_cast<int>((dst >> 16) & 0xFF) * ia) / 255;
                                g  = (g  * alpha + static_cast<int>((dst >> 8) & 0xFF) * ia) / 255;
                                bl = (bl * alpha + static_cast<int>(dst & 0xFF) * ia) / 255;
                                row[x] = 0xFF000000u | (static_cast<std::uint32_t>(r) << 16) |
                                         (static_cast<std::uint32_t>(g) << 8) | static_cast<std::uint32_t>(bl);
                                // No depth write: soft texels must not occlude each other,
                                // or the first leaf drawn would punch a hole in the rest.
                            }
                        }
                    }
                }
            }
        }
    }
}
