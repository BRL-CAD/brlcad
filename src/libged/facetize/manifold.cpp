/*                     M A N I F O L D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
/** @file libged/facetize/manifold.cpp
 *
 * The facetize command.
 *
 */

#include "common.h"

#include <map>
#include <set>
#include <vector>

#include <string.h>

#include "manifold/manifold.h"


#include "bu/app.h"
#include "../ged_private.h"
#include "./ged_facetize.h"

struct manifold_mesh {
    int num_vertices;
    int num_faces;
    fastf_t *vertices;
    unsigned int *faces;
};

static
struct manifold_mesh *
bot_to_mmesh(struct rt_bot_internal *bot)
{
    struct manifold_mesh *omesh = NULL;
    BU_GET(omesh, struct manifold_mesh);
    omesh->num_vertices = bot->num_vertices;
    omesh->num_faces = bot->num_faces;
    omesh->faces = (unsigned int *)bu_calloc(bot->num_faces * 3, sizeof(unsigned int), "faces array");
    for (size_t i = 0; i < bot->num_faces * 3; i++) {
	omesh->faces[i] = bot->faces[i];
    }
    omesh->vertices = (fastf_t *)bu_calloc(bot->num_vertices * 3, sizeof(fastf_t), "vert array");
    for (size_t i = 0; i < bot->num_vertices * 3; i++) {
	omesh->vertices[i] = bot->vertices[i];
    }
    return omesh;
}

// TODO - need to replace this with logic that uses Manifold's
// upstream examples to write out .glb files in the failure
// case.  See, for example,
//
// https://github.com/elalish/manifold/pull/581#pullrequestreview-1702123678
// https://github.com/elalish/manifold/blob/master/test/samples_test.cpp#L258-L269

/* Byte swaps a four byte value */
static void
lswap(unsigned int *v)
{
    unsigned int r;

    r =*v;
    *v = ((r & 0xff) << 24) | ((r & 0xff00) << 8) | ((r & 0xff0000) >> 8)
	| ((r & 0xff000000) >> 24);
}

