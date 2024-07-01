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
/** @file solid.h
 *
 * LS Dyna keyword file to BRL-CAD converter:
 * intermediate solid data declaration
 */

#ifndef SOLID_INCLUDED
#define SOLID_INCLUDED

#include "common.h"

#include <string>

#include "vmath.h"
#include "rt/wdb.h"
#include "arb.h"


class Solid {
public:
    Solid(void);
    ~Solid(void);

    void        setName(const char* value);

    void        addArb(Arb arb);

    const char*             getName(void) const;
    std::vector<Arb>        getArbs(void) const;

    void        write(rt_wdb* wdbp);

private:
    std::string     name;
    std::vector<Arb> arbs;

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
