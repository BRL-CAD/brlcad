#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "shape_recognition.h"


int
subbrep_is_planar(struct subbrep_object_data *data)
{
    int i = 0;
    // Check surfaces.  If a surface is anything other than a plane the verdict is no.
    // If all surfaces are planes, then the verdict is yes.
    for (i = 0; i < data->faces_cnt; i++) {
	if (GetSurfaceType(data->brep->m_F[data->faces[i]].SurfaceOf(), NULL) != SURFACE_PLANE) return 0;
    }
    data->type = PLANAR_VOLUME;
    return 1;
}

// TODO - implement tests to recognize arb4-arb8 and valid arbn (no concave face) primitives


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
