/*                       D B _ C O M B . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2011 United States Government as represented by
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
/** @addtogroup db4 */
/** @{ */
/** @file db_comb.c
 *
 * This module contains the import/export routines for "Combinations",
 * the non-leaf nodes in the directed acyclic graphs (DAGs) in the
 * BRL-CAD ".g" database.
 *
 * This parallels the function of the geometry (leaf-node)
 * import/export routines found in the g_xxx.c routines.
 *
 * As a reminder, some combinations are special, when marked with the
 * "Region" flag, everything from that node down is considered to be
 * made of uniform material.
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "db.h"
#include "mater.h"
#include "raytrace.h"

#include "../librt_private.h"


#define STAT_ROT 1
#define STAT_XLATE 2
#define STAT_PERSP 4
#define STAT_SCALE 8


/**
 * D B _ C O M B _ M A T _ C A T E G O R I Z E
 *
 * Describe with a bit vector the effects this matrix will have.
 */
static int
db_comb_mat_categorize(const fastf_t *matp)
{
    int status = 0;

    if (!matp) return 0;

    if (!ZERO(matp[0] - 1.0)
	|| !ZERO(matp[5] - 1.0)
	|| !ZERO(matp[10] - 1.0))
    {
	status |= STAT_ROT;
    }

    if (!ZERO(matp[MDX])
	|| !ZERO(matp[MDY])
	|| !ZERO(matp[MDZ]))
    {
	status |= STAT_XLATE;
    }

    if (!ZERO(matp[12])
	|| !ZERO(matp[13])
	|| !ZERO(matp[14]))
    {
	status |= STAT_PERSP;
    }

    if (!ZERO(matp[15]))
	status |= STAT_SCALE;

    return status;
}


/**
 * D B _ T R E E _ N L E A V E S
 *
 * Return count of number of leaf nodes in this tree.
 */
size_t
db_tree_nleaves(const union tree *tp)
{
    if (tp == TREE_NULL) return 0;

    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_NOP:
	    return 0;
	case OP_DB_LEAF:
	    return 1;
	case OP_SOLID:
	    return 1;
	case OP_REGION:
	    return 1;

	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    /* Unary ops */
	    return db_tree_nleaves(tp->tr_b.tb_left);

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    /* This node is known to be a binary op */
	    return db_tree_nleaves(tp->tr_b.tb_left) +
		db_tree_nleaves(tp->tr_b.tb_right);

	default:
	    bu_log("db_tree_nleaves: bad op %d\n", tp->tr_op);
	    bu_bomb("db_tree_nleaves\n");
    }
    return 0;	/* for the compiler */
}


/**
 * D B _ F L A T T E N _ T R E E
 *
 * Take a binary tree in "V4-ready" layout (non-unions pushed below unions,
 * left-heavy), and flatten it into an array layout, ready for conversion
 * back to the GIFT-inspired V4 database format.
 *
 * This is done using the db_non_union_push() routine.
 *
 * If argument 'free' is non-zero, then
 * the non-leaf nodes are freed along the way, to prevent memory leaks.
 * In this case, the caller's copy of 'tp' will be invalid upon return.
 *
 * When invoked at the very top of the tree, the op argument must be OP_UNION.
 */
struct rt_tree_array *
db_flatten_tree(
    struct rt_tree_array *rt_tree_array,
    union tree *tp,
    int op,
    int freeflag,
    struct resource *resp)
{

    RT_CK_TREE(tp);
    RT_CK_RESOURCE(resp);

    switch (tp->tr_op) {
	case OP_DB_LEAF:
	    rt_tree_array->tl_op = op;
	    rt_tree_array->tl_tree = tp;
	    return rt_tree_array+1;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	    /* This node is known to be a binary op */
	    rt_tree_array = db_flatten_tree(rt_tree_array, tp->tr_b.tb_left, op, freeflag, resp);
	    rt_tree_array = db_flatten_tree(rt_tree_array, tp->tr_b.tb_right, tp->tr_op, freeflag, resp);
	    if (freeflag) {
		/* The leaves have been stolen, free the binary op */
		tp->tr_b.tb_left = TREE_NULL;
		tp->tr_b.tb_right = TREE_NULL;
		RT_FREE_TREE(tp, resp);
	    }
	    return rt_tree_array;

	default:
	    bu_log("db_flatten_tree: bad op %d\n", tp->tr_op);
	    bu_bomb("db_flatten_tree\n");
    }

    return (struct rt_tree_array *)NULL; /* for the compiler */
}


/**
 * R T _ C O M B _ I M P O R T 4
 *
 * Import a combination record from a V4 database into internal form.
 */
int
rt_comb_import4(
    struct rt_db_internal *ip,
    const struct bu_external *ep,
    const mat_t matrix,		/* NULL if identity */
    const struct db_i *dbip,
    struct resource *resp)
{
    union record *rp;
    struct rt_tree_array *rt_tree_array;
    union tree *tree;
    struct rt_comb_internal *comb;
    size_t j;
    size_t node_count;

    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;
    if (dbip) RT_CK_DBI(dbip);

