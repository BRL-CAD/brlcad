/*                  S U B S U R F A C E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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

#include "common.h"
#include "brep.h"

Subsurface::Subsurface()
    : m_surf(NULL), m_isplanar(false)
{
    m_children[0] = m_children[1] = m_children[2] = m_children[3] = NULL;
}

Subsurface::Subsurface(ON_Surface *surf)
{
    m_surf = surf;
    if (surf) {
	SetBBox(surf->BoundingBox());
	m_u = surf->Domain(0);
	m_v = surf->Domain(1);
	m_isplanar = surf->IsPlanar();
    } else {
	m_isplanar = false;
    }
    m_children[0] = m_children[1] = m_children[2] = m_children[3] = NULL;
}

Subsurface::Subsurface(const Subsurface &_ssurf)
{
    m_surf = _ssurf.m_surf->Duplicate();
    m_u = _ssurf.m_u;
    m_v = _ssurf.m_v;
    m_isplanar = _ssurf.m_isplanar;
    m_children[0] = m_children[1] = m_children[2] = m_children[3] = NULL;
    SetBBox(_ssurf.m_node);
}

Subsurface::~Subsurface()
{
    for (int i = 0; i < 4; i++) {
	if (m_children[i]) {
	    delete m_children[i];
	}
    }
    delete m_surf;
}

int
Subsurface::Split()
{
    if (m_children[0] && m_children[1] && m_children[2] && m_children[3]) {
	return 0;
    }

    for (int i = 0; i < 4; i++) {
	m_children[i] = new Subsurface();
    }
    ON_Surface *temp_surf1 = NULL, *temp_surf2 = NULL;
    ON_BOOL32 ret = true;
    ret = m_surf->Split(0, m_surf->Domain(0).Mid(), temp_surf1, temp_surf2);
    if (!ret) {
	delete temp_surf1;
	delete temp_surf2;
	return -1;
    }

    ret = temp_surf1->Split(1, m_surf->Domain(1).Mid(), m_children[0]->m_surf, m_children[1]->m_surf);
    delete temp_surf1;
    if (!ret) {
	delete temp_surf2;
	return -1;
    }
    m_children[0]->m_u = ON_Interval(m_u.Min(), m_u.Mid());
    m_children[0]->m_v = ON_Interval(m_v.Min(), m_v.Mid());
    m_children[0]->SetBBox(m_children[0]->m_surf->BoundingBox());
    m_children[0]->m_isplanar = m_children[0]->m_surf->IsPlanar();
    m_children[1]->m_u = ON_Interval(m_u.Min(), m_u.Mid());
    m_children[1]->m_v = ON_Interval(m_v.Mid(), m_v.Max());
    m_children[1]->SetBBox(m_children[1]->m_surf->BoundingBox());
    m_children[1]->m_isplanar = m_children[1]->m_surf->IsPlanar();

    ret = temp_surf2->Split(1, m_v.Mid(), m_children[2]->m_surf, m_children[3]->m_surf);
    delete temp_surf2;
    if (!ret) {
	return -1;
    }
    m_children[2]->m_u = ON_Interval(m_u.Mid(), m_u.Max());
    m_children[2]->m_v = ON_Interval(m_v.Min(), m_v.Mid());
    m_children[2]->SetBBox(m_children[2]->m_surf->BoundingBox());
    m_children[2]->m_isplanar = m_children[2]->m_surf->IsPlanar();
    m_children[3]->m_u = ON_Interval(m_u.Mid(), m_u.Max());
    m_children[3]->m_v = ON_Interval(m_v.Mid(), m_v.Max());
    m_children[3]->SetBBox(m_children[3]->m_surf->BoundingBox());
    m_children[3]->m_isplanar = m_children[3]->m_surf->IsPlanar();

    return 0;
}

void
Subsurface::GetBBox(ON_3dPoint &min, ON_3dPoint &max)
{
    min = m_node.m_min;
    max = m_node.m_max;
}

void
Subsurface::SetBBox(const ON_BoundingBox &bbox)
{
    m_node = bbox;
    /* Make sure that each dimension of the bounding box is greater than
     * ON_ZERO_TOLERANCE.
     * It does the same work as building the surface tree when ray tracing
     */
    for (int i = 0; i < 3; i++) {
	double d = m_node.m_max[i] - m_node.m_min[i];
	if (ON_NearZero(d, ON_ZERO_TOLERANCE)) {
	    m_node.m_min[i] -= 0.001;
	    m_node.m_max[i] += 0.001;
	}
    }
}

bool
Subsurface::IsPointIn(const ON_3dPoint &pt, double tolerance /* = 0.0 */)
{
    ON_3dVector vtol(tolerance, tolerance, tolerance);
    ON_BoundingBox new_bbox(m_node.m_min - vtol, m_node.m_max + vtol);
    return new_bbox.IsPointIn(pt);
}

bool
Subsurface::Intersect(
	const Subcurve &curve,
	double tolerance /* = 0.0 */,
	ON_BoundingBox *intersection /* = NULL */) const
{
    ON_3dVector vtol(tolerance, tolerance, tolerance);
    ON_BoundingBox new_bbox(m_node.m_min - vtol, m_node.m_max + vtol);
    ON_BoundingBox box;
    bool ret = box.Intersection(new_bbox, curve.m_node);
    if (intersection != NULL) {
	*intersection = box;
    }
    return ret;
}

bool
Subsurface::Intersect(
	const Subsurface &surf,
	double tolerance /* = 0.0 */,
	ON_BoundingBox *intersection /* = NULL */) const
{
    ON_3dVector vtol(tolerance, tolerance, tolerance);
    ON_BoundingBox new_bbox(m_node.m_min - vtol, m_node.m_max + vtol);
    ON_BoundingBox box;
    bool ret = box.Intersection(new_bbox, surf.m_node);
    if (intersection != NULL) {
	*intersection = ON_BoundingBox(box.m_min - vtol, box.m_max + vtol);
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
