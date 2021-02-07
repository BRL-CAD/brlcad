/*                       G - S A T . C P P
 * BRL-CAD
 *
 * Copyright (c) 1993-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file g-sat.cpp
 *
 * C++ code for converting BRL-CAD models to the ACIS SAT format for
 * importing into CUBIT.  This code assumes that your receiving format
 * can handle CSG primitives and Boolean trees with transformation
 * matrices.
 *
 */

#include "common.h"

// system headers
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <map>
#include <vector>
#include <stack>
#include <string.h>
#include "bio.h"

// CGM headers
#ifndef TEST_BUILD
#include "NoAcisQueryEngine.hpp"
#include "NoAcisModifyEngine.hpp"
#include "GeometryQueryTool.hpp"
#include "GeometryModifyTool.hpp"
#include "AppUtil.hpp"
#include "CGMApp.hpp"
#include "CastTo.hpp"
#include "Body.hpp"
#include "RefEntity.hpp"
#include "RefEntityName.hpp"
#include "CubitAttrib.hpp"
#include "CubitAttribManager.hpp"
#else
#include "shim.h"
#endif

// interface headers
#include "vmath.h"
#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/avs.h"
#include "bu/bitv.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"


const int NUM_OF_CPUS_TO_USE = 1;

const int DEBUG_NAMES = 1;

const long debug = 1;
int verbose = 0;

static bg_tess_tol ttol;   /* tessellation tolerance in mm */
static bn_tol tol;    /* calculation tolerance */

// Global map for bodies names in the CGM global list
std::map<std::string, int> g_body_id_map;
std::map<std::string, int>::iterator g_itr;

int g_body_cnt = 0;

std::vector <std::string> g_CsgBoolExp;

const char *usage_msg = "Usage: %s [-v] [-xX lvl] [-a abs_tol] [-r rel_tol] [-n norm_tol] [-o out_file] brlcad_db.g object(s)\n";
const char *options = "t:a:n:o:r:vx:X:";
char *prog_name = NULL;
const char *output_file = NULL;


static void
tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters)
{
    // Skip delimiters at beginning.
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);

    // Find first "non-delimiter".
    std::string::size_type pos     = str.find_first_of(delimiters, lastPos);

    while (std::string::npos != pos || std::string::npos != lastPos) {
	// Found a token, add it to the vector.
	tokens.push_back(str.substr(lastPos, pos - lastPos));

	// Skip delimiters.  Note the "not_of"
	lastPos = str.find_first_not_of(delimiters, pos);

	// Find next "non-delimiter"
	pos = str.find_first_of(delimiters, lastPos);
    }
}


static std::string
infix_to_postfix(std::string str)
{

    std::stack <char> s;
    std::ostringstream ostr;
    char c;

    for (unsigned int i = 0; i < strlen(str.c_str()); i++) {
	c = str[i];
	if (c == '(') {
	    s.push(c);
	} else if (c == ')') {
	    while ((c = s.top()) != '(') {
		ostr << ' ' << c;
		s.pop();
	    }
	    s.pop();
	} else if (((c == 'u') || (c == '+') || (c == '-')) && (str[i+1] == ' ')) {
	    s.push(c);
	    i++;
	} else {
	    ostr << c ;
	}
    }

    if (!s.empty()) {
	ostr << s.top();
	s.pop();
    }

    return ostr.str();
}


static void
usage(const char *s)
{
    if (s) {
	fputs(s, stderr);
    }

    bu_exit(1, usage_msg, prog_name);
}


static int
parse_args(int ac, char **av)
{
    int c;

    if (! (prog_name=strrchr(*av, '/')))
	prog_name = *av;
    else
	++prog_name;

    /* Turn off bu_getopt error messages */
    bu_opterr = 0;

    /* get all the option flags from the command line */
    while ((c = bu_getopt(ac, av, options)) != EOF) {

	switch (c) {
	    case 't':               /* calculational tolerance */
		tol.dist = atof(bu_optarg);
		tol.dist_sq = tol.dist * tol.dist;
		/* fall through */
	    case 'o':               /* Output file name */
		/* grab output file name */
		output_file = bu_optarg;
		break;
	    case 'v':               /* verbosity */
		verbose++;
		break;
	    case 'x':               /* librt debug flag */
		sscanf(bu_optarg, "%x", &rt_debug);
		bu_printb("librt RT_G_DEBUG", RT_G_DEBUG, RT_DEBUG_FORMAT);
		bu_log("\n");
		break;
	    case 'X':               /* NMG debug flag */
		sscanf(bu_optarg, "%x", &nmg_debug);
		bu_printb("librt nmg_debug", nmg_debug, NMG_DEBUG_FORMAT);
		bu_log("\n");
		break;
	    default:
		usage("Bad or help flag specified\n");
		break;
	}

    }

    return bu_optind;
}