    if (rp[0].u_id != ID_COMB) {
	bu_log("rt_comb_import4: Attempt to import a non-combination\n");
	return -1;
    }

    /* Compute how many granules of MEMBER records follow */
    node_count = ep->ext_nbytes/sizeof(union record) - 1;

    if (node_count)
	rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count, sizeof(struct rt_tree_array), "rt_tree_array");
    else
	rt_tree_array = (struct rt_tree_array *)NULL;

    for (j=0; j<node_count; j++) {
	if (rp[j+1].u_id != ID_MEMB) {
	    bu_free((genptr_t)rt_tree_array, "rt_comb_import4: rt_tree_array");
	    bu_log("rt_comb_import4(): granule in external buffer is not ID_MEMB, id=%d\n", rp[j+1].u_id);
	    return -1;
	}

	switch (rp[j+1].M.m_relation) {
	    case '+':
		rt_tree_array[j].tl_op = OP_INTERSECT;
		break;
	    case '-':
		rt_tree_array[j].tl_op = OP_SUBTRACT;
		break;
	    default:
		bu_log("rt_comb_import4() unknown op=x%x, assuming UNION\n", rp[j+1].M.m_relation);
		/* Fall through */
	    case 'u':
		rt_tree_array[j].tl_op = OP_UNION;
		break;
	}

	/* Build leaf node for in-memory tree */
	{
	    union tree *tp;
	    mat_t diskmat;
	    char namebuf[NAMESIZE+1];

	    RT_GET_TREE(tp, resp);
	    rt_tree_array[j].tl_tree = tp;
	    tp->tr_l.tl_op = OP_DB_LEAF;

	    /* bu_strlcpy not safe here, buffer size mismatch */
	    strncpy(namebuf, rp[j+1].M.m_instname, NAMESIZE);
	    namebuf[NAMESIZE] = '\0'; /* sanity */

	    tp->tr_l.tl_name = bu_strdup(namebuf);

	    flip_mat_dbmat(diskmat, rp[j+1].M.m_mat, dbip->dbi_version < 0 ? 1 : 0);

	    /* Verify that rotation part is pure rotation */
	    if (fabs(diskmat[0]) > 1 || fabs(diskmat[1]) > 1 ||
		fabs(diskmat[2]) > 1 ||
		fabs(diskmat[4]) > 1 || fabs(diskmat[5]) > 1 ||
		fabs(diskmat[6]) > 1 ||
		fabs(diskmat[8]) > 1 || fabs(diskmat[9]) > 1 ||
		fabs(diskmat[10]) > 1)
	    {
		bu_log("ERROR: %s/%s improper scaling, rotation matrix elements > 1\n",
		       rp[0].c.c_name, namebuf);
	    }

	    /* Verify that perspective isn't used as a modeling transform */
	    if (!ZERO(diskmat[12])
		|| !ZERO(diskmat[13])
		|| !ZERO(diskmat[14]))
	    {
		bu_log("ERROR: %s/%s has perspective transform\n", rp[0].c.c_name, namebuf);
	    }

	    /* See if disk record is identity matrix */
	    if (bn_mat_is_identity(diskmat)) {
		if (matrix == NULL) {
		    tp->tr_l.tl_mat = NULL;	/* identity */
		} else {
		    tp->tr_l.tl_mat = bn_mat_dup(matrix);
		}
	    } else {
		if (matrix == NULL) {
		    tp->tr_l.tl_mat = bn_mat_dup(diskmat);
		} else {
		    mat_t prod;
		    bn_mat_mul(prod, matrix, diskmat);
		    tp->tr_l.tl_mat = bn_mat_dup(prod);
		}
	    }
/* bu_log("M_name=%s, matp=x%x\n", tp->tr_l.tl_name, tp->tr_l.tl_mat); */
	}
    }
    if (node_count)
	tree = db_mkgift_tree(rt_tree_array, node_count, &rt_uniresource);
    else
	tree = (union tree *)NULL;

    RT_DB_INTERNAL_INIT(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_COMBINATION;
    ip->idb_meth = &rt_functab[ID_COMBINATION];

    BU_GET(comb, struct rt_comb_internal);
    RT_COMB_INTERNAL_INIT(comb);

    comb->tree = tree;

    ip->idb_ptr = (genptr_t)comb;

    switch (rp[0].c.c_flags) {
	case DBV4_NON_REGION_NULL:
	case DBV4_NON_REGION:
	    comb->region_flag = 0;
	    break;
	case DBV4_REGION:
	    comb->region_flag = 1;
	    comb->is_fastgen = REGION_NON_FASTGEN;
	    break;
	case DBV4_REGION_FASTGEN_PLATE:
	    comb->region_flag = 1;
	    comb->is_fastgen = REGION_FASTGEN_PLATE;
	    break;
	case DBV4_REGION_FASTGEN_VOLUME:
	    comb->region_flag = 1;
	    comb->is_fastgen = REGION_FASTGEN_VOLUME;
	    break;
	default:
	    bu_log("WARNING: combination %s has illegal c_flag=x%x\n",
		   rp[0].c.c_name, rp[0].c.c_flags);
	    break;
    }

    if (comb->region_flag) {
	if (dbip->dbi_version < 0) {
	    comb->region_id = flip_short(rp[0].c.c_regionid);
	    comb->aircode = flip_short(rp[0].c.c_aircode);
	    comb->GIFTmater = flip_short(rp[0].c.c_material);
	    comb->los = flip_short(rp[0].c.c_los);
	} else {
	    comb->region_id = rp[0].c.c_regionid;
	    comb->aircode = rp[0].c.c_aircode;
	    comb->GIFTmater = rp[0].c.c_material;
	    comb->los = rp[0].c.c_los;
	}
    } else {
	/* set some reasonable defaults */
	comb->region_id = 0;
	comb->aircode = 0;
	comb->GIFTmater = 0;
	comb->los = 0;
    }

    comb->rgb_valid = rp[0].c.c_override;
    if (comb->rgb_valid) {
	comb->rgb[0] = rp[0].c.c_rgb[0];
	comb->rgb[1] = rp[0].c.c_rgb[1];
	comb->rgb[2] = rp[0].c.c_rgb[2];
    }
    if (rp[0].c.c_matname[0] != '\0') {
	char shader_str[94];

	memset(shader_str, 0, 94);

	/* copy shader info to a static string */
	strncpy(shader_str,  rp[0].c.c_matname, 32);
	shader_str[32] = '\0'; /* c_matname is a buffer, bu_strlcpy not safe here */

	strcat(shader_str, " ");

	/* c_matparm is a buffer, bu_strlcpy not safe here */
	strncat(shader_str, rp[0].c.c_matparm, 32);

	/* convert to TCL format and place into comb->shader */
	if (bu_shader_to_tcl_list(shader_str, &comb->shader)) {
	    bu_log("rt_comb_import4: Error: Cannot convert following shader to TCL format:\n");
	    bu_log("\t%s\n", shader_str);
	    bu_vls_free(&comb->shader);
	    return -1;
	}
    }
    /* XXX Separate flags for color inherit, shader inherit, (new) material inherit? */
    /* XXX cf: ma_cinherit, ma_minherit */
    /* This ? is necessary to clean up old databases with grunge here */
    comb->inherit = (rp[0].c.c_inherit == DB_INH_HIGHER) ? 1 : 0;
    /* Automatic material table lookup here? */
    if (comb->region_flag)
	bu_vls_printf(&comb->material, "gift%d", comb->GIFTmater);

    if (rt_tree_array) bu_free((genptr_t)rt_tree_array, "rt_tree_array");

    return 0;
}


