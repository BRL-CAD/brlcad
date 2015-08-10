/*                  P X _ E V E N T . C P P
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
/** @file px_event.cpp
 *
 * Implementation of ON_PX_EVENT.
 *
 */

#include "common.h"
#include "brep.h"

ON_PX_EVENT::ON_PX_EVENT()
{
    memset(this,0,sizeof(*this));
}

int
ON_PX_EVENT::Compare(const ON_PX_EVENT* a, const ON_PX_EVENT* b)
{
    if (!a) {
	return b ? 1 : 0;
    }

    if (!b)
	return -1;

    return a->m_Mid < b->m_Mid;
}

bool
ON_PX_EVENT::IsValid(ON_TextLog*,
		     double,
		     const class ON_3dPoint*,
		     const class ON_3dPoint*,
		     const class ON_Curve*,
		     const class ON_Interval*,
		     const class ON_Surface*,
		     const class ON_Interval*,
		     const class ON_Interval*) const
{
    // Implement later.
    return true;
}

void
ON_PX_EVENT::Dump(ON_TextLog& text_log) const
{
    text_log.Print("m_type: ");
    switch (m_type) {
	case ON_PX_EVENT::no_px_event:
	    text_log.Print("no_px_event");
	    break;
	case ON_PX_EVENT::ppx_point:
	    text_log.Print("ppx_point");
	    break;
	case ON_PX_EVENT::pcx_point:
	    text_log.Print("pcx_point");
	    break;
	case ON_PX_EVENT::psx_point:
	    text_log.Print("psx_point");
	    break;
	default:
	    text_log.Print("illegal value");
	    break;
    }
    text_log.Print("\n");
    text_log.PushIndent();

    text_log.Print("Intersection Point: \n");
    text_log.PushIndent();
    text_log.Print(m_Mid);
    text_log.Print("\n");
    text_log.PopIndent();
    text_log.Print("With uncertainty radius: \n");
    text_log.PushIndent();
    text_log.Print(m_radius);
    text_log.Print("\n");
    text_log.PopIndent();
    text_log.PopIndent();

    text_log.Print("pointA = \n");
    text_log.PushIndent();
    text_log.Print(m_A);
    text_log.Print("\n");
    text_log.PopIndent();

    switch (m_type) {
	case ON_PX_EVENT::ppx_point:
	    text_log.Print("pointB = \n");
	    break;

	case ON_PX_EVENT::pcx_point:
	    text_log.Print("curveB(");
	    text_log.Print(m_b[0]);
	    text_log.Print(") = \n");
	    break;

	case ON_PX_EVENT::psx_point:
	    text_log.Print("surfaceB");
	    text_log.Print(m_b);
	    text_log.Print(" = \n");
	    break;

	case ON_PX_EVENT::no_px_event:
	    break;
    }

    text_log.PushIndent();
    text_log.Print(m_B);
    text_log.Print("\n");
    text_log.PopIndent();

    text_log.PopIndent();
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
