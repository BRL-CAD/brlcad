/*                          D I S P L A Y _ E X P O S U R E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#include <algorithm>
#include <cmath>
#include <limits>

#include "foundation/image/color.h"
#include "foundation/image/image.h"
#include "foundation/image/tile.h"

#include "display_exposure.h"
#include "bu/log.h"
#include "vmath.h"

namespace {
double
exposure_multiplier(double stops)
{
    // Extreme finite stops have well-defined black or white display results.
    // Avoid asking exp2 to overflow or underflow beyond the double range.
    if (stops >= std::numeric_limits<double>::max_exponent)
	return std::numeric_limits<double>::infinity();
    if (stops < std::numeric_limits<double>::min_exponent - std::numeric_limits<double>::digits)
	return 0.0;
    return std::exp2(stops);
}
}

ArtDisplayExposure::ArtDisplayExposure(double stops)
    : m_multiplier(exposure_multiplier(stops))
{
}

bool
ArtDisplayExposure::apply(foundation::Tile& tile) const
{
    if (EQUAL(m_multiplier, 1.0))
	return true;
    if (tile.get_channel_count() != 4) {
	bu_log("art: display exposure requires an RGBA beauty image\n");
	return false;
    }
    for (size_t i = 0; i < tile.get_pixel_count(); ++i) {
	foundation::Color4f pixel;
	tile.get_pixel(i, pixel);
	for (size_t channel = 0; channel < 3; ++channel) {
	    // Preserve zero even when a positive exposure saturates the multiplier.
	    pixel[channel] = pixel[channel] > 0.0f
		? static_cast<float>(std::min(1.0, static_cast<double>(pixel[channel]) * m_multiplier)) : 0.0f;
	}
	tile.set_pixel(i, pixel);
    }
    return true;
}

bool
ArtDisplayExposure::apply(foundation::Image& image) const
{
    if (EQUAL(m_multiplier, 1.0))
	return true;
    const auto& properties = image.properties();
    for (size_t y = 0; y < properties.m_tile_count_y; ++y)
	for (size_t x = 0; x < properties.m_tile_count_x; ++x)
	    if (!apply(image.tile(x, y)))
		return false;
    return true;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
