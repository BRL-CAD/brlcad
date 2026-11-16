/*                        T I L E . C P P
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

#include "common.h"

#include <limits>

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wall"
#  pragma GCC diagnostic ignored "-Wextra"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wdocumentation"
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wpedantic"
#  pragma clang diagnostic ignored "-Wignored-qualifiers"
#endif

#include "tile.h"
#include "renderer/api/frame.h"
#include "foundation/image/conversion.h"
#include "foundation/image/pixel.h"
#include "foundation/image/image.h"
#include "foundation/image/tile.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

#include "bu/log.h"
#include "dm.h"


ArtTileCallback::~ArtTileCallback()
{
    finish();
}


bool
ArtTileCallback::flush()
{
    if (m_framebuffer && fb_flush(m_framebuffer) < 0) {
	bu_log("art: could not flush framebuffer\n");
	m_failed = true;
    }
    return !m_failed;
}


bool
ArtTileCallback::finish()
{
    if (m_framebuffer) {
	if (fb_close(m_framebuffer) < 0) {
	    bu_log("art: could not close framebuffer\n");
	    m_failed = true;
	}
	m_framebuffer = FB_NULL;
    }
    return !m_failed;
}


void
ArtTileCallback::on_tiled_frame_begin(const renderer::Frame* frame)
{
    const size_t tiles = frame->image().properties().m_tile_count;
    if (!m_passes || tiles > std::numeric_limits<size_t>::max() / m_passes) {
	bu_log("art: render pass tile count exceeds the supported range\n");
	m_failed = true;
	return;
    }
    // Native callbacks begin each pass; cumulative work belongs to one render.
    m_total_tiles = tiles * m_passes;
}


void
ArtTileCallback::on_tile_end(const renderer::Frame* frame, const size_t tile_x, const size_t tile_y)
{
    ++m_completed_tiles;
    if (m_failed || !m_framebuffer)
	return;
    const auto& image = frame->image();
    const auto& properties = image.properties();
    const auto& tile = image.tile(tile_x, tile_y);
    if (tile.get_channel_count() != 4) {
	bu_log("art: framebuffer requires an RGBA beauty image\n");
	m_failed = true;
	return;
    }

    // Match the half-float and sRGB path used by Frame::write_main_image.
    // The PNG writer can differ by a byte through dithering and rounding.
    foundation::Tile exposed(tile, foundation::PixelFormatFloat);
    if (!m_exposure.apply(exposed)) {
	m_failed = true;
	return;
    }
    foundation::Tile display(exposed, foundation::PixelFormatHalf);
    foundation::convert_linear_rgb_to_srgb(display);
    const size_t rgb_channels[] = {0, 1, 2, foundation::Pixel::SkipChannel};
    const foundation::Tile rgb(display, foundation::PixelFormatUInt8, rgb_channels);
    const size_t x_origin = tile_x * properties.m_tile_width;
    const size_t y_origin = tile_y * properties.m_tile_height;
    for (size_t y = 0; y < rgb.get_height(); ++y) {
	const size_t fb_y = properties.m_canvas_height - 1 - (y_origin + y);
	const ssize_t written = fb_write(m_framebuffer, static_cast<int>(x_origin),
	    static_cast<int>(fb_y), rgb.pixel(0, y), rgb.get_width());
	if (written != static_cast<ssize_t>(rgb.get_width())) {
	    bu_log("art: could not write framebuffer scanline\n");
	    m_failed = true;
	    return;
	}
    }
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
