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
 /** @file geometry.h
  *
  * LS Dyna keyword file to BRL-CAD converter:
  * intermediate geometry structure implementation
  */
#ifndef GEOMETRY_INCLUDED
#define GEOMETRY_INCLUDED

#include "common.h"

#include "bot.h"
#include "arbs.h"
#include "pipe.h"
#include "sketch.h"
#include "extrude.h"

struct BeamResultant {
    Sketch  skt;
    Extrude ext;
};

class Geometry {
public:

    void         setBaseName(const char* value);
    void         setThickness(double value);
    void         addTriangle(const point_t& point1,
			     const point_t& point2,
			     const point_t& point3);
    void         addArb(const char* arbName,
			const point_t& point1,
			const point_t& point2,
			const point_t& point3,
			const point_t& point4,
			const point_t& point5,
			const point_t& point6,
			const point_t& point7,
			const point_t& point8);
    void         addPipePnt(pipePoint point);
    void         addSketch(std::string name, std::string sectionType, const point_t& node1, const point_t& node2, const point_t& node3, std::vector<double> D);
    void         addBeamResultant(std::string sectionName,std::string sectionType, const point_t& node1, const point_t& node2, const point_t& node3, std::vector<double> D);
    const char*  getBaseName(void) const;
    Bot&         getBot(void);
    Arbs&        getArbs(void);
    Pipe&        getPipe(void);

    std::vector<std::string>       write(rt_wdb* wdbp);
private:
    std::string                name;
    Bot                        m_bot;
    Arbs                       m_arbs;
    Pipe                       m_pipe;
    std::vector<BeamResultant> m_BeamsResultant;
};


#endif // !GEOMETRY_INCLUDED


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