/**
 * R T _ C O M B _ E X P O R T 4
 */
int
rt_comb_export4(
    struct bu_external *ep,
    const struct rt_db_internal *ip,
    double UNUSED(local2mm),
    const struct db_i *dbip,
    struct resource *resp)
{
    struct rt_comb_internal *comb;
    size_t node_count;
    size_t actual_count;
    struct rt_tree_array *rt_tree_array;
    union tree *tp;
    union record *rp;
    size_t j;
    char *endp;
    struct bu_vls tmp_vls;

    RT_CK_DB_INTERNAL(ip);
    if (dbip) RT_CK_DBI(dbip);
    RT_CK_RESOURCE(resp);
    if (ip->idb_type != ID_COMBINATION) bu_bomb("rt_comb_export4() type not ID_COMBINATION");
    comb = (struct rt_comb_internal *)ip->idb_ptr;
    RT_CK_COMB(comb);

    if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
	db_non_union_push(comb->tree, resp);
	if (db_ck_v4gift_tree(comb->tree) < 0) {
	    /* Need to further modify tree */
	    bu_log("rt_comb_export4() Unable to V4-ify tree, aborting.\n");
	    rt_pr_tree(comb->tree, 0);
	    return -1;
	}
    }

    /* Count # leaves in tree -- that's how many Member records needed. */
    node_count = db_tree_nleaves(comb->tree);
    if (node_count > 0) {
	rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count, sizeof(struct rt_tree_array), "rt_tree_array");

	/* Convert tree into array form */
	actual_count = db_flatten_tree(rt_tree_array, comb->tree,
				       OP_UNION, 1, resp) - rt_tree_array;
	BU_ASSERT_SIZE_T(actual_count, ==, node_count);
	comb->tree = TREE_NULL;
    } else {
	rt_tree_array = (struct rt_tree_array *)NULL;
	actual_count = 0;
    }

    /* Reformat the data into the necessary V4 granules */
    BU_EXTERNAL_INIT(ep);
    ep->ext_nbytes = sizeof(union record) * (1 + node_count);
    ep->ext_buf = bu_calloc(1, ep->ext_nbytes, "v4 comb external");
    rp = (union record *)ep->ext_buf;

    /* Convert the member records */
    for (j = 0; j < node_count; j++) {
	tp = rt_tree_array[j].tl_tree;
	RT_CK_TREE(tp);
	if (tp->tr_op != OP_DB_LEAF) bu_bomb("rt_comb_export4() tree not OP_DB_LEAF");

	rp[j+1].u_id = ID_MEMB;
	switch (rt_tree_array[j].tl_op) {
	    case OP_INTERSECT:
		rp[j+1].M.m_relation = '+';
		break;
	    case OP_SUBTRACT:
		rp[j+1].M.m_relation = '-';
		break;
	    case OP_UNION:
		rp[j+1].M.m_relation = 'u';
		break;
	    default:
		bu_bomb("rt_comb_export4() corrupt rt_tree_array");
	}

	NAMEMOVE(tp->tr_l.tl_name, rp[j+1].M.m_instname);

	if (tp->tr_l.tl_mat) {
	    flip_dbmat_mat(rp[j+1].M.m_mat, tp->tr_l.tl_mat);
	} else {
	    flip_dbmat_mat(rp[j+1].M.m_mat, bn_mat_identity);
	}
	db_free_tree(tp, resp);
    }

    /* Build the Combination record, on the front */
    rp[0].u_id = ID_COMB;
    /* c_name[] filled in by db_wrap_v4_external() */
    if (comb->region_flag) {
	rp[0].c.c_regionid = (short)comb->region_id;
	rp[0].c.c_aircode = (short)comb->aircode;
	rp[0].c.c_material = (short)comb->GIFTmater;
	rp[0].c.c_los = (short)comb->los;
	switch (comb->is_fastgen) {
	    case REGION_FASTGEN_PLATE:
		rp[0].c.c_flags = DBV4_REGION_FASTGEN_PLATE;
		break;
	    case REGION_FASTGEN_VOLUME:
		rp[0].c.c_flags = DBV4_REGION_FASTGEN_VOLUME;
		break;
	    default:
	    case REGION_NON_FASTGEN:
		rp[0].c.c_flags = DBV4_REGION;
		break;
	}
    } else {
	rp[0].c.c_flags = DBV4_NON_REGION;
    }
    if (comb->rgb_valid) {
	rp[0].c.c_override = 1;
	rp[0].c.c_rgb[0] = comb->rgb[0];
	rp[0].c.c_rgb[1] = comb->rgb[1];
	rp[0].c.c_rgb[2] = comb->rgb[2];
    }

    bu_vls_init(&tmp_vls);

    /* convert TCL list format shader to keyword=value format */
    if (bu_shader_to_key_eq(bu_vls_addr(&comb->shader), &tmp_vls)) {

	bu_log("rt_comb_export4: Cannot convert following shader string to keyword=value format:\n");
	bu_log("\t%s\n", bu_vls_addr(&comb->shader));
	rp[0].c.c_matparm[0] = '\0';
	rp[0].c.c_matname[0] = '\0';
	return -1;
    } else {
	endp = strchr(bu_vls_addr(&tmp_vls), ' ');
	if (endp) {
	    size_t len;
	    len = endp - bu_vls_addr(&tmp_vls);
	    if (len <= 0 && bu_vls_strlen(&tmp_vls) > 0) {
		bu_log("WARNING: leading spaces on shader '%s' implies NULL shader\n",
		       bu_vls_addr(&tmp_vls));
	    }

	    if (len >= sizeof(rp[0].c.c_matname)) {
		bu_log("ERROR:  Shader name '%s' exceeds v4 database field, aborting.\n",
		       bu_vls_addr(&tmp_vls));
		return -1;
	    }
	    if (strlen(endp+1) >= sizeof(rp[0].c.c_matparm)) {
		bu_log("ERROR:  Shader parameters '%s' exceed database field, aborting.\nUse \"dbupgrade\" to enable unlimited length strings.\n",
		       endp+1);
		return -1;
	    }

	    /* stash as string even though c_matname/parm are NAMESIZE buffers */
	    bu_strlcpy(rp[0].c.c_matname, bu_vls_addr(&tmp_vls), sizeof(rp[0].c.c_matname));
	    bu_strlcpy(rp[0].c.c_matparm, endp+1, sizeof(rp[0].c.c_matparm));
	} else {
	    if ((size_t)bu_vls_strlen(&tmp_vls) >= sizeof(rp[0].c.c_matname)) {
		bu_log("ERROR:  Shader name '%s' exceeds v4 database field, aborting.\n",
		       bu_vls_addr(&tmp_vls));
		return -1;
	    }
	    bu_strlcpy(rp[0].c.c_matname, bu_vls_addr(&tmp_vls), sizeof(rp[0].c.c_matname));
	    rp[0].c.c_matparm[0] = '\0';
	}
    }
    bu_vls_free(&tmp_vls);

    rp[0].c.c_inherit = comb->inherit;

    if (rt_tree_array) bu_free((char *)rt_tree_array, "rt_tree_array");

    return 0;		/* OK */
}