// set_body_id function
static void
set_body_id(std::string body_name, int body_id)
{
    g_body_id_map[body_name] = body_id;
}


// get_body_id function
static int
get_body_id(std::string body_name)
{
    const int ERR_FLAG = -99;
    int rVal;

    std::map<std::string, int>::iterator iter;
    iter = g_body_id_map.find(body_name);

    if (iter != g_body_id_map.end()) {
	rVal = iter->second;
    } else {
	rVal = ERR_FLAG;
    }

    return rVal;
}


/* This routine just produces an ascii description of the Boolean tree.
 * In a real converter, this would output the tree in the desired format.
 */
static void
describe_tree(tree *tree, bu_vls *str)
{
    bu_vls left = BU_VLS_INIT_ZERO;
    bu_vls right = BU_VLS_INIT_ZERO;
    char op='\0';

    BU_CK_VLS(str);

    if (!tree) {
	/* this tree has no members */
	bu_vls_strcat(str, "{empty}");
	set_body_id("{empty}", -1);
	return;
    }

    RT_CK_TREE(tree);

    /* Handle all the possible node types.
     * the first four are the most common types, and are typically
     * the only ones found in a BRL-CAD database.
     */
    switch (tree->tr_op) {
	case OP_DB_LEAF:	/* leaf node, this is a member */
	    /* Note: tree->tr_l.tl_mat is a pointer to a
	     * transformation matrix to apply to this member
	     */
	    bu_vls_strcat(str,  tree->tr_l.tl_name);
	    break;
	case OP_UNION:		/* operator node */
	    op = DB_OP_UNION;
	    goto binary;
	case OP_INTERSECT:	/* intersection operator node */
	    op = DB_OP_INTERSECT;
	    goto binary;
	case OP_SUBTRACT:	/* subtraction operator node */
	    op = DB_OP_SUBTRACT;
	    goto binary;
	binary:				/* common for all binary nodes */
	    describe_tree(tree->tr_b.tb_left, &left);
	    describe_tree(tree->tr_b.tb_right, &right);
	    bu_vls_putc(str, '(');
	    bu_vls_vlscatzap(str, &left);
	    bu_vls_printf(str, " %c ", op);
	    bu_vls_vlscatzap(str, &right);
	    bu_vls_putc(str, ')');
	    break;
	case OP_NOT:
	    bu_vls_strcat(str, "(!");
	    describe_tree(tree->tr_b.tb_left, str);
	    bu_vls_putc(str, ')');
	    break;
	case OP_GUARD:
	    bu_vls_strcat(str, "(G");
	    describe_tree(tree->tr_b.tb_left, str);
	    bu_vls_putc(str, ')');
	    break;
	case OP_XNOP:
	    bu_vls_strcat(str, "(X");
	    describe_tree(tree->tr_b.tb_left, str);
	    bu_vls_putc(str, ')');
	    break;
	case OP_NOP:
	    bu_vls_strcat(str, "NOP");
	    break;
	default:
	    bu_exit(2, "ERROR: describe_tree() got unrecognized op (%d)\n", tree->tr_op);
    }
}


/**
 * @brief This routine is called when a region is first encountered in the
 * hierarchy when processing a tree
 *
 * @param pathp A listing of all the nodes traversed to get to this node in the database
 * @param combp the combination record for this region
 *
 */
static int
region_start (db_tree_state *UNUSED(tsp),
	      const db_full_path *pathp,
	      const rt_comb_internal *combp,
	      void *UNUSED(client_data))
{
    directory *dp;
    bu_vls str = BU_VLS_INIT_ZERO;
    std::ostringstream ostr;
    std::string infix, postfix;

    if (debug&DEBUG_NAMES) {
	char *name = db_path_to_string(pathp);
	bu_log("region_start %s\n", name);
	bu_free(name, "reg_start name");
    }

    dp = DB_FULL_PATH_CUR_DIR(pathp);

    /* here is where the conversion should be done */
    std::cout << "* Here is where the conversion should be done *" << std::endl;
    printf("Write this region (name=%s) as a part in your format:\n", dp->d_namep);

    describe_tree(combp->tree, &str);

    printf("\t%s\n\n", bu_vls_addr(&str));
    ostr << bu_vls_addr(&str);

    bu_vls_free(&str);

    infix = ostr.str();
    postfix = infix_to_postfix(infix);
    std::cout << "\tIn postfix: "<< postfix << std::endl << std::endl;
    g_CsgBoolExp.push_back(postfix);

    return 0;
}


/**
 * @brief This is called when all sub-elements of a region have been processed by leaf_func.
 *
 * @return TREE_NULL if data in curtree was "stolen", otherwise db_walk_tree will
 * clean up the data in the tree * that is returned
 *
 * If it wants to retain the data in curtree it can by returning TREE_NULL.  Otherwise
 * db_walk_tree will clean up the data in the tree * that is returned.
 *
 */
