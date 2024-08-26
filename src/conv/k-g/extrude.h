/*                 E X T R U D E . H
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
 /** @file extrude.h
  *
  * LS Dyna keyword file to BRL-CAD converter:
  * intermediate extrude implementation
  */
#ifndef EXTRUDE_INCLUDED
#define EXTRUDE_INCLUDED

#include "common.h"
#include "wdb.h"



class Extrude {
public:
    Extrude(void);

    void                            setName(const char* value);

    const char*                     getName(void) const;

    void                            extrudeSection(std::string sectionName,const point_t& V, vect_t h, vect_t u_vec, vect_t v_vec, rt_sketch_internal* skt);

    std::vector<std::string>        write(rt_wdb* wdbp);
private:
    std::string          name;
    rt_extrude_internal* m_extrude;
};


#endif // !EXTRUDE_INCLUDED


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
