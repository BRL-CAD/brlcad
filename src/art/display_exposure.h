/*                          D I S P L A Y _ E X P O S U R E . H
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

#ifndef ART_DISPLAY_EXPOSURE_H
#define ART_DISPLAY_EXPOSURE_H

#include "common.h"

namespace foundation { class Image; class Tile; }

class ArtDisplayExposure
{
public:
    explicit ArtDisplayExposure(double stops = 0.0);
    bool apply(foundation::Image& image) const;
    bool apply(foundation::Tile& tile) const;

private:
    const double m_multiplier;
};

#endif

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