static tree *
region_end (db_tree_state *UNUSED(tsp),
	    const db_full_path *pathp,
	    tree *curtree,
	    void *UNUSED(client_data))
{
    if (debug&DEBUG_NAMES) {
	char *name = db_path_to_string(pathp);
	bu_log("region_end   %s\n", name);
	bu_free(name, "region_end name");
    }

    return curtree;
}


/* routine to output the facetted NMG representation of a BRL-CAD region */
static bool
make_bot(nmgregion *r,
	 model *UNUSED(m),
	 shell *s)
{
    // Create the geometry modify and query tool
    GeometryModifyTool *gmt = GeometryModifyTool::instance();
    GeometryQueryTool *gqt = GeometryQueryTool::instance();

    vertex *v;

    CubitVector cv;
    DLIList <RefVertex*> VertexList;
    DLIList <RefEdge*> EdgeList;
    DLIList <RefFace*> FaceList;

    point_t bot_min, bot_max;  // bounding box points
    point_t bot_cp;

    // initialize bot_min and bot_max
    VSETALL(bot_min, INFINITY);
    VSETALL(bot_max, -INFINITY);

    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	faceuse *fu;

	NMG_CK_SHELL(s);

	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    loopuse *lu;

	    NMG_CK_FACEUSE(fu);

	    if (fu->orientation != OT_SAME)
		continue;

	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		edgeuse *eu;

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		/* loop through the edges in this loop (facet) */
		// printf("\tfacet:\n");
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    NMG_CK_EDGEUSE(eu);

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);
		    VMINMAX(bot_min, bot_max, v->vg_p->coord);
		    // printf("\t\t(%g %g %g)\n", V3ARGS(v->vg_p->coord));
		    cv.set(V3ARGS(v->vg_p->coord));
		    VertexList.append(gmt->make_RefVertex(cv));
		}
		const int MAX_VERTICES = 3;
		for (int i=0; i <= MAX_VERTICES-1; i++) {
		    EdgeList.append(gmt->make_RefEdge(STRAIGHT_CURVE_TYPE,
						      VertexList[i], VertexList[(i+1)%MAX_VERTICES]));
		}

		RefFace *face = gmt->make_RefFace(PLANE_SURFACE_TYPE, EdgeList);

		EdgeList.clean_out();
		VertexList.clean_out();

		FaceList.append(face);
	    }

	}

    }
    DLIList <Body*> BodyList;

    CubitStatus status;

    status = gmt->create_solid_bodies_from_surfs(FaceList, BodyList);
    // status = gmt->create_body_from_surfs(FaceList, BotBody);

    Body *BotBody, *RegBotBody;
    for (int i=0; i < BodyList.size(); i++) {
	BotBody = BodyList[i];

	if (status != CUBIT_FAILURE) {
	    std::cout << "make_bot made a Body!" << std::endl;
	    gmt->regularize_body(BotBody, RegBotBody);
	} else {
	    std::cout << "make_bot did not made a Body! Substituted bounding box instead of Body." << std::endl;

	    double bb_width = fabs(bot_max[0] - bot_min[0]);
	    double bb_depth = fabs(bot_max[1] - bot_min[1]);
	    double bb_height = fabs(bot_max[2] - bot_min[2]);

	    gmt->brick(bb_width, bb_depth, bb_height);

	    VSUB2SCALE(bot_cp, bot_max, bot_min, 0.5);
	    VADD2(bot_cp, bot_cp, bot_min);
	    CubitVector bbox_cp(V3ARGS(bot_cp));

	    status = gqt->translate(gqt->get_last_body(), bbox_cp);
	}
    }

    FaceList.clean_out();

    return status;
}


/* This routine is called by the tree walker (db_walk_tree)
 * for every primitive encountered in the trees specified on the command line */
