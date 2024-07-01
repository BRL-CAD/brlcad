/*                           B O T . H
 * BRL-CAD
 *
 * Copyright (c) 2024 United States Government as represented by
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
/** @file Arb.h
 *
 * LS Dyna keyword file to BRL-CAD converter:
 * intermediate Arb data declaration
 */

#ifndef ARB_INCLUDED
#define ARB_INCLUDED

#include "common.h"

#include <string>

#include "vmath.h"
#include "rt/wdb.h"


class Arb {
public:
    Arb(void);
    ~Arb(void);

    void        setName(const char* value);

    void        addNodes(
			const point_t& point1,
			const point_t& point2,
			const point_t& point3,
			const point_t& point4,
			const point_t& point5,
			const point_t& point6,
			const point_t& point7,
			const point_t& point8);

    const char* getName(void) const;
    void        write(rt_wdb* wdbp);

private:
    std::string     name;
    rt_arb_internal arb_internal;

};


#endif //SOLID_INCLUDED


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