/**
 * D B _ T R E E _ F L A T T E N _ D E S C R I B E
 *
 * Produce a GIFT-compatible listing, one "member" per line,
 * regardless of the structure of the tree we've been given.
 */
void
db_tree_flatten_describe(
    struct bu_vls *vls,
    const union tree *tp,
    int indented,
    int lvl,
    double mm2local,
    struct resource *resp)
{
    size_t i;
    size_t node_count;
    struct rt_tree_array *rt_tree_array;
    char op = OP_NOP;
    int status;
    union tree *ntp;

    BU_CK_VLS(vls);
    RT_CK_RESOURCE(resp);

    if (!tp) {
	/* no tree, probably an empty combination */
	bu_vls_strcat(vls, "-empty-\n");
	return;
    }
    RT_CK_TREE(tp);

    node_count = db_tree_nleaves(tp);
    if (node_count == 0) {
	if (!indented) bu_vls_spaces(vls, 2*lvl);
	bu_vls_strcat(vls, "-empty-\n");
	return;
    }

    /*
     * We're going to whack the heck out of the tree, but our
     * argument is 'const'.  Before getting started, make a
     * private copy just for us.
     */
    ntp = db_dup_subtree(tp, resp);
    RT_CK_TREE(ntp);

    /* Convert to "v4 / GIFT style", so that the flatten makes sense. */
    if (db_ck_v4gift_tree(ntp) < 0)
	db_non_union_push(ntp, resp);
    RT_CK_TREE(ntp);

