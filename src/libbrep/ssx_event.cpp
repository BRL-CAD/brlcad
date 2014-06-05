/*                   S S X _ E V E N T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file ssx_event.cpp
 *
 * Extend the ssx_event routines provided by openNURBS.
 *
 */

#include "common.h"
#include "brep.h"

void
DumpSSXEvent(ON_SSX_EVENT &x, ON_TextLog &text_log)
{
    text_log.Print("m_type: ");
    switch (x.m_type) {
	case ON_SSX_EVENT::no_ssx_event:
	    text_log.Print("no_ssx_event");
	    break;
	case ON_SSX_EVENT::ssx_transverse:
	    text_log.Print("ssx_transverse");
	    break;
	case ON_SSX_EVENT::ssx_tangent:
	    text_log.Print("ssx_tangent");
	    break;
	case ON_SSX_EVENT::ssx_overlap:
	    text_log.Print("ssx_overlap");
	    break;
	case ON_SSX_EVENT::ssx_transverse_point:
	    text_log.Print("ssx_transverse_point");
	    break;
	case ON_SSX_EVENT::ssx_tangent_point:
	    text_log.Print("ssx_tangent_point");
	    break;
	default:
	    text_log.Print("illegal value");
	    break;
    }
    text_log.Print("\n");
    text_log.PushIndent();

    switch (x.m_type) {
	case ON_SSX_EVENT::ssx_transverse_point:
	case ON_SSX_EVENT::ssx_tangent_point:
	    // don't use %g so the text_log double format can control display precision
	    text_log.Print("SurfaceA(");
	    text_log.Print(x.m_pointA[0]);
	    text_log.Print(",");
	    text_log.Print(x.m_pointA[1]);
	    text_log.Print(") = \n");

	    text_log.Print("SurfaceB(");
	    text_log.Print(x.m_pointB[0]);
	    text_log.Print(",");
	    text_log.Print(x.m_pointB[1]);
	    text_log.Print(") = \n");

	    text_log.PushIndent();
	    text_log.Print(x.m_point3d);
	    text_log.Print("\n");
	    text_log.PopIndent();
	    break;

	case ON_SSX_EVENT::ssx_transverse:
	case ON_SSX_EVENT::ssx_tangent:
	case ON_SSX_EVENT::ssx_overlap:
	    text_log.Print("SurfaceA:\n");
	    text_log.PushIndent();
	    x.m_curveA->Dump(text_log);
	    text_log.PopIndent();

	    text_log.Print("SurfaceB:\n");
	    text_log.PushIndent();
	    x.m_curveB->Dump(text_log);
	    text_log.PopIndent();

	    text_log.Print("3D curves:\n");
	    text_log.PushIndent();
	    x.m_curve3d->Dump(text_log);
	    text_log.PopIndent();
	    break;

	case ON_SSX_EVENT::no_ssx_event:
	case ON_SSX_EVENT::ssx_32bit_enum:
	    // these extra cses keep gcc happy and quiet
	    break;
    }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
