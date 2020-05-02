#include "common.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "vmath.h"
#include "bu/avs.h"
#include "bu/path.h"
#include "brep.h"
#include "wdb.h"
#include "analyze.h"
#include "ged.h"
#include "./ged_bot.h"


bool
bot_face_normal(vect_t *n, struct rt_bot_internal *bot, int i)
{
    vect_t a,b;

    /* sanity */
    if (!n || !bot || i < 0 || (size_t)i > bot->num_faces ||
	    bot->faces[i*3+2] < 0 || (size_t)bot->faces[i*3+2] > bot->num_vertices) {
	return false;
    }

     VSUB2(a, &bot->vertices[bot->faces[i*3+1]*3], &bot->vertices[bot->faces[i*3]*3]);
     VSUB2(b, &bot->vertices[bot->faces[i*3+2]*3], &bot->vertices[bot->faces[i*3]*3]);
     VCROSS(*n, a, b);
     VUNITIZE(*n);
     if (bot->orientation == RT_BOT_CW) {
	 VREVERSE(*n, *n);
     }

     return true;
}



extern "C" int
_bot_cmd_arb6(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] <objname> arb6 <output_obj>";
    const char *purpose_string = "generate an ARB6 representation of the specified plate mode BoT object";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    if (gb->intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
        bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type bot\n", gb->solid_name.c_str());
        return GED_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern.idb_ptr);
    if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS) {
        bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not a plate mode bot\n", gb->solid_name.c_str());
	return GED_ERROR;
    }

    // Check for at least 1 non-zero thickness, or there's no volume to define
    bool have_solid = false;
    for (size_t i = 0; i < bot->num_faces; i++) {
	if (bot->thickness[i] > VUNITIZE_TOL) {
	    have_solid = true;
	}
    }
    if (!have_solid) {
        bu_vls_printf(gb->gedp->ged_result_str, "bot %s does not have any non-degenerate face thicknesses\n", gb->solid_name.c_str());
	return GED_OK;
    }

    // Average the face normals at each vertex to get an average direction in
    // which to move each vertex for arb6 generation.
    vect_t *f2n = (vect_t *)bu_calloc(bot->num_faces, sizeof(vect_t), "face normals");
    vect_t *v2n = (vect_t *)bu_calloc(bot->num_vertices, sizeof(vect_t), "vert normals");
    int *v2fc = (int *)bu_calloc(bot->num_vertices, sizeof(int), "cnts");
    for (size_t i = 0; i < bot->num_faces; i++) {
	vect_t n;
	bot_face_normal(&n, bot, i);
	VMOVE(f2n[i],n);
    }
    // Add up all the face normal vectors at each vertex
    for (size_t i = 0; i < bot->num_faces; i++) {
	for (int j = 0; j < 3; j++) {
	    VADD2(v2n[bot->faces[i*3+j]], v2n[bot->faces[i*3+j]], f2n[i]);
	    v2fc[bot->faces[i*3+j]] = v2fc[bot->faces[i*3+j]] + 1;
	}
    }
    // Average based on how many faces contributed
    for (size_t i = 0; i < bot->num_vertices; i++) {
	VSCALE(v2n[i], v2n[i], (1.0/(double)v2fc[i]));
    }
    // Unitize
    for (size_t i = 0; i < bot->num_vertices; i++) {
	VUNITIZE(v2n[i]);
    }

    // Make a comb to hold the union of the arb6 primitives
    struct wmember wcomb;
    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&comb_name, "%s_arb6.c", gb->dp->d_namep);
    // TODO - db_lookup to make sure it doesn't already exist
    BU_LIST_INIT(&wcomb.l);

    // For each face, define an arb6 using shifted vertices.  For each face
    // vertex two new points will be constructed - an inner and outer - based
    // on the original point, the local face thickness, and the avg face dir
    // and its inverse.  Check the face_mode flag to know which points to shift
    // in which direction.
    struct bu_vls prim_name = BU_VLS_INIT_ZERO;
    for (size_t i = 0; i < bot->num_faces; i++) {
	fastf_t pnts[3*6];
	point_t pf[3];
	vect_t pv1[3], pv2[3];
	for (int j = 0; j < 3; j++) {
	    VMOVE(pf[j], &bot->vertices[bot->faces[i*3+j]*3]);
	    VSCALE(pv1[j], v2n[bot->faces[i*3+j]], bot->thickness[i]);
	    VSCALE(pv2[j], v2n[bot->faces[i*3+j]], -1*bot->thickness[i]);
	}
	for (int j = 0; j < 3; j++) {
	    point_t npnt;
	    VADD2(npnt, pf[j], pv1[j]);
	    for (int k = 0; k < 3; k++) {
		pnts[j*3+k] = npnt[k];
	    }
	}
	for (int j = 3; j < 6; j++) {
	    point_t npnt;
	    VADD2(npnt, pf[j], pv2[j]);
	    for (int k = 0; k < 3; k++) {
		pnts[j*3+k] = npnt[k];
	    }
	}
	bu_vls_sprintf(&prim_name, "%s.arb6.%zd", gb->dp->d_namep, i);
	mk_arb6(gb->gedp->ged_wdbp, bu_vls_cstr(&prim_name), pnts);
	(void)mk_addmember(bu_vls_cstr(&prim_name), &(wcomb.l), NULL, DB_OP_UNION);
    }

    // Write the comb
    mk_lcomb(gb->gedp->ged_wdbp, bu_vls_addr(&comb_name), &wcomb, 0, NULL, NULL, NULL, 0);

    bu_vls_free(&comb_name);
    bu_vls_free(&prim_name);
    bu_free(f2n, "face normals");
    bu_free(v2n, "vert normals");
    bu_free(v2fc, "cnts");

    return GED_OK;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