    node_count = db_tree_nleaves(ntp);
    rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count, sizeof(struct rt_tree_array), "rt_tree_array");

    /*
     * free=0 means that the tree won't have any leaf nodes freed.
     */
    (void)db_flatten_tree(rt_tree_array, ntp, OP_UNION, 0, resp);

    for (i=0; i<node_count; i++) {
	union tree *itp = rt_tree_array[i].tl_tree;

	RT_CK_TREE(itp);
	BU_ASSERT_LONG(itp->tr_op, ==, OP_DB_LEAF);
	BU_ASSERT_PTR(itp->tr_l.tl_name, !=, NULL);

	switch (rt_tree_array[i].tl_op) {
	    case OP_INTERSECT:
		op = '+';
		break;
	    case OP_SUBTRACT:
		op = '-';
		break;
	    case OP_UNION:
		op = 'u';
		break;
	    default:
		bu_bomb("db_tree_flatten_describe() corrupt rt_tree_array");
	}

	status = db_comb_mat_categorize(itp->tr_l.tl_mat);
	if (!indented) bu_vls_spaces(vls, 2*lvl);
	bu_vls_printf(vls, " %c %s", op, itp->tr_l.tl_name);
	if (status & STAT_ROT) {
	    fastf_t az, el;
	    bn_ae_vec(&az, &el, itp->tr_l.tl_mat ?
		      itp->tr_l.tl_mat : bn_mat_identity);
	    bu_vls_printf(vls,
			  " az=%g, el=%g, ",
			  az, el);
	}
	if (status & STAT_XLATE) {
	    bu_vls_printf(vls, " [%g, %g, %g]",
			  itp->tr_l.tl_mat[MDX]*mm2local,
			  itp->tr_l.tl_mat[MDY]*mm2local,
			  itp->tr_l.tl_mat[MDZ]*mm2local);
	}
	if (status & STAT_SCALE) {
	    bu_vls_printf(vls, " scale %g",
			  1.0/itp->tr_l.tl_mat[15]);
	}
	if (status & STAT_PERSP) {
	    bu_vls_printf(vls,
			  " Perspective=[%g, %g, %g]??",
			  itp->tr_l.tl_mat[12],
			  itp->tr_l.tl_mat[13],
			  itp->tr_l.tl_mat[14]);
	}
	bu_vls_printf(vls, "\n");
    }

    if (rt_tree_array) bu_free((genptr_t)rt_tree_array, "rt_tree_array");
    db_free_tree(ntp, resp);
}


/**
 * D B _ T R E E _ D E S C R I B E
 */
