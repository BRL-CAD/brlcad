#include "common.h"

#include <map>
#include <set>
#include <queue>
#include <list>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "plot3.h"
#include "opennurbs.h"
#include "brep.h"
#include "../libbrep/shape_recognition.h"


struct model *
brep_to_nmg(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct wmember *wcomb)
{
    struct bu_vls prim_name = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&prim_name, "nmg_%s.s", bu_vls_addr(data->key));
    std::set<int> b_verts;
    std::vector<int> b_verts_array;
    std::map<int, int> b_verts_to_verts;
    struct model *m = nmg_mm();
    struct nmgregion *r = nmg_mrsv(m);
    struct shell *s = BU_LIST_FIRST(shell, &(r)->s_hd);
    struct faceuse **fu;         /* array of faceuses */
    struct vertex **verts;       /* Array of pointers to vertex structs */
    struct vertex ***loop_verts; /* Array of pointers to vertex structs to pass to nmg_cmface */

    struct bn_tol nmg_tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };

    int point_cnt = 0;
    int face_cnt = 0;
    int max_edge_cnt = 0;

    // One loop to a face, and the object data has the set of loops that make
    // up this object.
    for (int s_it = 0; s_it < data->loops_cnt; s_it++) {
	const ON_BrepLoop *b_loop = &(data->brep->m_L[data->loops[s_it]]);
	const ON_BrepFace *b_face = b_loop->Face();
	face_cnt++;
	if (b_loop->m_ti.Count() > max_edge_cnt) max_edge_cnt = b_loop->m_ti.Count();
	for (int ti = 0; ti < b_loop->m_ti.Count(); ti++) {
	    const ON_BrepTrim *trim = &(b_face->Brep()->m_T[b_loop->m_ti[ti]]);
	    const ON_BrepEdge *edge = &(b_face->Brep()->m_E[trim->m_ei]);
	    if (b_verts.find(edge->Vertex(0)->m_vertex_index) == b_verts.end()) {
		b_verts.insert(edge->Vertex(0)->m_vertex_index);
		b_verts_array.push_back(edge->Vertex(0)->m_vertex_index);
		b_verts_to_verts[edge->Vertex(0)->m_vertex_index] = b_verts_array.size()-1;
	    }
	    if (b_verts.find(edge->Vertex(1)->m_vertex_index) == b_verts.end()) {
		b_verts.insert(edge->Vertex(1)->m_vertex_index);
		b_verts_array.push_back(edge->Vertex(1)->m_vertex_index);
		b_verts_to_verts[edge->Vertex(1)->m_vertex_index] = b_verts_array.size()-1;
	    }
	}
    }

    point_cnt = b_verts.size();

    verts = (struct vertex **)bu_calloc(point_cnt, sizeof(struct vertex *), "brep_to_nmg: verts");
    loop_verts = (struct vertex ***) bu_calloc(max_edge_cnt, sizeof(struct vertex **), "brep_to_nmg: loop_verts");
    fu = (struct faceuse **) bu_calloc(face_cnt, sizeof(struct faceuse *), "brep_to_nmg: fu");

    // Make the faces
    int face_count = 0;
    for (int s_it = 0; s_it < data->loops_cnt; s_it++) {
	int loop_length = 0;
	const ON_BrepLoop *b_loop = &(data->brep->m_L[data->loops[s_it]]);
	const ON_BrepFace *b_face = b_loop->Face();
	for (int ti = 0; ti < b_loop->m_ti.Count(); ti++) {
	    const ON_BrepTrim *trim = &(b_face->Brep()->m_T[b_loop->m_ti[ti]]);
	    const ON_BrepEdge *edge = &(b_face->Brep()->m_E[trim->m_ei]);
	    if (trim->m_bRev3d) {
		loop_verts[loop_length] = &(verts[b_verts_to_verts[edge->Vertex(0)->m_vertex_index]]);
	    } else {
		loop_verts[loop_length] = &(verts[b_verts_to_verts[edge->Vertex(1)->m_vertex_index]]);
	    }
	    loop_length++;
	}
	fu[face_count] = nmg_cmface(s, loop_verts, loop_length);
	face_count++;
    }

    for (int p = 0; p < point_cnt; p++) {
	ON_3dPoint pt = data->brep->m_V[b_verts_array[p]].Point();
	point_t nmg_pt;
	nmg_pt[0] = pt.x;
	nmg_pt[1] = pt.y;
	nmg_pt[2] = pt.z;
	nmg_vertex_gv(verts[p], pt);
    }

    for (int f = 0; f < face_cnt; f++) {
	nmg_fu_planeeqn(fu[f], &nmg_tol);
    }

    nmg_fix_normals(s, &nmg_tol);
    (void)nmg_mark_edges_real(&s->l.magic);
    /* Compute "geometry" for region and shell */
    nmg_region_a(r, &nmg_tol);

    /* Create the nmg primitive */
    mk_nmg(wdbp, bu_vls_addr(&prim_name), m);
    if (wcomb) (void)mk_addmember(bu_vls_addr(&prim_name), &((*wcomb).l), NULL, WMOP_UNION);

    bu_vls_free(&prim_name);

    return m;
}

