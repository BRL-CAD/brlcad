/*                    S U B C U R V E . C P P
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

Subcurve::Subcurve()
    : m_curve(NULL), m_islinear(false)
{
    m_children[0] = m_children[1] = NULL;
}

Subcurve::Subcurve(ON_Curve *curve)
{
    m_curve = curve;
    if (curve) {
	m_node = curve->BoundingBox();
	m_t = curve->Domain();
	m_islinear = curve->IsLinear();
    } else {
	m_islinear = false;
    }
    m_children[0] = m_children[1] = NULL;
}

Subcurve::Subcurve(const Subcurve &_scurve)
{
    m_islinear = _scurve.m_islinear;
    m_curve = _scurve.m_curve->Duplicate();
    m_t = _scurve.m_t;
    m_children[0] = m_children[1] = NULL;
    SetBBox(_scurve.m_node);
}

Subcurve::~Subcurve()
{
    for (int i = 0; i < 2; i++) {
	if (m_children[i]) {
	    delete m_children[i];
	}
    }
    delete m_curve;
}

int
Subcurve::Split()
{
    if (m_children[0] && m_children[1]) {
	return 0;
    }

    for (int i = 0; i < 2; i++) {
	m_children[i] = new Subcurve();
    }
    ON_SimpleArray<double> pline_t;
    double split_t = m_t.Mid();
    if (m_curve->IsPolyline(NULL, &pline_t) && pline_t.Count() > 2) {
	split_t = pline_t[pline_t.Count() / 2];
    }
    if (m_curve->Split(split_t, m_children[0]->m_curve, m_children[1]->m_curve) == false) {
	return -1;
    }

    m_children[0]->m_t = m_children[0]->m_curve->Domain();
    m_children[0]->SetBBox(m_children[0]->m_curve->BoundingBox());
    m_children[0]->m_islinear = m_children[0]->m_curve->IsLinear();
    m_children[1]->m_t = m_children[1]->m_curve->Domain();
    m_children[1]->SetBBox(m_children[1]->m_curve->BoundingBox());
    m_children[1]->m_islinear = m_children[1]->m_curve->IsLinear();

    return 0;
}

void
Subcurve::GetBBox(ON_3dPoint &min, ON_3dPoint &max)
{
    min = m_node.m_min;
    max = m_node.m_max;
}

void
Subcurve::SetBBox(const ON_BoundingBox &bbox)
{
    m_node = bbox;
}

bool
Subcurve::IsPointIn(const ON_3dPoint &pt, double tolerance /* = 0.0 */)
{
    ON_3dVector vtol(tolerance, tolerance, tolerance);
    ON_BoundingBox new_bbox(m_node.m_min - vtol, m_node.m_max + vtol);
    return new_bbox.IsPointIn(pt);
}

bool
Subcurve::Intersect(const Subcurve &other,
	            double tolerance /* = 0.0 */,
		    ON_BoundingBox *intersection /* = NULL */) const
{
    ON_3dVector vtol(tolerance, tolerance, tolerance);
    ON_BoundingBox new_bbox(m_node.m_min - vtol, m_node.m_max + vtol);
    ON_BoundingBox box;
    bool ret = box.Intersection(new_bbox, other.m_node);
    if (intersection != NULL) {
	*intersection = box;
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