void
db_tree_describe(
    struct bu_vls *vls,
    const union tree *tp,
    int indented,
    int lvl,
    double mm2local)
{
    int status;

    BU_CK_VLS(vls);

    if (!tp) {
	/* no tree, probably an empty combination */
	bu_vls_strcat(vls, "-empty-\n");
	return;
    }
    RT_CK_TREE(tp);
    switch (tp->tr_op) {

	case OP_DB_LEAF:
	    status = db_comb_mat_categorize(tp->tr_l.tl_mat);

	    /* One per line, out onto the vls */
	    if (!indented) bu_vls_spaces(vls, 2*lvl);
	    bu_vls_strcat(vls, tp->tr_l.tl_name);
	    if (status & STAT_ROT) {
		fastf_t az, el;
		bn_ae_vec(&az, &el, tp->tr_l.tl_mat ?
			  tp->tr_l.tl_mat : bn_mat_identity);
		bu_vls_printf(vls,
			      " az=%g, el=%g, ",
			      az, el);
	    }
	    if (status & STAT_XLATE) {
		bu_vls_printf(vls, " [%g, %g, %g]",
			      tp->tr_l.tl_mat[MDX]*mm2local,
			      tp->tr_l.tl_mat[MDY]*mm2local,
			      tp->tr_l.tl_mat[MDZ]*mm2local);
	    }
	    if (status & STAT_SCALE) {
		bu_vls_printf(vls, " scale %g",
			      1.0/tp->tr_l.tl_mat[15]);
	    }
	    if (status & STAT_PERSP) {
		bu_vls_printf(vls,
			      " Perspective=[%g, %g, %g]??",
			      tp->tr_l.tl_mat[12],
			      tp->tr_l.tl_mat[13],
			      tp->tr_l.tl_mat[14]);
	    }
	    bu_vls_printf(vls, "\n");
	    return;

	    /* This node is known to be a binary op */
	case OP_UNION:
	    if (!indented) bu_vls_spaces(vls, 2*lvl);
	    bu_vls_strcat(vls, "u ");
	    goto bin;
	case OP_INTERSECT:
	    if (!indented) bu_vls_spaces(vls, 2*lvl);
	    bu_vls_strcat(vls, "+ ");
	    goto bin;
	case OP_SUBTRACT:
	    if (!indented) bu_vls_spaces(vls, 2*lvl);
	    bu_vls_strcat(vls, "- ");
	    goto bin;
	case OP_XOR:
	    if (!indented) bu_vls_spaces(vls, 2*lvl);
	    bu_vls_strcat(vls, "^ ");
	bin:
	    db_tree_describe(vls, tp->tr_b.tb_left, 1, lvl+1, mm2local);
	    db_tree_describe(vls, tp->tr_b.tb_right, 0, lvl+1, mm2local);
	    return;

	    /* This node is known to be a unary op */
	case OP_NOT:
	    if (!indented) bu_vls_spaces(vls, 2*lvl);
	    bu_vls_strcat(vls, "! ");
	    goto unary;
	case OP_GUARD:
	    if (!indented) bu_vls_spaces(vls, 2*lvl);
	    bu_vls_strcat(vls, "G ");
	    goto unary;
	case OP_XNOP:
	    if (!indented) bu_vls_spaces(vls, 2*lvl);
	    bu_vls_strcat(vls, "X ");
	unary:
	    db_tree_describe(vls, tp->tr_b.tb_left, 1, lvl+1, mm2local);
	    return;

	case OP_NOP:
	    if (!indented) bu_vls_spaces(vls, 2*lvl);
	    bu_vls_strcat(vls, "NOP\n");
	    return;

	default:
	    bu_log("db_tree_describe: bad op %d\n", tp->tr_op);
	    bu_bomb("db_tree_describe\n");
    }
}


/**
 * D B _ C O M B _ D E S C R I B E
 */
void
db_comb_describe(
    struct bu_vls *str,
    const struct rt_comb_internal *comb,
    int verbose,
    double mm2local,
    struct resource *resp)
{
    RT_CK_COMB(comb);
    RT_CK_RESOURCE(resp);

    if (comb->region_flag) {
	bu_vls_printf(str,
		      "REGION id=%d (air=%d, los=%d, GIFTmater=%d) ",
		      comb->region_id,
		      comb->aircode,
		      comb->los,
		      comb->GIFTmater);

	if (comb->is_fastgen == REGION_FASTGEN_PLATE)
	    bu_vls_printf(str, "(FASTGEN plate mode) ");
	else if (comb->is_fastgen == REGION_FASTGEN_VOLUME)
	    bu_vls_printf(str, "(FASTGEN volume mode) ");
    }

    bu_vls_strcat(str, "--\n");
    if (bu_vls_strlen(&comb->shader) > 0) {
	bu_vls_printf(str,
		      "Shader '%s'\n",
		      bu_vls_addr(&comb->shader));
    }

    if (comb->rgb_valid) {
	bu_vls_printf(str,
		      "Color %d %d %d\n",
		      comb->rgb[0],
		      comb->rgb[1],
		      comb->rgb[2]);
    }

    if (bu_vls_strlen(&comb->shader) > 0 || comb->rgb_valid) {
	if (comb->inherit) {
	    bu_vls_strcat(str,
			  "(These material properties override all lower ones in the tree)\n");
	}
    }