static void
tris_to_stl(const char *name, double *vertices, unsigned int *faces, int tricnt)
{
    char output_file[MAXPATHLEN];
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s.stl", name);
    bu_dir(output_file, MAXPATHLEN, BU_DIR_CURR, bu_vls_cstr(&fname), NULL);
    bu_vls_free(&fname);
    int fd = open(output_file, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if (fd < 0)
	return;
    char buf[81];       /* need exactly 80 chars for header */
    memset(buf, 0, sizeof(buf));
    snprintf(buf, 80, "Manifold left input %s", name);
    if (write(fd, &buf, 80) < 0)
	perror("stl write failure\n");

     /* Write out number of triangles */
    unsigned char tot_buffer[4];
    *(uint32_t *)tot_buffer = htonl((unsigned long)tricnt);
    lswap((unsigned int *)tot_buffer);
    if (write(fd, tot_buffer, 4) < 0)
	perror("stl write failure\n");

    /* Write out the vertex data for each triangle */
    point_t A, B, C;
    vect_t BmA, CmA, norm;
    unsigned long vi;
    for (int i = 0; i < tricnt; i++) {
	float flts[12];
	float *flt_ptr;
	unsigned char vert_buffer[50];

	vi = 3*faces[3*i];
	VSET(A, vertices[vi], vertices[vi+1], vertices[vi+2]);
	vi = 3*faces[3*i+1];
	VSET(B, vertices[vi], vertices[vi+1], vertices[vi+2]);
	vi = 3*faces[3*i+2];
	VSET(C, vertices[vi], vertices[vi+1], vertices[vi+2]);

	VSUB2(BmA, B, A);
	VSUB2(CmA, C, A);
	//if (bot->orientation != RT_BOT_CW) {
	    //VCROSS(norm, BmA, CmA);
	VCROSS(norm, CmA, BmA);
	VUNITIZE(norm);

	memset(vert_buffer, 0, sizeof(vert_buffer));

	flt_ptr = flts;
	VMOVE(flt_ptr, norm);
	flt_ptr += 3;
	VMOVE(flt_ptr, A);
	flt_ptr += 3;
	VMOVE(flt_ptr, B);
	flt_ptr += 3;
	VMOVE(flt_ptr, C);

	bu_cv_htonf(vert_buffer, (const unsigned char *)flts, 12);
	for (int j = 0; j < 12; j++) {
	    lswap((unsigned int *)&vert_buffer[j*4]);
	}
	if (write(fd, vert_buffer, 50) < 0)
	    perror("stl write failure\n");
    }

    close(fd);
}

long
bool_meshes(
	double **o_coords, int *o_ccnt, unsigned int **o_tris, int *o_tricnt,
	manifold::OpType b_op,
	double *a_coords, int a_ccnt, unsigned int *a_tris, int a_tricnt,
	double *b_coords, int b_ccnt, unsigned int *b_tris, int b_tricnt,
	const char *lname, const char *rname)
{
    if (!o_coords || !o_ccnt || !o_tris || !o_tricnt)
	return -1;

    manifold::Mesh a_mesh;
    for (int i = 0; i < a_ccnt; i++)
	a_mesh.vertPos.push_back(glm::vec3(a_coords[3*i], a_coords[3*i+1], a_coords[3*i+2]));
    for (int i = 0; i < a_tricnt; i++)
	a_mesh.triVerts.push_back(glm::vec3(a_tris[3*i], a_tris[3*i+1], a_tris[3*i+2]));
    manifold::Mesh b_mesh;
    for (int i = 0; i < b_ccnt; i++)
	b_mesh.vertPos.push_back(glm::vec3(b_coords[3*i], b_coords[3*i+1], b_coords[3*i+2]));
    for (int i = 0; i < b_tricnt; i++)
	b_mesh.triVerts.push_back(glm::vec3(b_tris[3*i], b_tris[3*i+1], b_tris[3*i+2]));

    manifold::Manifold a_manifold(a_mesh);
    if (a_manifold.Status() != manifold::Manifold::Error::NoError) {
	bu_log("Error - a invalid\n");
	return -1;
    }
    manifold::Manifold b_manifold(b_mesh);
    if (b_manifold.Status() != manifold::Manifold::Error::NoError) {
	bu_log("Error - b invalid\n");
	return -1;
    }

    manifold::Manifold result;
    try {
	result = a_manifold.Boolean(b_manifold, b_op);
	if (result.Status() != manifold::Manifold::Error::NoError) {
	    bu_log("Error - bool result invalid\n");
	    return -1;
	}
    } catch (...) {
	bu_log("Manifold boolean library threw failure\n");
	const char *evar = getenv("GED_MANIFOLD_DEBUG");
	// write out the failing inputs to files to aid in debugging
	if (evar && strlen(evar)) {
	    tris_to_stl(lname, a_coords, a_tris, a_tricnt);
	    tris_to_stl(rname, b_coords, b_tris, b_tricnt);
	    bu_exit(EXIT_FAILURE, "Exiting to avoid overwriting debug outputs from Manifold boolean failure.");
	}
	return -1;
    }
    manifold::Mesh rmesh = result.GetMesh();

    (*o_coords) = (double *)calloc(rmesh.vertPos.size()*3, sizeof(double));
    (*o_tris) = (unsigned int *)calloc(rmesh.triVerts.size()*3, sizeof(unsigned int));
    for (size_t i = 0; i < rmesh.vertPos.size(); i++) {
	(*o_coords)[3*i] = rmesh.vertPos[i].x;
	(*o_coords)[3*i+1] = rmesh.vertPos[i].y;
	(*o_coords)[3*i+2] = rmesh.vertPos[i].z;
    }
    for (size_t i = 0; i < rmesh.triVerts.size(); i++) {
	(*o_tris)[3*i] = rmesh.triVerts[i].x;
	(*o_tris)[3*i+1] = rmesh.triVerts[i].y;
	(*o_tris)[3*i+2] = rmesh.triVerts[i].z;
    }
    *o_ccnt = (int)rmesh.vertPos.size();
    *o_tricnt = (int)rmesh.triVerts.size();


    int not_solid = bg_trimesh_solid2(*o_ccnt, *o_tricnt, (fastf_t *)*o_coords, (int *)*o_tris, NULL);
    if (not_solid) {
	*o_ccnt = 0;
	*o_tricnt = 0;
	bu_free(*o_coords, "coords");
	*o_coords = NULL;
	bu_free(*o_tris, "tris");
	*o_tris = NULL;
	bu_log("Error: Manifold boolean succeeded, but result reports as non-solid: failure.");
	return -1;
    }

    return rmesh.triVerts.size();
}

int
_ged_manifold_do_bool(
        union tree *tp, union tree *tl, union tree *tr,
        int op, struct bu_list *UNUSED(vlfree), const struct bn_tol *tol, void *UNUSED(data))
{
    struct rt_bot_internal *lbot = NULL;
    struct rt_bot_internal *rbot = NULL;
    struct manifold_mesh *lmesh = NULL;
    struct manifold_mesh *rmesh = NULL;
    struct manifold_mesh *omesh = NULL;

    // Translate op for MANIFOLD
    manifold::OpType manifold_op = manifold::OpType::Add;
    switch (op) {
	case OP_UNION:
	    manifold_op = manifold::OpType::Add;
	    break;
	case OP_INTERSECT:
	    manifold_op = manifold::OpType::Intersect;
	    break;
	case OP_SUBTRACT:
	    manifold_op = manifold::OpType::Subtract;
	    break;
	default:
	    manifold_op = manifold::OpType::Add;
    };

    // We're either working with the results of CSG NMG tessellations,
    // or we have prior manifold_mesh results - figure out which.
    // If it's the NMG results, we need to make manifold_meshes for
    // processing.
    if (tl->tr_d.td_r) {
	if (!BU_SETJUMP) {
	    /* try */
	    lbot = (struct rt_bot_internal *)nmg_mdl_to_bot(tl->tr_d.td_r->m_p, &RTG.rtg_vlfree, tol);
	} else {
	    /* catch */
	    BU_UNSETJUMP;
	    lbot = NULL;
	    lmesh = NULL;
	} BU_UNSETJUMP;

	if (lbot) {
	    lmesh = bot_to_mmesh(lbot);
	    // We created this locally if it wasn't originally a BoT - clean up
	    if (lbot->vertices)
		bu_free(lbot->vertices, "verts");
	    if (lbot->faces)
		bu_free(lbot->faces, "faces");
	    BU_FREE(lbot, struct rt_bot_internal);
	    lbot = NULL;
	}
    } else {
	lmesh = (struct manifold_mesh *)tl->tr_d.td_d;
    }
    if (tr->tr_d.td_r) {

	if (!BU_SETJUMP) {
	    /* try */
	    rbot = (struct rt_bot_internal *)nmg_mdl_to_bot(tr->tr_d.td_r->m_p, &RTG.rtg_vlfree, tol);
	} else {
	    /* catch */
	    BU_UNSETJUMP;
	    rbot = NULL;
	    rmesh = NULL;
	} BU_UNSETJUMP;

	if (rbot) {
	    rmesh = bot_to_mmesh(rbot);
	    // We created this locally if it wasn't originally a BoT - clean up
	    if (rbot->vertices)
		bu_free(rbot->vertices, "verts");
	    if (rbot->faces)
		bu_free(rbot->faces, "faces");
	    BU_FREE(rbot, struct rt_bot_internal);
	    rbot = NULL;
	}

    } else {
	rmesh = (struct manifold_mesh *)tr->tr_d.td_d;
    }
    int failed = 0;
    if (!lmesh || !rmesh) {
	failed = 1;
    } else {
	BU_GET(omesh, struct manifold_mesh);
	int mret = bool_meshes(
		(double **)&omesh->vertices, &omesh->num_vertices, &omesh->faces, &omesh->num_faces,
		manifold_op,
		(double *)lmesh->vertices, lmesh->num_vertices, lmesh->faces, lmesh->num_faces,
		(double *)rmesh->vertices, rmesh->num_vertices, rmesh->faces, rmesh->num_faces,
		tl->tr_d.td_name,
		tr->tr_d.td_name
		);

	failed = (mret < 0) ? 1 : 0;

	if (failed) {
	    // TODO - we should be able to try an NMG boolean op here as a
	    // fallback, but we may need to translate one or both of the inputs
	    // into NMG
	    bu_free(omesh->faces, "faces");
	    bu_free(omesh->vertices, "vertices");
	    BU_PUT(omesh, struct manifold_mesh);
	    omesh = NULL;
	}
    }

    // Memory cleanup
    if (tl->tr_d.td_r) {
	nmg_km(tl->tr_d.td_r->m_p);
	tl->tr_d.td_r = NULL;
    }
    if (tr->tr_d.td_r) {
	nmg_km(tr->tr_d.td_r->m_p);
	tr->tr_d.td_r = NULL;
    }
    if (tl->tr_d.td_d) {
	struct manifold_mesh *km = (struct manifold_mesh *)tl->tr_d.td_d;
	bu_free(km->faces, "faces");
	bu_free(km->vertices, "vertices");
	BU_PUT(km, struct manifold_mesh);
	tl->tr_d.td_d = NULL;
    }
    if (tr->tr_d.td_d) {
	struct manifold_mesh *km = (struct manifold_mesh *)tr->tr_d.td_d;
	bu_free(km->faces, "faces");
	bu_free(km->vertices, "vertices");
	BU_PUT(km, struct manifold_mesh);
	tr->tr_d.td_d = NULL;
    }

    tp->tr_d.td_r = NULL;

    if (failed) {
	tp->tr_d.td_d = NULL;
	return -1;
    }

    tp->tr_op = OP_TESS;
    tp->tr_d.td_d = omesh;
    return 0;
}

#if 0
static struct manifold_mesh *
_try_manifold_facetize(struct _ged_facetize_state *s, int argc, const char **argv)
{
    struct ged *gedp = s->gedp;
    int i;
    union tree *ftree = NULL;
    struct manifold_mesh *omesh = NULL;

    _ged_facetize_log_nmg(s);

    struct db_tree_state init_state;
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    db_init_db_tree_state(&init_state, gedp->dbip, wdbp->wdb_resp);
    /* Establish tolerances */
    init_state.ts_ttol = &wdbp->wdb_ttol;
    init_state.ts_tol = &wdbp->wdb_tol;
    init_state.ts_m = NULL;
    union tree *facetize_tree = (union tree *)0;

    if (!BU_SETJUMP) {
	/* try */
	i = db_walk_tree(gedp->dbip, argc, (const char **)argv,
			 1,
			&init_state,
			 0,			/* take all regions */
			 facetize_region_end,
			 rt_booltree_leaf_tess,
			 (void *)&facetize_tree
			);
    } else {
	/* catch */
	BU_UNSETJUMP;
	_ged_facetize_log_default(s);
	return NULL;
    } BU_UNSETJUMP;

    if (i < 0) {
	/* Destroy NMG */
	_ged_facetize_log_default(s);
	return NULL;
    }

    if (facetize_tree) {
	ftree = rt_booltree_evaluate(facetize_tree, &RTG.rtg_vlfree, &wdbp->wdb_tol, &rt_uniresource, &_manifold_do_bool, 0, (void *)s);
	if (!ftree) {
	    _ged_facetize_log_default(s);
	    return NULL;
	}
	if (ftree->tr_d.td_d) {
	    omesh = (struct manifold_mesh *)ftree->tr_d.td_d;
	    _ged_facetize_log_default(s);
	    return omesh;
	} else if (ftree->tr_d.td_r) {
	    // If we had only one NMG mesh, there was no bool
	    // operation - we need to set up a mesh
	    struct rt_bot_internal *bot = NULL;
	    if (!BU_SETJUMP) {
		/* try */
		bot = (struct rt_bot_internal *)nmg_mdl_to_bot(ftree->tr_d.td_r->m_p, &RTG.rtg_vlfree, &wdbp->wdb_tol);
	    } else {
		/* catch */
		BU_UNSETJUMP;
		bot = NULL;
		omesh = NULL;
	    } BU_UNSETJUMP;

	    if (bot) {
		omesh = bot_to_mmesh(bot);
		// We created this locally if it wasn't originally a BoT - clean up
		if (bot->vertices)
		    bu_free(bot->vertices, "verts");
		if (bot->faces)
		    bu_free(bot->faces, "faces");
		BU_FREE(bot, struct rt_bot_internal);
		bot = NULL;
	    }
	}
    }

    _ged_facetize_log_default(s);

    return omesh;
}

// For MANIFOLD, we do a tree walk.  Each solid is individually triangulated, and
// then the boolean op is applied with the result of the mesh generated thus
// far.  This is conceptually similar to how NMG does its processing, with the
// main difference being that the external code is actually doing the mesh
// boolean processing rather than libnmg.
int
_ged_manifold_obj(struct _ged_facetize_state *s, const char *objname, const char *newname)
{
    int ret = BRLCAD_ERROR;
    struct manifold_mesh *mesh = NULL;
    int ac;
    const char *av[2];
    if (!s || !objname || !newname)
	goto ged_manifold_obj_memfree;

    ac = 1;
    av[0] = objname;
    av[1] = NULL;
    mesh = _try_manifold_facetize(s, ac, av);

    if (mesh && mesh->num_faces > 0) {
	struct rt_bot_internal *bot;
	BU_GET(bot, struct rt_bot_internal);
	bot->magic = RT_BOT_INTERNAL_MAGIC;
	bot->mode = RT_BOT_SOLID;
	bot->orientation = RT_BOT_CCW;
	bot->bot_flags = 0;
	bot->num_vertices = mesh->num_vertices;
	bot->num_faces = mesh->num_faces;
	bot->thickness = NULL;
	bot->face_mode = (struct bu_bitv *)NULL;
	bot->num_normals = 0;
	bot->num_face_normals = 0;
	bot->normals = NULL;
	bot->face_normals = NULL;
	bot->vertices = (fastf_t *)bu_calloc(mesh->num_vertices * 3, sizeof(fastf_t), "vertices");
	for (int i = 0; i < mesh->num_vertices * 3; i++) {
	    bot->vertices[i] = mesh->vertices[i];
	}
	bot->faces = (int *)bu_calloc(mesh->num_faces * 3, sizeof(int), "faces");
	for (int i = 0; i < mesh->num_faces * 3; i++) {
	    bot->faces[i] = mesh->faces[i];
	}

	ret = _ged_facetize_write_bot(s, bot, newname);
    } else {
	goto ged_manifold_obj_memfree;
    }

ged_manifold_obj_memfree:
    if (mesh) {
	if (mesh->vertices)
	    bu_free(mesh->vertices, "verts");
	if (mesh->faces)
	    bu_free(mesh->faces, "faces");
	BU_FREE(mesh, struct manifold_mesh);
    }
    if (!s->quiet && ret != BRLCAD_OK) {
	bu_log("MANIFOLD: failed to generate %s\n", newname);
    }

    return ret;
}
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