static tree *
primitive_func(db_tree_state *tsp,
	       const db_full_path *pathp,
	       rt_db_internal *ip,
	       void *UNUSED(client_data))
{
    const double NEARZERO = 0.0001;

    int i;
    std::ostringstream ostr;
    std::string name;

    directory *dp;

    // Create the geometry modify and query tool
    GeometryModifyTool *gmt = GeometryModifyTool::instance();
    GeometryQueryTool *gqt = GeometryQueryTool::instance();

    dp = DB_FULL_PATH_CUR_DIR(pathp);

    if (debug&DEBUG_NAMES) {
	char *cname = db_path_to_string(pathp);
	bu_log("leaf_func    %s\n", cname);
	bu_free(cname, "region_end name");
    }

    /* handle each type of primitive (see h/rtgeom.h) */
    if (ip->idb_major_type == DB5_MAJORTYPE_BRLCAD) {
	switch (ip->idb_minor_type) {
	    /* most commonly used primitives */
	    case ID_TOR:	/* torus */
		{
		    CubitVector x_axis(1.0, 0.0, 0.0);
		    CubitVector y_axis(0.0, 1.0, 0.0);
		    CubitVector z_axis(0.0, 0.0, 1.0);

		    rt_tor_internal *tor = (rt_tor_internal *)ip->idb_ptr;

		    if (debug&DEBUG_NAMES) {
			printf("Write this torus (name=%s) in your format:\n", dp->d_namep);
			printf("\tV=(%g %g %g)\n", V3ARGS(tor->v));
			printf("\tnormal=(%g %g %g)\n", V3ARGS(tor->h));
			printf("\tradius1 = %g\n", tor->r_a);
			printf("\tradius2 = %g\n", tor->r_h);
		    }

		    CubitVector tor_v(V3ARGS(tor->v));
		    CubitVector tor_h(V3ARGS(tor->h));

		    double tor_ra = tor->r_a;
		    double tor_rh = tor->r_h;

		    if (gmt->torus(tor_ra, tor_rh)) {
			g_body_cnt++;
			ostr << dp->d_namep;
			name = ostr.str();
			set_body_id(name, g_body_cnt);
			ostr.flush();

			CubitVector raxis = z_axis * tor_h;
			double rangle = z_axis.interior_angle(tor_h);

			gqt->rotate(gqt->get_last_body(), raxis, rangle);
			gqt->translate(gqt->get_last_body(), tor_v);
		    }

		    break;
		}
	    case ID_TGC: /* truncated general cone frustum */
	    case ID_REC: /* right elliptical cylinder */
		{
		    /* This primitive includes circular cross-section
		     * cones and cylinders
		     */
		    CubitVector x_axis(1.0, 0.0, 0.0);
		    CubitVector y_axis(0.0, 1.0, 0.0);
		    CubitVector z_axis(0.0, 0.0, 1.0);

		    bool direct_convert = false;

		    fastf_t maxb, ma, mb, mc, md, mh;
		    vect_t axb;

		    CubitVector center_of_base;

		    rt_tgc_internal *tgc = (rt_tgc_internal *)ip->idb_ptr;

		    if (debug&DEBUG_NAMES) {
			printf("Write this TGC (name=%s) in your format:\n", dp->d_namep);
			printf("\tV=(%g %g %g)\n", V3ARGS(tgc->v));
			printf("\tH=(%g %g %g)\n", V3ARGS(tgc->h));
			printf("\tA=(%g %g %g)\n", V3ARGS(tgc->a));
			printf("\tB=(%g %g %g)\n", V3ARGS(tgc->b));
			printf("\tC=(%g %g %g)\n", V3ARGS(tgc->c));
			printf("\tD=(%g %g %g)\n", V3ARGS(tgc->d));
		    }

		    CubitVector tgc_v(V3ARGS(tgc->v));
		    CubitVector tgc_h(V3ARGS(tgc->h));
		    CubitVector tgc_a(V3ARGS(tgc->a));
		    CubitVector tgc_b(V3ARGS(tgc->b));
		    CubitVector tgc_c(V3ARGS(tgc->c));
		    CubitVector tgc_d(V3ARGS(tgc->d));

		    VCROSS(axb, tgc->a, tgc->b);
		    maxb = MAGNITUDE(axb);
		    ma = MAGNITUDE(tgc->a);
		    mb = MAGNITUDE(tgc->b);
		    mc = MAGNITUDE(tgc->c);
		    md = MAGNITUDE(tgc->d);
		    mh = MAGNITUDE(tgc->h);

		    // check for right cone or cylinder
		    if (fabs(fabs(VDOT(tgc->h, axb))-(mh*maxb)) < NEARZERO) {
			// have a right cylinder or cone
			if ((fabs(ma - mb) < NEARZERO) && (fabs(mc - md)  < NEARZERO)) {
			    std::cout << "DEBUG: This TGC is a rcc or trc" << std::endl;
			    direct_convert = true;
			} else if ((fabs(ma - mc) < NEARZERO) && (fabs(mb - md)  < NEARZERO)) {
			    std::cout << "DEBUG: This TGC is a rec" << std::endl;
			    direct_convert = true;
			} else if ((fabs(ma/mc) - fabs(mb/md)) < NEARZERO) {
			    std::cout << "DEBUG: This TGC is a tec" << std::endl;
			    direct_convert = true;
			} else {
			    std::cout << "DEBUG: This TGC is a right tgc" << std::endl;
			    direct_convert = false;
			}
		    }

		    if (direct_convert) {
			if (gmt->cylinder(mh, ma, mb, mc)) {
			    g_body_cnt++;
			    ostr << dp->d_namep;
			    name = ostr.str();
			    set_body_id(name, g_body_cnt);
			    ostr.flush();

			    if ((fabs(ma - mb) > NEARZERO)) {
				double axbangle = x_axis.interior_angle(tgc_a);
				std::cout << "axbangle = " << axbangle << std::endl;
				if (axbangle > NEARZERO) {
				    gqt->rotate(gqt->get_last_body(), z_axis, axbangle);
				}
			    }

			    CubitVector raxis = z_axis * tgc_h;
			    double rangle = z_axis.interior_angle(tgc_h);
			    std::cout << "rangle = " << rangle << std::endl;
			    if (rangle > NEARZERO) {
				gqt->rotate(gqt->get_last_body(), raxis, rangle);
			    }

			    CubitVector tgc_cp = tgc_v + (tgc_h / 2.0);
			    gqt->translate(gqt->get_last_body(), tgc_cp);
			}
		    } else {
			goto TESS_CASE;
		    }

		    break;
		}
	    case ID_ELL:
	    case ID_SPH:
		{
		    CubitVector x_axis(1.0, 0.0, 0.0);
		    CubitVector y_axis(0.0, 1.0, 0.0);
		    CubitVector z_axis(0.0, 0.0, 1.0);

		    /* spheres and ellipsoids */
		    rt_ell_internal *ell = (rt_ell_internal *)ip->idb_ptr;

		    if (debug&DEBUG_NAMES) {
			printf("Write this ellipsoid (name=%s) in your format:\n", dp->d_namep);
			printf("\tV=(%g %g %g)\n", V3ARGS(ell->v));
			printf("\tA=(%g %g %g)\n", V3ARGS(ell->a));
			printf("\tB=(%g %g %g)\n", V3ARGS(ell->b));
			printf("\tC=(%g %g %g)\n", V3ARGS(ell->c));
		    }

		    CubitVector ell_v(V3ARGS(ell->v));

		    double magsq_a = MAGSQ (ell->a);
		    double magsq_b = MAGSQ (ell->b);
		    double magsq_c = MAGSQ (ell->c);

		    if ((fabs(magsq_a - magsq_b) < NEARZERO) && (fabs(magsq_a - magsq_c) < NEARZERO)) {
			if (gmt->sphere(sqrt(magsq_a))) {
			    g_body_cnt++;
			    ostr << dp->d_namep;
			    name = ostr.str();
			    set_body_id(name, g_body_cnt);
			    ostr.flush();

			    gqt->translate(gqt->get_last_body(), ell_v);
			}
		    } else {
			CubitVector rot_axis;

			CubitVector ell_a(V3ARGS(ell->a));
			CubitVector ell_b(V3ARGS(ell->b));
			CubitVector ell_c(V3ARGS(ell->c));

			double mag_a = sqrt(magsq_a);
			double mag_b = sqrt(magsq_b);
			double mag_c = sqrt(magsq_c);

			if (gmt->sphere(1.0)) {
			    g_body_cnt++;
			    ostr << dp->d_namep;
			    name = ostr.str();
			    set_body_id(name, g_body_cnt);
			    ostr.flush();

			    CubitVector scale_vector(mag_a, mag_b, mag_c);
			    gqt->scale(gqt->get_last_body(), scale_vector, true);

			    if ((fabs(mag_a - mag_b) > NEARZERO)) {
				double axbangle = x_axis.interior_angle(ell_a);
				gqt->rotate(gqt->get_last_body(), z_axis, axbangle);
			    }

			    CubitVector raxis = z_axis * ell_c;
			    double hangle = z_axis.interior_angle(ell_c);
			    gqt->rotate(gqt->get_last_body(), raxis, hangle);
			    gqt->translate(gqt->get_last_body(), ell_v);
			}

		    }

		    break;
		}
	    case ID_ARB8:	/* convex primitive with from four to six faces */
		{
		    /* this primitive may have degenerate faces
		     * faces are: 0123, 7654, 0347, 1562, 0451, 3267
		     * (points listed above in counter-clockwise order)
		     */
		    rt_arb_internal *arb = (rt_arb_internal *)ip->idb_ptr;

		    if (debug&DEBUG_NAMES) {
			printf("Write this ARB (name=%s) in your format:\n", dp->d_namep);
			for (i=0 ; i<8 ; i++) {
			    printf("\tpoint #%d: (%g %g %g)\n", i, V3ARGS(arb->pt[i]));
			}
		    }

		    goto TESS_CASE;
		}

	    case ID_BOT:	/* Bag O' Triangles */
		{
		    goto TESS_CASE;

		    break;
		}

		/* less commonly used primitives */
	    case ID_ARS:
		{
		    /* series of curves
		     * each with the same number of points
		     */
		    goto TESS_CASE;

		    break;
		}
	    case ID_HALF:
		{
		    /* half universe defined by a plane */
		    goto TESS_CASE;

		    break;
		}
	    case ID_POLY:
		{
		    /* polygons (up to 5 vertices per) */
		    goto TESS_CASE;

		    break;
		}
	    case ID_BSPLINE:
		{
		    /* NURB surfaces */
		    goto TESS_CASE;

		    break;
		}
	    case ID_NMG:
		{
		    nmgregion *r = NULL;
		    shell *s = NULL;

		    /* N-manifold geometry */
		    model *m = (model *)ip->idb_ptr;

		    NMG_CK_MODEL(m);

		    /* walk the nmg to convert it to triangular facets */
		    nmg_triangulate_model(m, NULL, tsp->ts_tol);

		    if (make_bot(r, m, s)) {
			g_body_cnt++;
			ostr << dp->d_namep;
			name = ostr.str();
			set_body_id(name, g_body_cnt);
			ostr.flush();
		    }

		    break;
		}
	    case ID_ARBN:
		{
		    goto TESS_CASE;

		    break;
		}

	    case ID_DSP:
		{
		    /* Displacement map (terrain primitive) */
		    /* normally used for terrain only */
		    /* the DSP primitive may reference an external file */
		    goto TESS_CASE;

		    break;
		}
	    case ID_HF:
		{
		    /* height field (terrain primitive) */
		    /* the HF primitive references an external file */
		    goto TESS_CASE;

		    break;
		}

		/* rarely used primitives */
	    case ID_EBM:
		{
		    /* extruded bit-map */
		    /* the EBM primitive references an external file */
		    goto TESS_CASE;

		    break;
		}
	    case ID_VOL:
		{
		    /* the VOL primitive references an external file */
		    goto TESS_CASE;

		    break;
		}
	    case ID_PIPE:
		{
		    goto TESS_CASE;

		    break;
		}
	    case ID_PARTICLE:
		{
		    goto TESS_CASE;

		    break;
		}
	    case ID_RPC:
		{
		    goto TESS_CASE;

		    break;
		}
	    case ID_RHC:
		{
		    goto TESS_CASE;

		    break;
		}
	    case ID_EPA:
		{
		    goto TESS_CASE;

		    break;
		}
	    case ID_EHY:
		{
		    goto TESS_CASE;

		    break;
		}
	    case ID_ETO:
		{
		    goto TESS_CASE;

		    break;
		}
	    case ID_GRIP:
		{
		    goto TESS_CASE;

		    break;
		}

	    case ID_SKETCH:
		{
		    goto TESS_CASE;

		    break;
		}
	    case ID_EXTRUDE:
		{
		    /* note that an extrusion references a sketch, make sure you convert
		     * the sketch also
		     */
		    goto TESS_CASE;

		    break;
		}

	    default:    TESS_CASE:
		// bu_log("Primitive %s is unrecognized type (%d)\n", dp->d_namep, ip->idb_type);
		// break;
		{
		    /* This section is for primitives which cannot be directly represented in the
		     * format we are going to.  So we convert it to triangles
		     */
		    nmgregion *r = NULL;
		    model *m = nmg_mm();
		    shell *s = NULL;

		    NMG_CK_MODEL(m);

		    if (OBJ[ip->idb_type].ft_tessellate(&r, m, ip, tsp->ts_ttol, tsp->ts_tol) != 0) {
			bu_exit(1, "Failed to tessellate!\n");
		    }

		    //bu_log("triangulate %d\n", ip->idb_minor_type);
		    /* walk the nmg to convert it to triangular facets */
		    nmg_triangulate_model(m, NULL, tsp->ts_tol);

		    if (make_bot(r, m, s)) {
			g_body_cnt++;
			ostr << dp->d_namep;
			name = ostr.str();
			set_body_id(name, g_body_cnt);
			ostr.flush();
		    }


		    break;
		}


	}
    } else {
	switch (ip->idb_major_type) {
	    case DB5_MAJORTYPE_BINARY_UNIF:
		{
		    /* not actually a primitive, just a block of storage for data
		     * a uniform array of chars, ints, floats, doubles, ...
		     */
		    printf("Found a binary object (%s)\n\n", dp->d_namep);
		    break;
		}
	    default:
		bu_log("Major type of %s is unrecognized type (%d)\n", dp->d_namep, ip->idb_major_type);
		break;
	}
    }

    return (tree *) NULL;
}