    if (comb->tree) {
	if (verbose) {
	    db_tree_flatten_describe(str, comb->tree, 0, 1, mm2local, resp);
	} else {
	    rt_pr_tree_vls(str, comb->tree);
	}
    } else {
	bu_vls_strcat(str, "(empty tree)\n");
    }
}


/**
 * R T _ C O M B _ I F R E E
 *
 * Free the storage associated with the rt_db_internal version of this combination.
 */
void
rt_comb_ifree(struct rt_db_internal *ip)
{
    register struct rt_comb_internal *comb;

    RT_CK_DB_INTERNAL(ip);
    comb = (struct rt_comb_internal *)ip->idb_ptr;

    if (comb) {
	/* If tree hasn't been stolen, release it */
	db_free_tree(comb->tree, &rt_uniresource);
	RT_FREE_COMB_INTERNAL(comb);
	bu_free((genptr_t)comb, "comb ifree");
    }
    ip->idb_ptr = GENPTR_NULL;	/* sanity */
}


/**
 * R T _ C O M B _ D E S C R I B E
 *
 * rt_functab[ID_COMBINATION].ft_describe() method
 */
int
rt_comb_describe(
    struct bu_vls *str,
    const struct rt_db_internal *ip,
    int verbose,
    double mm2local,
    struct resource *resp,
    struct db_i *dbip)
{
    const struct rt_comb_internal *comb;

    RT_CK_DB_INTERNAL(ip);
    RT_CK_RESOURCE(resp);
    if (dbip) RT_CK_DBI(dbip);

    comb = (struct rt_comb_internal *)ip->idb_ptr;
    RT_CK_COMB(comb);

    db_comb_describe(str, comb, verbose, mm2local, resp);
    return 0;
}


/*==================== END g_comb.c / table.c interface ========== */

/**
 * D B _ W R A P _ V 4 _ E X T E R N A L
 *
 * As the v4 database does not really have the notion of "wrapping",
 * this function writes the object name into the
 * proper place (a standard location in all granules).
 */
void
db_wrap_v4_external(struct bu_external *op, const char *name)
{
    union record *rec;

    BU_CK_EXTERNAL(op);

    rec = (union record *)op->ext_buf;
    NAMEMOVE(name, rec->s.s_name);
}


/* Some export support routines */

/**
 * D B _ C K _ L E F T _ H E A V Y _ T R E E
 *
 * Support routine for db_ck_v4gift_tree().
 * Ensure that the tree below 'tp' is left-heavy, i.e. that there are
 * nothing but solids on the right side of any binary operations.
 *
 * Returns -
 * -1 ERROR
 * 0 OK
 */
int
db_ck_left_heavy_tree(
    const union tree *tp,
    int no_unions)
{
    RT_CK_TREE(tp);
    switch (tp->tr_op) {

	case OP_DB_LEAF:
	    break;

	case OP_UNION:
	    if (no_unions) return -1;
	    /* else fall through */
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    if (db_ck_left_heavy_tree(tp->tr_b.tb_right, no_unions) < 0)
		return -1;
	    return db_ck_left_heavy_tree(tp->tr_b.tb_left, no_unions);

	default:
	    bu_log("db_ck_left_heavy_tree: bad op %d\n", tp->tr_op);
	    bu_bomb("db_ck_left_heavy_tree\n");
    }
    return 0;
}


/**
 * D B _ C K _ V 4 G I F T _ T R E E
 *
 * Look a gift-tree in the mouth.
 * Ensure that this boolean tree conforms to the GIFT convention that
 * union operations must bind the loosest.
 * There are two stages to this check:
 * 1) Ensure that if unions are present they are all at the root of tree,
 * 2) Ensure non-union children of union nodes are all left-heavy
 * (nothing but solid nodes permitted on rhs of binary operators).
 *
 * Returns -
 * -1 ERROR
 * 0 OK
 */
int
db_ck_v4gift_tree(const union tree *tp)
{
    RT_CK_TREE(tp);
    switch (tp->tr_op) {

	case OP_DB_LEAF:
	    break;

	case OP_UNION:
	    if (db_ck_v4gift_tree(tp->tr_b.tb_left) < 0)
		return -1;
	    return db_ck_v4gift_tree(tp->tr_b.tb_right);

	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    return db_ck_left_heavy_tree(tp, 1);

	default:
	    bu_log("db_ck_v4gift_tree: bad op %d\n", tp->tr_op);
	    bu_bomb("db_ck_v4gift_tree\n");
    }
    return 0;
}


/**
 * D B _ M K B O O L _ T R E E
 *
 * Given a rt_tree_array array, build a tree of "union tree" nodes
 * appropriately connected together.  Every element of the
 * rt_tree_array array used is replaced with a TREE_NULL.
 * Elements which are already TREE_NULL are ignored.
 * Returns a pointer to the top of the tree.
 */
