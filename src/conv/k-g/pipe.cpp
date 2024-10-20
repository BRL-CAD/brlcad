/*                 P I P E . C P P
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
 /** @file pipe.cpp
  *
  * LS Dyna keyword file to BRL-CAD converter:
  * intermediate pipe implementation
  */

#include "pipe.h"


Pipe::Pipe(void)
{
    BU_GET(m_pipe, rt_pipe_internal);
    m_pipe->pipe_magic = RT_PIPE_INTERNAL_MAGIC;
    BU_LIST_INIT(&(m_pipe->pipe_segs_head));
}


void Pipe::setName
(
    const char* value
) {
    if (value != nullptr)
	m_name = value;
    else
	m_name = "";
}


void Pipe::addPoint
(
    const pipePoint& point
) {
    struct wdb_pipe_pnt* ctlPoint;
    BU_ALLOC(ctlPoint, struct wdb_pipe_pnt);
    ctlPoint->l.magic = WDB_PIPESEG_MAGIC;

    BU_ALLOC(ctlPoint, struct wdb_pipe_pnt);

    VMOVE(ctlPoint->pp_coord, point.coords);
    ctlPoint->pp_id = point.innerDiameter;
    ctlPoint->pp_od = point.outerDiameter;
    ctlPoint->pp_bendradius = 2*point.outerDiameter;

    BU_LIST_PUSH(&(m_pipe->pipe_segs_head), &(ctlPoint->l));
    m_pipe->pipe_count += 1;
}


rt_pipe_internal* Pipe::getPipe(void) const
{
    return m_pipe;
}


std::vector<std::string> Pipe::write
(
    rt_wdb* wdbp
) {
    std::vector<std::string> ret;

    std::string pipeName = m_name;
    pipeName += ".pipe";


    rt_pipe_internal* pipe_wdb;
    BU_GET(pipe_wdb, rt_pipe_internal);
    pipe_wdb->pipe_magic = RT_PIPE_INTERNAL_MAGIC;
    BU_LIST_INIT(&pipe_wdb->pipe_segs_head);
    BU_LIST_APPEND_LIST(&pipe_wdb->pipe_segs_head, &m_pipe->pipe_segs_head);
    pipe_wdb->pipe_count = m_pipe->pipe_count;

    if (pipe_wdb->pipe_count > 0) {
	wdb_export(wdbp, pipeName.c_str(), pipe_wdb, ID_PIPE, 1);
	ret.push_back(pipeName);
    }

    return ret;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