// Defined but not used (??)
#if 0
/* routine to output the facetted NMG representation of a BRL-CAD region */
static void
output_triangles(nmgregion *r,
		 model *UNUSED(m),
		 shell *s)
{
    vertex *v;

    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	faceuse *fu;

	NMG_CK_SHELL(s);

	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    loopuse *lu;
	    vect_t facet_normal;

	    NMG_CK_FACEUSE(fu);

	    if (fu->orientation != OT_SAME)
		continue;

	    /* Grab the face normal if needed */
	    NMG_GET_FU_NORMAL(facet_normal, fu);

	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		edgeuse *eu;

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		/* loop through the edges in this loop (facet) */
		printf("\tfacet:\n");
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    NMG_CK_EDGEUSE(eu);

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);
		    printf("\t\t(%g %g %g)\n", V3ARGS(v->vg_p->coord));
		}
	    }
	}
    }
}
#endif


tree *
booltree_evaluate(tree *tp, resource *resp)
{
    union tree *tl;
    union tree *tr;
    char op;
    char *name;
    int namelen;

    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_NOP:
	    return 0;
	case OP_DB_LEAF:
	    /* Hit a tree leaf */
	    return tp;
	case OP_UNION:
	    op = DB_OP_UNION;
	    break;
	case OP_INTERSECT:
	    op = DB_OP_INTERSECT;
	    break;
	case OP_SUBTRACT:
	    op = DB_OP_SUBTRACT;
	    break;
	default:
	    bu_log("booltree_evaluate: bad op %d\n", tp->tr_op);
	    return 0;
    }
    /* Handle a boolean operation node.  First get its leaves. */
    tl = booltree_evaluate(tp->tr_b.tb_left, resp);
    tr = booltree_evaluate(tp->tr_b.tb_right, resp);

    if (tl == 0 || !tl->tr_d.td_r) {
	if (tr == 0 || !tr->tr_d.td_r)
	    return 0;
	if (op == DB_OP_UNION)
	    return tr;
	/* For sub and intersect, if lhs is 0, result is null */
	//db_free_tree(tr);
	tp->tr_b.tb_right = TREE_NULL;
	tp->tr_op = OP_NOP;
	return 0;
    }
    if (tr == 0 || !tr->tr_d.td_r) {
	if (tl == 0 || !tl->tr_d.td_r)
	    return 0;
	if (op == DB_OP_INTERSECT) {
	    db_free_tree(tl, resp);
	    tp->tr_b.tb_left = TREE_NULL;
	    tp->tr_op = OP_NOP;
	    return 0;
	}
	/* For sub and add, if rhs is 0, result is lhs */
	return tl;
    }
    if (tl->tr_op != OP_DB_LEAF) bu_exit(2, "booltree_evaluate() bad left tree\n");
    if (tr->tr_op != OP_DB_LEAF) bu_exit(2, "booltree_evaluate() bad right tree\n");

    bu_log(" {%s} %c {%s}\n", tl->tr_d.td_name, op, tr->tr_d.td_name);
    std::cout << "******" << tl->tr_d.td_name << " " << (char)op << " " << tr->tr_d.td_name << "***********" << std::endl;

    /* Build string of result name */
    namelen = strlen(tl->tr_d.td_name) + 1 /* for op */ + 2 /* for spaces */ + strlen(tr->tr_d.td_name) + 3;
    name = (char *)bu_malloc(namelen, "booltree_evaluate name");

    snprintf(name, namelen, "(%s %c %s)", tl->tr_d.td_name, op, tr->tr_d.td_name);

    /* Clean up child tree nodes (and their names) */
    db_free_tree(tl, resp);
    db_free_tree(tr, resp);

    /* Convert argument binary node into a result node */
    tp->tr_op = OP_DB_LEAF;
    tp->tr_d.td_name = name;
    return tp;
}