union tree *
db_mkbool_tree(
    struct rt_tree_array *rt_tree_array,
    size_t howfar,
    struct resource *resp)
{
    register struct rt_tree_array *tlp;
    size_t i;
    register struct rt_tree_array *first_tlp = (struct rt_tree_array *)0;
    register union tree *xtp;
    register union tree *curtree;
    register int inuse;

    RT_CK_RESOURCE(resp);

    if (howfar == 0)
	return TREE_NULL;

    /* Count number of non-null sub-trees to do */
    for (i=howfar, inuse=0, tlp=rt_tree_array; i>0; i--, tlp++) {
	if (tlp->tl_tree == TREE_NULL)
	    continue;
	if (inuse++ == 0)
	    first_tlp = tlp;
    }

    /* Handle trivial cases */
    if (inuse <= 0)
	return TREE_NULL;
    if (inuse == 1) {
	curtree = first_tlp->tl_tree;
	first_tlp->tl_tree = TREE_NULL;
	return curtree;
    }

    if (first_tlp->tl_op != OP_UNION) {
	first_tlp->tl_op = OP_UNION;	/* Fix it */
	if (RT_G_DEBUG & DEBUG_TREEWALK) {
	    bu_log("db_mkbool_tree() WARNING: non-union (%c) first operation ignored\n",
		   first_tlp->tl_op);
	}
    }

    curtree = first_tlp->tl_tree;
    first_tlp->tl_tree = TREE_NULL;
    tlp=first_tlp+1;
    for (i=howfar-(tlp-rt_tree_array); i>0; i--, tlp++) {
	if (tlp->tl_tree == TREE_NULL)
	    continue;

	RT_GET_TREE(xtp, resp);
	xtp->tr_b.tb_left = curtree;
	xtp->tr_b.tb_right = tlp->tl_tree;
	xtp->tr_b.tb_regionp = (struct region *)0;
	xtp->tr_op = tlp->tl_op;
	curtree = xtp;
	tlp->tl_tree = TREE_NULL;	/* empty the input slot */
    }
    return curtree;
}


/**
 * D B _ M K G I F T _ T R E E
 */
union tree *
db_mkgift_tree(struct rt_tree_array *trees, size_t subtreecount, struct resource *resp)
{
    struct rt_tree_array *tstart;
    struct rt_tree_array *tnext;
    union tree *curtree;
    long i;
    long j;
    long treecount;

    RT_CK_RESOURCE(resp);

    BU_ASSERT_SIZE_T(subtreecount, <, LONG_MAX);
    treecount = (long)subtreecount;

    /*
     * This is how GIFT interpreted equations, so it is duplicated here.
     * Any expressions between UNIONs are evaluated first.  For example:
     * A - B - C u D - E - F
     * becomes (A - B - C) u (D - E - F)
     * so first do the parenthesised parts, and then go
     * back and glue the unions together.
     * As always, unions are the downfall of free enterprise!
     */
    tstart = trees;
    tnext = trees+1;
    for (i=treecount-1; i>=0; i--, tnext++) {
	/* If we went off end, or hit a union, do it */

	if (i>0 && tnext->tl_op != OP_UNION)
	    continue;

	j = tnext-tstart;
	if (j <= 0)
	    continue;

	curtree = db_mkbool_tree(tstart, (size_t)j, resp);
	/* db_mkbool_tree() has side effect of zapping tree array,
	 * so build new first node in array.
	 */
	tstart->tl_op = OP_UNION;
	tstart->tl_tree = curtree;

	if (RT_G_DEBUG&DEBUG_TREEWALK) {
	    bu_log("db_mkgift_tree() intermediate term:\n");
	    rt_pr_tree(tstart->tl_tree, 0);
	}

	/* tstart here at union */
	tstart = tnext;
    }

    curtree = db_mkbool_tree(trees, subtreecount, resp);
    if (RT_G_DEBUG&DEBUG_TREEWALK) {
	bu_log("db_mkgift_tree() returns:\n");
	rt_pr_tree(curtree, 0);
    }
    return curtree;
}


/**
 * r t _ c o m b _ g e t _ c o l o r
 *
 * fills in rgb with the color for a given comb combination
 *
 * returns truthfully if a color could be got
 */
int
rt_comb_get_color(unsigned char rgb[3], const struct rt_comb_internal *comb)
{
    struct mater *mp = MATER_NULL;

    if (!comb)
	return 0;

    RT_CK_COMB(comb);

    if (comb->rgb_valid) {
	rgb[0] = comb->rgb[0];
	rgb[1] = comb->rgb[1];
	rgb[2] = comb->rgb[2];
	return 1;
    }

    for (mp = rt_material_head(); mp != MATER_NULL; mp = mp->mt_forw) {
	if (comb->region_id <= mp->mt_high && comb->region_id >= mp->mt_low) {
	    rgb[0] = mp->mt_r;
	    rgb[1] = mp->mt_g;
	    rgb[2] = mp->mt_b;
	    return 1;
	}
    }

    /* fail */
    return 0;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
