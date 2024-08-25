/*                 S K E T C H . H
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
 /** @file sketch.h
  *
  * LS Dyna keyword file to BRL-CAD converter:
  * intermediate sketch implementation
  */
#ifndef SKETCH_INCLUDED
#define SKETCH_INCLUDED

#include "common.h"
#include "wdb.h"



class Sketch {
public:
    Sketch(void);

    void                            setName(const char* value);

    void                            creatSketch(std::string sketchName,std::string sectionType, const point_t& node1, const point_t& node2, const point_t& node3, std::vector<double> D);

    const char*                     getName(void) const;
    rt_sketch_internal*             getSketch(void) const;

    std::vector<std::string>        write(rt_wdb* wdbp);
private:
    std::string       name;
    rt_sketch_internal* m_sketch;
};


#endif // !SKETCH_INCLUDED


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8