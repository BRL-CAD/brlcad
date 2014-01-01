/*                  R E N D E R _ S V C . C P P
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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

#include "render_svc.h"

using namespace OSL;

#ifdef OSL_NAMESPACE
namespace OSL_NAMESPACE {
#endif

namespace OSL {


bool
SimpleRenderer::get_matrix (Matrix44 &result, TransformationPtr xform,
			    float time)
{
    // SimpleRenderer doesn't understand motion blur and transformations
    // are just simple 4x4 matrices.
    result = *(OSL::Matrix44 *)xform;
    return true;
}


bool
SimpleRenderer::get_matrix (Matrix44 &result, ustring from, float time)
{
    TransformMap::const_iterator found = m_named_xforms.find (from);
    if (found != m_named_xforms.end()) {
	result = *(found->second);
	return true;
    } else {
	return false;
    }
}

bool
SimpleRenderer::get_matrix (Matrix44 &result, TransformationPtr xform)
{
    // SimpleRenderer doesn't understand motion blur and transformations
    // are just simple 4x4 matrices.
    result = *(OSL::Matrix44 *)xform;
    return true;
}


bool
SimpleRenderer::get_matrix (Matrix44 &result, ustring from)
{
    // SimpleRenderer doesn't understand motion blur, so we never fail
    // on account of time-varying transformations.
    TransformMap::const_iterator found = m_named_xforms.find (from);
    if (found != m_named_xforms.end()) {
	result = *(found->second);
	return true;
    } else {
	return false;
    }
}


void
SimpleRenderer::name_transform (const char *name, const OSL::Matrix44 &xform)
{
    shared_ptr<Transformation> M (new OSL::Matrix44 (xform));
    m_named_xforms[ustring(name)] = M;
}

bool
SimpleRenderer::get_array_attribute (void *renderstate, bool derivatives, ustring object,
				     TypeDesc type, ustring name,
				     int index, void *val)
{
    return false;
}

bool
SimpleRenderer::get_attribute (void *renderstate, bool derivatives, ustring object,
			       TypeDesc type, ustring name, void *val)
{
    return false;
}

bool
SimpleRenderer::get_userdata (bool derivatives, ustring name, TypeDesc type, void *renderstate, void *val)
{
    return false;
}

bool
SimpleRenderer::has_userdata (ustring name, TypeDesc type, void *renderstate)
{
    return false;
}

void *
SimpleRenderer::get_pointcloud_attr_query (ustring *attr_names,
					   TypeDesc *attr_types, int nattrs)
{
    return NULL;
}

int
SimpleRenderer::pointcloud (ustring filename, const OSL::Vec3 &center, float radius,
			    int max_points, void *_attr_query, void **attr_outdata)
{
    return 0;
}

int
SimpleRenderer::pointcloud_search (ustring filename, const OSL::Vec3 &center,
				   float radius, int max_points, size_t *out_indices,
				   float *out_distances, int derivs_offset)
{
    return 0;
}

int
SimpleRenderer::pointcloud_get (ustring filename, size_t *indices, int count,
				ustring attr_name, TypeDesc attr_type,
				void *out_data)
{
    return 0;
}

};  // namespace OSL

#ifdef OSL_NAMESPACE
}; // end namespace OSL_NAMESPACE
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
