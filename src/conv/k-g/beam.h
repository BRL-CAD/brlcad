/*                 G E O M E T R Y . H
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
 /** @file beam.h
  *
  * LS Dyna keyword file to BRL-CAD converter:
  * intermediate beam implementation
  */
#ifndef BEAM_INCLUDED
#define BEAM_INCLUDED

#include "common.h"
#include "wdb.h"

struct beamPoint {
    point_t   coords;
    double    innerDiameter;
    double    outerDiameter;
};

class Beam {
public:
    Beam(void);

    void                            setName(const char* value);

    void                            addBeam(const char* beamName,
					    const beamPoint& point1,
					    const beamPoint& point2);

    void                            addBeam(const char* beamName,
					    const beamPoint& point1,
					    const beamPoint& point2,
					    const beamPoint& point3);

    std::vector<std::string>       write(rt_wdb* wdbp);
private:
    std::string                             name;
    std::map<std::string, bu_list>          m_list;
    //rt_pipe_internal m_pipe;
};


#endif // !BEAM_INCLUDED


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8