int
main(int argc, char *argv[])
{
    struct user_data {
	int info;
    } user_data;

    char idbuf[132];

    int arg_count;
    const int MIN_NUM_OF_ARGS = 2;

    bu_setprogname(argv[0]);
    bu_setlinebuf(stderr);

    rt_i *rtip;
    db_tree_state init_state;

    /* calculational tolerances mostly used by NMG routines */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    /* Defaults, updated by command line options. */
    ttol.abs = 0.0;
    ttol.rel = 0.01;
    ttol.norm = 0.0;

    /* parse command line arguments. */
    arg_count = parse_args(argc, argv);

    std::string output;
    if (!output_file) {
	output = argv[bu_optind];
	output += ".sat";
	output_file = output.c_str();
    }

    if ((argc - arg_count) < MIN_NUM_OF_ARGS) {
	usage("Error: Must specify model and objects on the command line\n");
    }

    // ***********************************************************************************
    // Setup Cubit Routines
    // ***********************************************************************************

    // CGM Initialization
    const char* ACIS_SAT = "ACIS_SAT";

    int dummy_argc = 0;
    char **dummy_argv =NULL;

    // Initialize the application
    AppUtil::instance()->startup(dummy_argc, dummy_argv);
    CGMApp::instance()->startup(dummy_argc, dummy_argv);

    CGMApp::instance()->attrib_manager()->auto_flag(true);

    // Create the geometry modify and query tool
    GeometryModifyTool *gmt = GeometryModifyTool::instance();
    GeometryQueryTool *gqt = GeometryQueryTool::instance();

    // Create the ACIS engines
    AcisQueryEngine::instance();
    AcisModifyEngine::instance();

    // Get version number of the geometry engine.
    CubitString version = gqt->get_engine_version_string();
    std::cout << "ACIS Engine: " << version << std::endl;

    /* Open BRL-CAD database */
    /* Scan all the records in the database and build a directory */
    rtip=rt_dirbuild(argv[bu_optind], idbuf, sizeof(idbuf));

    if (rtip == RTI_NULL) {
	usage("rt_dirbuild failure\n");
    }

    init_state = rt_initial_tree_state;
    init_state.ts_dbip = rtip->rti_dbip;
    init_state.ts_rtip = rtip;
    init_state.ts_resp = NULL;
    init_state.ts_tol = &tol;
    init_state.ts_ttol = &ttol;
    bu_avs_init(&init_state.ts_attrs, 1, "avs in tree_state");

    bu_optind++;

    /* Walk the trees named on the command line
     * outputting combinations and primitives
     */
    for (int i = bu_optind ; i < argc ; i++) {
	struct directory *dp;

	dp = db_lookup(rtip->rti_dbip, argv[i], LOOKUP_QUIET);
	if (dp == RT_DIR_NULL) {
	    bu_log("Cannot find %s\n", argv[i]);
	    continue;
	}

	db_walk_tree(rtip->rti_dbip, argc - i, (const char **)&argv[i], NUM_OF_CPUS_TO_USE,
		     &init_state , region_start, region_end, primitive_func, (void *) &user_data);
    }

    // *************************************************************************************
    // Write SAT file
    // *************************************************************************************

    // Make bodies list.
    Body* tool_body;

    // Export geometry
    if (g_body_cnt == 0) {
	usage("No geometry to convert.\n");
    }

    std::cout << "*** CSG DEBUG BEGIN ***" << std::endl;
    DLIList<Body*> all_region_bodies, region_bodies, from_bodies;

    for (unsigned int i=0; i < g_CsgBoolExp.size(); i++) {
	std::cout << " R" << i << " = " << g_CsgBoolExp[i] << ": " << get_body_id(g_CsgBoolExp[i]) << std::endl;

	int body_id = get_body_id(g_CsgBoolExp[i]);

	if (body_id >= 0) {
	    all_region_bodies.append(gqt->get_body(body_id));
	} else if (body_id == -1) {
	    // {empty}
	    std::cout << "DEBUG: {empty}" << std::endl;
	} else {
	    //tokenize
	    std::vector<std::string> csgTokens;
	    char csgOp;

	    tokenize(g_CsgBoolExp[i], csgTokens, " ");

	    std::cout << "DEBUG " << csgTokens.size() << std::endl;

	    for (unsigned int j = 0; j < csgTokens.size(); j++) {
		std::cout <<" T" << j << " = " << csgTokens[j] << ": " << get_body_id(csgTokens[j]) << std::endl;

		if (get_body_id(csgTokens[j]) >= 0) {
		    region_bodies.append(gqt->get_body(get_body_id(csgTokens[j])));
		} else {
		    csgOp = csgTokens[j].at(0);
		    std::cout << "*DEBUG* csgOp = " << csgOp << std::endl;
		    switch (csgOp) {
			case DB_OP_INTERSECT:
			    std::cout << "*** DEBUG INTERSECT ***" << std::endl;
			    tool_body = region_bodies.pop();
			    from_bodies.append(region_bodies.pop());
			    gmt->intersect(tool_body, from_bodies, region_bodies, CUBIT_TRUE);
			    break;
			case DB_OP_SUBTRACT:
			    std::cout << "*** DEBUG SUBTRACT ***" << std::endl;
			    tool_body = region_bodies.pop();
			    from_bodies.append(region_bodies.pop());
			    gmt->subtract(tool_body, from_bodies, region_bodies, CUBIT_TRUE);
			    break;
			case DB_OP_UNION:
			    std::cout << "*** DEBUG UNION ***" << std::endl;
			    if (region_bodies.size() >= 2) {
				from_bodies.append(region_bodies.pop());
				from_bodies.append(region_bodies.pop());
				if (!gmt->unite(from_bodies, region_bodies, CUBIT_TRUE)) {
				    region_bodies+=from_bodies;
				}
			    } else {
				region_bodies+=from_bodies;
			    }
			    break;
			default:
			    // do nothing -- should get here
			    break;
		    }

		    from_bodies.clean_out();
		} // end if on get_body_id

	    } // end for loop over csgTokens

	    csgTokens.clear();

	    if (region_bodies.size() != 0) {
		all_region_bodies+=region_bodies;
		region_bodies.clean_out();
	    }
	} // end if/else on body_id
    } // end for loop over g_CsgBoolExp

    std::cout << "*** CSG DEBUG END ***" << std::endl;

    // Make entities list.
    DLIList<RefEntity*> parent_entities;

    CAST_LIST_TO_PARENT(all_region_bodies, parent_entities);

    int size = parent_entities.size();
    std::cout << "Number of bodies to be exported: " << size << std::endl;

    // Export geometry
    if (size != 0) {
	(void)gqt->export_solid_model(parent_entities, output_file, ACIS_SAT, size, version);
    } else {
	usage("No geometry to convert.\n");
    }

    CGMApp::instance()->shutdown();

    std::cout << "Number of primitives processed: " << g_body_cnt << std::endl;

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
