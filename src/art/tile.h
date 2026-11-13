/*                          T I L E . H
 * BRL-CAD
 *
 * Copyright (c) 2022-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#ifndef ART_TILE_H
#define ART_TILE_H

#include "common.h"

#include <atomic>
#include <cstdint>

#include "renderer/kernel/rendering/itilecallback.h"

struct fb;

// MasterRenderer serializes calls when given a single ITileCallback.
// Own the framebuffer for exactly one render, including failure cleanup.
class ArtTileCallback final : public renderer::ITileCallback
{
public:
    explicit ArtTileCallback(struct fb *framebuffer) : m_framebuffer(framebuffer) {}
    ~ArtTileCallback() override;
    ArtTileCallback(const ArtTileCallback&) = delete;
    ArtTileCallback& operator=(const ArtTileCallback&) = delete;

    void on_tiled_frame_begin(const renderer::Frame*) override;
    void on_tiled_frame_end(const renderer::Frame*) override {}
    void on_tile_begin(const renderer::Frame*, size_t, size_t, size_t, size_t) override {}
    void on_tile_end(const renderer::Frame*, size_t, size_t) override;
    void on_progressive_frame_update(const renderer::Frame&, double, std::uint64_t,
	double, std::uint64_t) override {}

    // The callback lives on the caller's stack.
    void release() override {}
    size_t completed_tiles() const { return m_completed_tiles; }
    size_t total_tiles() const { return m_total_tiles; }
    bool failed() const { return m_failed.load(std::memory_order_relaxed); }
    bool flush();
    bool finish();

private:
    struct fb *m_framebuffer;
    std::atomic<bool> m_failed{false};
    size_t m_completed_tiles = 0;
    size_t m_total_tiles = 0;
};

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
