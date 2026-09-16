/*                 S K E T C H _ B R E P . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file sketch_brep.h
 *
 * Shared exact sketch-segment construction for sketch-derived BReps.
 */

#ifndef LIBRT_PRIMITIVES_SKETCH_SKETCH_BREP_H
#define LIBRT_PRIMITIVES_SKETCH_SKETCH_BREP_H

#include <vector>


enum rt_sketch_brep_curve_status {
    RT_SKETCH_BREP_CURVE_CREATED = 0,
    RT_SKETCH_BREP_CURVE_DEGENERATE,
    RT_SKETCH_BREP_CURVE_INVALID,
    RT_SKETCH_BREP_CURVE_UNSUPPORTED
};


static inline bool
rt_sketch_brep_order_loops(ON_Brep &brep, const struct bn_tol *tol,
	std::vector<ON_SimpleArray<ON_Curve *> > *ordered_loops,
	size_t *outer_loop_index)
{
    if (!ordered_loops || !outer_loop_index)
	return false;
    ordered_loops->clear();
    const double tolerance = (tol && tol->dist > 0.0) ?
	tol->dist : RT_LEN_TOL;
    const int curve_count = brep.m_C3.Count();
    if (curve_count < 1)
	return false;

    std::vector<bool> used(curve_count, false);
    int used_count = 0;
    while (used_count < curve_count) {
	int first_index = -1;
	for (int i = 0; i < curve_count; ++i) {
	    if (!used[i]) {
		first_index = i;
		break;
	    }
	}
	if (first_index < 0)
	    return false;

	ordered_loops->push_back(ON_SimpleArray<ON_Curve *>());
	ON_SimpleArray<ON_Curve *> &loop = ordered_loops->back();
	ON_Curve *first = brep.m_C3[first_index];
	loop.Append(first);
	used[first_index] = true;
	++used_count;
	const ON_3dPoint start = first->PointAtStart();
	ON_3dPoint current = first->PointAtEnd();
	while (current.DistanceTo(start) > tolerance) {
	    int next_index = -1;
	    bool reverse = false;
	    for (int i = 0; i < curve_count; ++i) {
		if (used[i])
		    continue;
		ON_Curve *candidate = brep.m_C3[i];
		if (current.DistanceTo(candidate->PointAtStart()) <= tolerance) {
		    next_index = i;
		    break;
		}
		if (current.DistanceTo(candidate->PointAtEnd()) <= tolerance) {
		    next_index = i;
		    reverse = true;
		    break;
		}
	    }
	    if (next_index < 0)
		return false;
	    ON_Curve *next = brep.m_C3[next_index];
	    if (reverse && !next->Reverse())
		return false;
	    loop.Append(next);
	    used[next_index] = true;
	    ++used_count;
	    current = next->PointAtEnd();
	}
    }

    *outer_loop_index = 0;
    double largest_diagonal = -1.0;
    for (size_t i = 0; i < ordered_loops->size(); ++i) {
	ON_BoundingBox bbox;
	for (int j = 0; j < (*ordered_loops)[i].Count(); ++j)
	    (*ordered_loops)[i][j]->GetBoundingBox(bbox, true);
	const double diagonal = bbox.Diagonal().Length();
	if (diagonal > largest_diagonal) {
	    largest_diagonal = diagonal;
	    *outer_loop_index = i;
	}
    }
    return true;
}


static inline bool
rt_sketch_brep_embedding(ON_Xform *embedding, const ON_3dPoint &origin,
	const ON_3dVector &u_vector, const ON_3dVector &v_vector)
{
    if (!embedding)
	return false;
    ON_3dVector normal = ON_CrossProduct(u_vector, v_vector);
    if (!normal.Unitize())
	return false;
    *embedding = ON_Xform(1.0);
    (*embedding)[0][0] = u_vector.x;
    (*embedding)[0][1] = v_vector.x;
    (*embedding)[0][2] = normal.x;
    (*embedding)[0][3] = origin.x;
    (*embedding)[1][0] = u_vector.y;
    (*embedding)[1][1] = v_vector.y;
    (*embedding)[1][2] = normal.y;
    (*embedding)[1][3] = origin.y;
    (*embedding)[2][0] = u_vector.z;
    (*embedding)[2][1] = v_vector.z;
    (*embedding)[2][2] = normal.z;
    (*embedding)[2][3] = origin.z;
    return true;
}


