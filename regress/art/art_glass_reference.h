/*              A R T _ G L A S S _ R E F E R E N C E . H
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
#ifndef ART_GLASS_REFERENCE_H
#define ART_GLASS_REFERENCE_H

float interface_transmission(float ni, float nt, float ci, float ct)
{
    float rs = (ni * ci - nt * ct) / (ni * ci + nt * ct);
    float rp = (nt * ci - ni * ct) / (nt * ci + ni * ct);
    return 1 - 0.5 * (rs * rs + rp * rp);
}

float interface_radiance_transmission(float ni, float nt, float ci, float ct)
{
    // Radiance divided by index squared is invariant under refraction.
    float ratio = ni / nt;
    return interface_transmission(ni, nt, ci, ct) * ratio * ratio;
}

color target_color(point position)
{
    return color(0.35 + 0.25 * sin(0.17 * position[0]),
        0.35 + 0.25 * sin(0.19 * position[1]),
        0.35 + 0.25 * cos(0.11 * position[0] + 0.13 * position[1]));
}

#endif
