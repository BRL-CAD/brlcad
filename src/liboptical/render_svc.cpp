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

};  // namespace OSL

#ifdef OSL_NAMESPACE
}; // end namespace OSL_NAMESPACE
#endif