int
subbrep_to_csg_arb8(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct wmember *wcomb)
{
    struct csg_object_params *params = data->params;
    if (data->type == ARB8) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&prim_name, "arb8_%s.s", bu_vls_addr(data->key));

	mk_arb8(wdbp, bu_vls_addr(&prim_name), (const fastf_t *)params->p);
	std::cout << bu_vls_addr(&prim_name) << ": " << params->bool_op << "\n";
	if (wcomb) (void)mk_addmember(bu_vls_addr(&prim_name), &((*wcomb).l), NULL, db_str2op(&(params->bool_op)));
	bu_vls_free(&prim_name);
	return 1;
    } else {
	return 0;
    }
}
int
subbrep_to_csg_planar(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct wmember *wcomb)
{
    if (data->type == PLANAR_VOLUME) {
	// BRL-CAD's arbn primitive does not support concave shapes, and we want to use
	// simpler primitives than the generic nmg if the opportunity arises.  A heuristic
	// along the following lines may help:
	//
	// Step 1.  Check the number of vertices.  If the number is small enough that the
	//          volume might conceivably be expressed by an arb4-arb8, try that first.
	//
	// Step 2.  If the arb4-arb8 test fails, do the convex hull and see if all points
	//          are used by the hull.  If so, the shape is convex and may be expressed
	//          as an arbn.
	//
	// Step 3.  If the arbn test fails, construct sets of contiguous concave faces using
	//          the set of edges with one or more vertices not in the convex hull.  If
	//          those shapes are all convex, construct an arbn tree (or use simpler arb
	//          shapes if the subtractions may be so expressed).
	//
	// Step 4.  If the subtraction volumes in Step 3 are still not convex, cut our losses
	//          and proceed to the nmg primitive.  It may conceivably be worth some
	//          additional searches to spot convex subsets of shapes that can be more
	//          simply represented, but that is not particularly simple to do well
	//          and should wait until it is clear we would get a benefit from it.  Look
	//          at the arbn tessellation routine for a guide on how to set up the
	//          nmg - that's the most general of the arb* primitives and should be
	//          relatively close to what is needed here.
	(void)brep_to_nmg(data, wdbp, wcomb);
	return 1;
    } else {
	return 0;
    }
}

int
subbrep_to_csg_cylinder(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct wmember *wcomb)
{
    struct csg_object_params *params = data->params;
    if (data->type == CYLINDER) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&prim_name, "rcc_%s.s", bu_vls_addr(data->key));

	mk_rcc(wdbp, bu_vls_addr(&prim_name), params->origin, params->hv, params->radius);
	std::cout << bu_vls_addr(&prim_name) << ": " << params->bool_op << "\n";
	if (wcomb) (void)mk_addmember(bu_vls_addr(&prim_name), &((*wcomb).l), NULL, db_str2op(&(params->bool_op)));
	bu_vls_free(&prim_name);
	return 1;
    }
    return 0;
}

int
subbrep_to_csg_conic(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct wmember *wcomb)
{
    struct csg_object_params *params = data->params;
    if (data->type == CONE) {
	struct bu_vls prim_name = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&prim_name, "trc_%s.s", bu_vls_addr(data->key));

	mk_cone(wdbp, bu_vls_addr(&prim_name), params->origin, params->hv, params->height, params->radius, params->r2);
	std::cout << bu_vls_addr(&prim_name) << ": " << params->bool_op << "\n";
	if (wcomb) (void)mk_addmember(bu_vls_addr(&prim_name), &((*wcomb).l), NULL, db_str2op(&(params->bool_op)));
	bu_vls_free(&prim_name);
	return 1;
    }
    return 0;
}