static inline ON_Curve *
rt_sketch_brep_curve(const struct rt_sketch_internal *sketch, size_t index,
	const ON_Xform &embedding, const struct bn_tol *tol,
	enum rt_sketch_brep_curve_status *status)
{
    if (status)
	*status = RT_SKETCH_BREP_CURVE_INVALID;
    if (!sketch || index >= sketch->curve.count ||
	!sketch->curve.segment[index])
	return NULL;

    const double tolerance = (tol && tol->dist > 0.0) ?
	tol->dist : RT_LEN_TOL;
    const uint32_t *magic = (const uint32_t *)sketch->curve.segment[index];
    ON_Curve *curve = NULL;

    switch (*magic) {
	case CURVE_LSEG_MAGIC: {
	    const struct line_seg *segment = (const struct line_seg *)magic;
	    if (segment->start < 0 || segment->end < 0 ||
		(size_t)segment->start >= sketch->vert_count ||
		(size_t)segment->end >= sketch->vert_count)
		return NULL;
	    const ON_3dPoint start(sketch->verts[segment->start][0],
		sketch->verts[segment->start][1], 0.0);
	    const ON_3dPoint end(sketch->verts[segment->end][0],
		sketch->verts[segment->end][1], 0.0);
	    if (start.DistanceTo(end) <= ON_ZERO_TOLERANCE) {
		if (status)
		    *status = RT_SKETCH_BREP_CURVE_DEGENERATE;
		return NULL;
	    }
	    curve = new ON_LineCurve(start, end);
	    break;
	}
	case CURVE_CARC_MAGIC: {
	    const struct carc_seg *segment = (const struct carc_seg *)magic;
	    if (segment->start < 0 || segment->end < 0 ||
		(size_t)segment->start >= sketch->vert_count ||
		(size_t)segment->end >= sketch->vert_count)
		return NULL;
	    const ON_2dPoint start_2d(sketch->verts[segment->start]);
	    const ON_2dPoint end_2d(sketch->verts[segment->end]);
	    const ON_3dPoint start(start_2d.x, start_2d.y, 0.0);
	    if (segment->radius < 0.0) {
		const ON_3dPoint center(end_2d.x, end_2d.y, 0.0);
		ON_3dVector xaxis = start - center;
		const double radius = xaxis.Length();
		if (radius <= ON_ZERO_TOLERANCE || !xaxis.Unitize()) {
		    if (status)
			*status = RT_SKETCH_BREP_CURVE_DEGENERATE;
		    return NULL;
		}
		ON_3dVector yaxis = ON_CrossProduct(ON_zaxis, xaxis);
		if (!yaxis.Unitize())
		    return NULL;
		const ON_ArcCurve circle_curve(ON_Circle(
		    ON_Plane(center, xaxis, yaxis), radius));
		ON_NurbsCurve *nurbs = ON_NurbsCurve::New();
		if (circle_curve.GetNurbForm(*nurbs) <= 0) {
		    delete nurbs;
		    return NULL;
		}
		curve = nurbs;
		break;
	    }

	    const double radius = segment->radius;
	    const ON_2dPoint midpoint = (start_2d + end_2d) * 0.5;
	    const ON_2dVector start_to_mid = midpoint - start_2d;
	    const double half_chord_sq = start_to_mid.LengthSquared();
	    if (radius <= ON_ZERO_TOLERANCE ||
		half_chord_sq <= ON_ZERO_TOLERANCE * ON_ZERO_TOLERANCE ||
		radius * radius < half_chord_sq)
		return NULL;
	    ON_2dVector center_direction(-start_to_mid.y, start_to_mid.x);
	    if (!center_direction.Unitize())
		return NULL;
	    const double offset = sqrt(radius * radius - half_chord_sq);
	    ON_2dPoint center_2d = midpoint + offset * center_direction;
	    const double cross = (end_2d.x - start_2d.x) *
		(center_2d.y - start_2d.y) -
		(end_2d.y - start_2d.y) * (center_2d.x - start_2d.x);
	    if (!(cross > 0.0 && segment->center_is_left))
		center_2d = midpoint - offset * center_direction;
	    double start_angle = atan2(start_2d.y - center_2d.y,
		start_2d.x - center_2d.x);
	    double end_angle = atan2(end_2d.y - center_2d.y,
		end_2d.x - center_2d.x);
	    if (segment->orientation) {
		while (end_angle > start_angle)
		    end_angle -= 2.0 * ON_PI;
	    } else {
		while (end_angle < start_angle)
		    end_angle += 2.0 * ON_PI;
	    }
	    const double middle_angle = 0.5 * (start_angle + end_angle);
	    const ON_3dPoint middle(
		center_2d.x + radius * cos(middle_angle),
		center_2d.y + radius * sin(middle_angle), 0.0);
	    const ON_3dPoint end(end_2d.x, end_2d.y, 0.0);
	    const ON_Arc arc(start, middle, end);
	    if (!arc.IsValid())
		return NULL;
	    const ON_ArcCurve arc_curve(arc);
	    ON_NurbsCurve *nurbs = ON_NurbsCurve::New();
	    if (arc_curve.GetNurbForm(*nurbs) <= 0) {
		delete nurbs;
		return NULL;
	    }
	    curve = nurbs;
	    break;
	}
	case CURVE_BEZIER_MAGIC: {
	    const struct bezier_seg *segment = (const struct bezier_seg *)magic;
	    if (segment->degree < 1 || !segment->ctl_points)
		return NULL;
	    ON_3dPointArray points(segment->degree + 1);
	    for (int i = 0; i <= segment->degree; ++i) {
		const int vertex = segment->ctl_points[i];
		if (vertex < 0 || (size_t)vertex >= sketch->vert_count)
		    return NULL;
		points.Append(ON_3dPoint(sketch->verts[vertex][0],
		    sketch->verts[vertex][1], 0.0));
	    }
	    const ON_BezierCurve bezier(points);
	    ON_NurbsCurve *nurbs = ON_NurbsCurve::New();
	    if (bezier.GetNurbForm(*nurbs) <= 0) {
		delete nurbs;
		return NULL;
	    }
	    curve = nurbs;
	    break;
	}
	case CURVE_NURB_MAGIC: {
	    const struct nurb_seg *segment = (const struct nurb_seg *)magic;
	    const bool rational = RT_NURB_IS_PT_RATIONAL(segment->pt_type);
	    if (segment->order < 2 || segment->c_size < segment->order ||
		!segment->ctl_points || !segment->k.knots ||
		segment->k.k_size != segment->c_size + segment->order ||
		(rational && !segment->weights))
		return NULL;
	    ON_NurbsCurve *nurbs = ON_NurbsCurve::New(3, rational,
		segment->order, segment->c_size);
	    for (int i = 1; i < segment->k.k_size - 1; ++i)
		nurbs->SetKnot(i - 1, segment->k.knots[i]);
	    for (int i = 0; i < segment->c_size; ++i) {
		const int vertex = segment->ctl_points[i];
		if (vertex < 0 || (size_t)vertex >= sketch->vert_count) {
		    delete nurbs;
		    return NULL;
		}
		if (rational) {
		    const double weight = segment->weights[i];
		    if (fabs(weight) <= ON_ZERO_TOLERANCE) {
			delete nurbs;
			return NULL;
		    }
		    const double x = sketch->verts[vertex][0];
		    const double y = sketch->verts[vertex][1];
		    nurbs->SetCV(i, ON_4dPoint(x, y, 0.0, weight));
		} else {
		    nurbs->SetCV(i, ON_3dPoint(sketch->verts[vertex][0],
			sketch->verts[vertex][1], 0.0));
		}
	    }
	    if (!nurbs->IsValid()) {
		delete nurbs;
		return NULL;
	    }
	    curve = nurbs;
	    break;
	}
	default:
	    if (status)
		*status = RT_SKETCH_BREP_CURVE_UNSUPPORTED;
	    return NULL;
    }

    if (!curve || !curve->IsValid() || !curve->Transform(embedding) ||
	!curve->IsValid()) {
	delete curve;
	return NULL;
    }
    ON_BoundingBox bbox;
    if (!curve->GetBoundingBox(bbox, false) ||
	bbox.Diagonal().Length() <= tolerance) {
	delete curve;
	if (status)
	    *status = RT_SKETCH_BREP_CURVE_DEGENERATE;
	return NULL;
    }
    if (sketch->curve.reverse && sketch->curve.reverse[index] &&
	!curve->Reverse()) {
	delete curve;
	return NULL;
    }
    if (status)
	*status = RT_SKETCH_BREP_CURVE_CREATED;
    return curve;
}


#endif /* LIBRT_PRIMITIVES_SKETCH_SKETCH_BREP_H */
