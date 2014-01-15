/*                         B O T . H
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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
/** @file Bot.h
 *
 * RAW geometry file to BRL-CAD converter:
 * bag of triangles intermediate data structure declaration
 *
 *  Origin -
 *	IABG mbH (Germany)
 */

#ifndef CONV_RAW_BOT_H
#define CONV_RAW_BOT_H

#include "common.h"
#include <vector>
#include "wdb.h"


class Bot {
public:
    Bot(void);

    void               setName(const std::string& value);
    void               setThickness(double value);
    size_t             addPoint(point_t& point);
    void               addTriangle(int  a,
				   int  b,
				   int  c);

    const std::string& name(void) const;
    void               write(rt_wdb* wdbp) const;


private:
    std::string          m_name;
    double               m_thickness;
    std::vector<fastf_t> m_vertices;
    std::vector<int>     m_faces;

    void writeSolid(rt_wdb* wdbp) const;
    void writePlate(rt_wdb* wdbp) const;
};


#endif /* CONV_RAW_BOT_H */
