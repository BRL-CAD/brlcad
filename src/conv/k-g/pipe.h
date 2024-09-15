/*                 P I P E . H
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
 /** @file pipe.h
  *
  * LS Dyna keyword file to BRL-CAD converter:
  * intermediate pipe implementation
  */
#ifndef PIPE_INCLUDED
#define PIPE_INCLUDED

#include "common.h"
#include "wdb.h"

struct pipePoint {
    point_t   coords;
    double    innerDiameter;
    double    outerDiameter;
};

class Pipe {
public:
    Pipe(void);

    void                            setName(const char* value);

    void                            addPipePnt(const pipePoint& point);

    rt_pipe_internal*               getPipe(void) const;

    std::vector<std::string>        write(rt_wdb* wdbp);
private:
    std::string       name;
    rt_pipe_internal* m_pipe;
};


#endif // !PIPE_INCLUDED


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