void
process_params(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct wmember *wcomb)
{
    switch (data->type) {
	case ARB8:
	    subbrep_to_csg_arb8(data, wdbp, wcomb);
	    break;
	case PLANAR_VOLUME:
	    subbrep_to_csg_planar(data, wdbp, wcomb);
	    break;
	case CYLINDER:
	    subbrep_to_csg_cylinder(data, wdbp, wcomb);
	    break;
	case CONE:
	    subbrep_to_csg_conic(data, wdbp, wcomb);
	    break;
	case SPHERE:
	    break;
	case ELLIPSOID:
	    break;
	case TORUS:
	    break;
	default:
	    break;
    }
}


int
make_shapes(struct subbrep_object_data *data, struct rt_wdb *wdbp, struct wmember *pcomb)
{
    std::cout << "Making shape for " << bu_vls_addr(data->key) << "\n";
    if (data->type == BREP) {
	if (data->local_brep) {
	    struct bu_vls brep_name = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&brep_name, "brep_%s.s", bu_vls_addr(data->key));
	    mk_brep(wdbp, bu_vls_addr(&brep_name), data->local_brep);
	    // TODO - not always union
	    if (pcomb) (void)mk_addmember(bu_vls_addr(&brep_name), &(pcomb->l), NULL, WMOP_UNION);
	    bu_vls_free(&brep_name);
	} else {
	    bu_log("Warning - mk_brep called but data->local_brep is empty\n");
	}
    } else {
	if (data->type == COMB) {
	    struct wmember wcomb;
	    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
	    struct bu_vls member_name = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&comb_name, "comb_%s.c", bu_vls_addr(data->key));
	    BU_LIST_INIT(&wcomb.l);
	    bu_log("make comb %s\n", bu_vls_addr(data->key));
	    for (unsigned int i = 0; i < BU_PTBL_LEN(data->children); i++){
		struct subbrep_object_data *cdata = (struct subbrep_object_data *)BU_PTBL_GET(data->children,i);
		std::cout << "Making child shape(" << cdata->type << "):\n";
		make_shapes(cdata, wdbp, &wcomb);
		subbrep_object_free(cdata);
	    }
	    mk_lcomb(wdbp, bu_vls_addr(&comb_name), &wcomb, 0, NULL, NULL, NULL, 0);
	    // TODO - not always union
	    if (pcomb) (void)mk_addmember(bu_vls_addr(&comb_name), &(pcomb->l), NULL, WMOP_UNION);

	    bu_vls_free(&member_name);
	    bu_vls_free(&comb_name);
	} else {
	    std::cout << "type: " << data->type << "\n";
	    process_params(data, wdbp, pcomb);
	}
    }
    return 0;
}

int
main(int argc, char *argv[])
{
    struct db_i *dbip;
    struct rt_wdb *wdbp;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_brep_internal *brep_ip = NULL;

    if (argc != 3) {
	bu_exit(1, "Usage: %s file.g object", argv[0]);
    }

    dbip = db_open(argv[1], DB_OPEN_READWRITE);
    if (dbip == DBI_NULL) {
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
    }

    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);

    if (db_dirbuild(dbip) < 0)
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);

    dp = db_lookup(dbip, argv[2], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_exit(1, "ERROR: Unable to look up object %s\n", argv[2]);
    }

    RT_DB_INTERNAL_INIT(&intern)

    if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0) {
        bu_exit(1, "ERROR: Unable to get internal representation of %s\n", argv[2]);
    }

    if (intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BREP) {
	bu_exit(1, "ERROR: object %s does not appear to be of type BRep\n", argv[2]);
    } else {
	brep_ip = (struct rt_brep_internal *)intern.idb_ptr;
    }
    RT_BREP_CK_MAGIC(brep_ip);

    ON_Brep *brep = brep_ip->brep;
    struct wmember pcomb;
    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&comb_name, "%s_csg.c", dp->d_namep);
    BU_LIST_INIT(&pcomb.l);

    struct bu_ptbl *subbreps = find_subbreps(brep);
    for (unsigned int i = 0; i < BU_PTBL_LEN(subbreps); i++){
	struct subbrep_object_data *obj = (struct subbrep_object_data *)BU_PTBL_GET(subbreps, i);
	//print_subbrep_object(obj, "");
	// first, make the comb (or, if we have a brep, make that)
	(void)make_shapes(obj, wdbp, &pcomb);
	subbrep_object_free(obj);
	BU_PUT(obj, struct subbrep_object_data);
    }
    bu_ptbl_free(subbreps);
    BU_PUT(subbreps, struct bu_ptbl);
    // TODO - probably should be region
    mk_lcomb(wdbp, bu_vls_addr(&comb_name), &pcomb, 0, NULL, NULL, NULL, 0);

    db_close(dbip);

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
