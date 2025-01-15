/*                           B O T . H
 * BRL-CAD
 *
 * Copyright (c) 2024-2025 United States Government as represented by
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
/** @file bot.h
 *
 * LS Dyna keyword file to BRL-CAD converter:
 * intermediate bag of triangles data declaration
 */

#ifndef BOT_INCLUDED
#define BOT_INCLUDED

#include "common.h"

#include <string>

#include "vmath.h"
#include "rt/wdb.h"


class Bot {
public:
    Bot(void);
    ~Bot(void);

    void                      setName(const char* value);
    void                      setThickness(double value);
    void                      addTriangle(const point_t& point1,
					  const point_t& point2,
					  const point_t& point3);

    std::vector <std::string> write(rt_wdb* wdbp);

private:
    std::string     m_name;
    rt_bot_internal m_botInternal;
    double          m_thickness;
};


#endif // !BOT_INCLUDED


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
