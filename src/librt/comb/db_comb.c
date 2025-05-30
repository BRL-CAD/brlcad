/*                       D B _ C O M B . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2025 United States Government as represented by
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

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include "bio.h"


#include "vmath.h"
#include "bn.h"
#include "rt/db4.h"
#include "raytrace.h"

#include "../librt_private.h"


#define STAT_ROT 1
#define STAT_XLATE 2
#define STAT_PERSP 4
#define STAT_SCALE 8

int
rt_shader_to_list(const char *in, struct bu_vls *vls)
{
    size_t len;
    size_t shader_name_len = 0;
    char *iptr;
    const char *shader;
    char *copy;
    char *next;

    if (UNLIKELY(in == NULL))
	in = "";

    copy = bu_strdup(in);
    next = copy;

    BU_CK_VLS(vls);

    while (next) {
	iptr = next;

	/* find start of shader name */
	while (isspace((int)(*iptr)))
	    iptr++;

	shader = iptr;

	/* find end of shader name */
	while (*iptr && !isspace((int)(*iptr)) && *iptr != ';')
	    iptr++;

	shader_name_len = iptr - shader;

	if (shader_name_len == 5 && !bu_strncasecmp(shader, "stack", 5)) {

	    /* stack shader, loop through all shaders in stack */
	    int done = 0;

	    bu_vls_strcat(vls, "stack {");

	    while (!done) {
		const char *shade1;

		/* find start of shader */
		while (isspace((int)(*iptr)))
		    iptr++;

		if (*iptr == '\0')
		    break;

		shade1 = iptr;

		/* find end of shader */
		while (*iptr && *iptr != ';')
		    iptr++;

		if (*iptr == '\0')
		    done = 1;

		*iptr = '\0';

		bu_vls_putc(vls, '{');

		if (rt_shader_to_list(shade1, vls)) {
		    bu_free(copy, CPP_FILELINE);
		    return 1;
		}

		bu_vls_strcat(vls, "} ");

		if (!done)
		    iptr++;
	    }
	    bu_vls_putc(vls, '}');
	    bu_free(copy, CPP_FILELINE);
	    return 0;
	}

	if (shader_name_len == 6 && !bu_strncasecmp(shader, "envmap", 6)) {
	    bu_vls_strcat(vls, "envmap {");
	    if (rt_shader_to_list(iptr, vls)) {
		bu_free(copy, CPP_FILELINE);
		return 1;
	    }
	    bu_vls_putc(vls, '}');
	    bu_free(copy, CPP_FILELINE);
	    return 0;
	}

	bu_vls_strncat(vls, shader, shader_name_len);

	/* skip more white space */
	while (*iptr && isspace((int)(*iptr)))
	    iptr++;

	/* iptr now points at start of parameters, if any */
	if (*iptr && *iptr != ';') {
	    int needClosingBrace = 0;

	    bu_vls_strcat(vls, " {");

	    if (*iptr == '{') {
		/* if parameter set begins with open brace then
		 * it should already have a closing brace
		 */
		iptr++;
	    } else {
		/* otherwise we'll need to add it */
		needClosingBrace = 1;
	    }

	    /* append next set of parameters (if any) to vls */
	    len = bu_vls_strlen(vls);
	    if (bu_key_eq_to_key_val(iptr, (const char **)&next, vls)) {
		bu_free(copy, CPP_FILELINE);
		return 1;
	    }

	    if (needClosingBrace) {
		/* Add closing brace unless we didn't actually append any
		 * parameters, in which case we need to delete the " {"
		 * appended earlier.
		 */
		if (bu_vls_strlen(vls) > len) {
		    bu_vls_putc(vls, '}');
		} else {
		    bu_vls_trunc(vls, (int)len - 2);
		}
	    }
	} else if (*iptr && *iptr == ';') {
	    next = ++iptr;
	} else {
	    next = (char *)NULL;
	}
    }

    bu_free(copy, CPP_FILELINE);
    return 0;
}


/**
 * Given a list in "{1}, {2}, {3}" form, return a copy of the
 * 'index'th entry, which may itself be a list.
 *
 * Note: caller is responsible for freeing the returned string.
 */
static char *
parse_list_elem(const char *in, int idx)
{
    int depth = 0;
    int count = 0;
    int len = 0;
    const char *ptr = in;
    const char *prev = NULL;
    const char *start = NULL;
    const char *end = NULL;

    struct bu_vls out = BU_VLS_INIT_ZERO;
    char *ret = NULL;

    while (ptr && *ptr) {
	/* skip leading white space */
	while (*ptr && isspace((int)(*ptr))) {
	    prev = ptr;
	    ptr++;
	}

	if (!*ptr)
	    break;

	if (depth == 0 && count == idx)
	    start = ptr;

	if (*ptr == '{') {
	    depth++;
	    prev = ptr;
	    ptr++;
	} else if (*ptr == '}') {
	    depth--;
	    if (depth == 0)
		count++;
	    if (start && depth == 0) {
		end = ptr;
		break;
	    }
	    prev = ptr;
	    ptr++;
	} else {
	    while (*ptr &&
		   (!isspace((int)(*ptr)) || (prev && *prev == '\\')) &&
		   (*ptr != '}' || (prev && *prev == '\\')) &&
		   (*ptr != '{' || (prev && *prev == '\\')))
	    {
		prev = ptr;
		ptr++;
	    }
	    if (depth == 0)
		count++;

	    if (start && depth == 0) {
		end = ptr-1;
		break;
	    }
	}
    }

    if (!start)
	return (char *)NULL;

    if (*start == '{') {
	if (!end || *end != '}') {
	    bu_log("Error in list (uneven braces?): %s\n", in);
	    return (char *)NULL;
	}

	/* remove enclosing braces */
	start++;
	while (start < end && isspace((int)(*start)))
	    start++;

	end--;
	while (end > start && isspace((int)(*end)) && *(end-1) != '\\')
	    end--;

	if (start == end)
	    return (char *)NULL;
    }

    len = end - start + 1;
    bu_vls_strncpy(&out, start, len);

    ret = bu_vls_strdup(&out);
    bu_vls_free(&out);

    return ret;
}

/**
 * Return number of items in a string, interpreted as a Tcl list.
 */
static int
parse_list_length(const char *in)
{
    int count = 0;
    int depth = 0;
    const char *ptr = in;
    const char *prev = NULL;

    if (UNLIKELY(in == NULL))
	return 0;

    while (ptr && *ptr) {
	/* skip leading white space */
	while (*ptr && isspace((int)(*ptr))) {
	    prev = ptr;
	    ptr++;
	}

	if (!*ptr)
	    break;

	if (*ptr == '{') {
	    if (depth == 0)
		count++;
	    depth++;
	    prev = ptr;
	    ptr++;
	} else if (*ptr == '}') {
	    depth--;
	    prev = ptr;
	    ptr++;
	} else {
	    if (depth == 0)
		count++;

	    while (*ptr &&
		   (!isspace((int)(*ptr)) || (prev && *prev == '\\')) &&
		   (*ptr != '}' || (prev && *prev == '\\')) &&
		   (*ptr != '{' || (prev && *prev == '\\')))
	    {
		prev = ptr;
		ptr++;
	    }
	}
    }

    return count;
}


static int
parse_key_val_to_vls(struct bu_vls *vls, char *params)
{
    int len;
    int j;

    if (UNLIKELY(!params || strlen(params) == 0))
	return 0;

    len = parse_list_length(params);

    if (len == 0) {
	return 0;
    } else if (len == 1) {
	bu_vls_putc(vls, ' ');
	bu_vls_strcat(vls, params);
	return 0;
    } else if (len%2) {
	bu_log("ERROR: shader parameters must be even numbered! (key value pairings)\n\t%s\n", params);
	return 1;
    }

    for (j = 0; j < len; j += 2) {
	char *keyword;
	char *value;

	keyword = parse_list_elem(params, j);
	if (!keyword)
	    continue;
	value = parse_list_elem(params, j+1);
	if (!value) {
	    bu_free(keyword, "parse_key_val_to_vls() keyword");
	    continue;
	}

	bu_vls_putc(vls, ' ');
	bu_vls_strcat(vls, keyword);
	bu_vls_putc(vls, '=');
	if (parse_list_length(value) > 1) {
	    bu_vls_putc(vls, '"');
	    bu_vls_strcat(vls, value);
	    bu_vls_putc(vls, '"');
	} else {
	    bu_vls_strcat(vls, value);
	}

	bu_free(keyword, "parse_key_val_to_vls() keyword");
	bu_free(value, "parse_key_val_to_vls() value");

    }
    return 0;
}




int
rt_shader_to_key_eq(const char *in, struct bu_vls *vls)
{
    int len;
    int ret = 0;
    char *shader;
    char *params;

    BU_CK_VLS(vls);

    if (UNLIKELY(in == NULL))
	return 0;

    len = parse_list_length(in);

    if (len == 0) {
	return 0;
    } else if (len == 1) {
	/* shader with no parameters */
	if (bu_vls_strlen(vls))
	    bu_vls_putc(vls, ' ');
	bu_vls_strcat(vls, in);
	return 0;
    } else if (len != 2) {
	bu_log("ERROR: expecting exactly two shader parameters (not %d)!!\n\t%s\n", len, in);
	return 1;
    }

    shader = parse_list_elem(in, 0);
    if (!shader) {
	bu_log("ERROR: failed to parse valid shader name\n");
	return 1;
    }
    params = parse_list_elem(in, 1);
    if (!params) {
	bu_free(shader, "shader");
	bu_log("ERROR: failed to parse valid shader parameters\n");
	return 1;
    }

    /* FIXME: should not be aware of specific shader names here.
     * breaks encapsulation and just sucks.
     */
    if (BU_STR_EQUAL(shader, "envmap")) {
	/* environment map */

	if (bu_vls_strlen(vls))
	    bu_vls_putc(vls, ' ');
	bu_vls_strcat(vls, "envmap");

	rt_shader_to_key_eq(params, vls);
    } else if (BU_STR_EQUAL(shader, "stack")) {
	/* stacked shaders */

	int i;

	if (bu_vls_strlen(vls))
	    bu_vls_putc(vls, ' ');
	bu_vls_strcat(vls, "stack");

	/* get number of shaders in the stack */
	len = parse_list_length(params);

	/* process each shader in the stack */
	for (i = 0; i < len; i++) {
	    char *shader1;

	    /* each parameter must be a shader specification in itself */
	    shader1 = parse_list_elem(params, i);

	    if (i > 0)
		bu_vls_putc(vls, ';');
	    rt_shader_to_key_eq(shader1, vls);
	    bu_free(shader1, "shader1");
	}
    } else {
	if (bu_vls_strlen(vls))
	    bu_vls_putc(vls, ' ');
	bu_vls_strcat(vls, shader);
	ret = parse_key_val_to_vls(vls, params);
    }

    bu_free(shader, "shader");
    bu_free(params, "params");

    return ret;
}




/**
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
		BU_PUT(tp, union tree);
	    }
	    return rt_tree_array;

	default:
	    bu_log("db_flatten_tree: bad op %d\n", tp->tr_op);
	    bu_bomb("db_flatten_tree\n");
    }

    return (struct rt_tree_array *)NULL; /* for the compiler */
}


int
rt_comb_import4(
    struct rt_db_internal *ip,
    const struct bu_external *ep,
    const mat_t matrix,		/* NULL if identity */
    const struct db_i *dbip,
    struct resource *UNUSED(resp))
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

    for (j = 0; j < node_count; j++) {
	if (rp[j+1].u_id != ID_MEMB) {
	    bu_free((void *)rt_tree_array, "rt_comb_import4: rt_tree_array");
	    bu_log("rt_comb_import4(): granule in external buffer is not ID_MEMB, id=%d\n", rp[j+1].u_id);
	    return -1;
	}

	switch (rp[j+1].M.m_relation) {
	    case DB_OP_INTERSECT:
		rt_tree_array[j].tl_op = OP_INTERSECT;
		break;
	    case DB_OP_SUBTRACT:
		rt_tree_array[j].tl_op = OP_SUBTRACT;
		break;
	    default:
		bu_log("rt_comb_import4() unknown op=x%x, assuming UNION\n", rp[j+1].M.m_relation);
		/* Fall through */
	    case DB_OP_UNION:
		rt_tree_array[j].tl_op = OP_UNION;
		break;
	}

	/* Build leaf node for in-memory tree */
	{
	    union tree *tp;
	    mat_t diskmat;
	    char namebuf[NAMESIZE+1];

	    BU_GET(tp, union tree);
	    RT_TREE_INIT(tp);
	    rt_tree_array[j].tl_tree = tp;
	    tp->tr_l.tl_op = OP_DB_LEAF;

	    /* bu_strlcpy not safe here, buffer size mismatch */
	    memset(namebuf, 0, NAMESIZE+1);
	    memcpy(namebuf, rp[j+1].M.m_instname, sizeof(rp[j+1].M.m_instname));

	    tp->tr_l.tl_name = bu_strdup(namebuf);

	    flip_mat_dbmat(diskmat, rp[j+1].M.m_mat, (dbip && dbip->dbi_version < 0) ? 1 : 0);

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
    ip->idb_meth = &OBJ[ID_COMBINATION];

    BU_ALLOC(comb, struct rt_comb_internal);
    RT_COMB_INTERNAL_INIT(comb);

    comb->tree = tree;

    ip->idb_ptr = (void *)comb;

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
	if (!dbip || dbip->dbi_version < 0) {
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
#define MAX_SS 128
	char shader_str[MAX_SS];

	memset(shader_str, 0, MAX_SS);

	/* copy shader info to a static string */

	/* write shader name.  c_matname is a buffer, bu_strlcpy not
	 * safe here.
	 */
	memcpy(shader_str, rp[0].c.c_matname, sizeof(rp[0].c.c_matname));

	bu_strlcat(shader_str, " ", MAX_SS);

	/* write shader parameters.  c_matparm is a buffer, bu_strlcpy
	 * not safe here.
	 */
	memcpy(shader_str+strlen(shader_str), rp[0].c.c_matparm, sizeof(rp[0].c.c_matparm));

	/* convert to TCL format and place into comb->shader */
	if (rt_shader_to_list(shader_str, &comb->shader)) {
	    bu_log("rt_comb_import4: Error: Cannot convert following shader to TCL format:\n");
	    bu_log("\t%s\n", shader_str);
	    bu_vls_free(&comb->shader);
	    return -1;
	}
    }

    /* May want separate inherit flags for color, shader, and material
     * but currently all rolled into one setting.
     */
    comb->inherit = (rp[0].c.c_inherit == DB_INH_HIGHER) ? 1 : 0;

    if (rt_tree_array) bu_free((void *)rt_tree_array, "rt_tree_array");

    return 0;
}


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
    size_t actual_count = 0;
    struct rt_tree_array *rt_tree_array;
    union tree *tp;
    union record *rp;
    size_t j;
    char *endp;
    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

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
	BU_ASSERT(actual_count == node_count);
	comb->tree = TREE_NULL;
    } else {
	rt_tree_array = (struct rt_tree_array *)NULL;
    }

    /* Reformat the data into the necessary V4 granules */
    BU_EXTERNAL_INIT(ep);
    ep->ext_nbytes = sizeof(union record) * (1 + node_count);
    ep->ext_buf = (uint8_t *)bu_calloc(1, ep->ext_nbytes, "v4 comb external");
    rp = (union record *)ep->ext_buf;

    /* Convert the member records */
    for (j = 0; j < node_count; j++) {
	tp = rt_tree_array[j].tl_tree;
	RT_CK_TREE(tp);
	if (tp->tr_op != OP_DB_LEAF) bu_bomb("rt_comb_export4() tree not OP_DB_LEAF");

	rp[j+1].u_id = ID_MEMB;
	switch (rt_tree_array[j].tl_op) {
	    case OP_INTERSECT:
		rp[j+1].M.m_relation = DB_OP_INTERSECT;
		break;
	    case OP_SUBTRACT:
		rp[j+1].M.m_relation = DB_OP_SUBTRACT;
		break;
	    case OP_UNION:
		rp[j+1].M.m_relation = DB_OP_UNION;
		break;
	    default:
		bu_bomb("rt_comb_export4() corrupt rt_tree_array");
	}

	NAMEMOVE(tp->tr_l.tl_name, rp[j+1].M.m_instname);
	rp[j+1].M.m_instname[NAMESIZE-1] = '\0'; /* sanity */

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

    /* convert TCL list format shader to keyword=value format */
    if (rt_shader_to_key_eq(bu_vls_addr(&comb->shader), &tmp_vls)) {

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
	BU_ASSERT(itp->tr_op == OP_DB_LEAF);
	BU_ASSERT(itp->tr_l.tl_name != NULL);

	switch (rt_tree_array[i].tl_op) {
	    case OP_INTERSECT:
		op = DB_OP_INTERSECT;
		break;
	    case OP_SUBTRACT:
		op = DB_OP_SUBTRACT;
		break;
	    case OP_UNION:
		op = DB_OP_UNION;
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
	    bu_vls_printf(vls, " az=%g, el=%g, ", az, el);
	}
	if (status & STAT_XLATE && itp->tr_l.tl_mat) {
	    bu_vls_printf(vls, " [%g, %g, %g]",
			  itp->tr_l.tl_mat[MDX]*mm2local,
			  itp->tr_l.tl_mat[MDY]*mm2local,
			  itp->tr_l.tl_mat[MDZ]*mm2local);
	}
	if (status & STAT_SCALE && itp->tr_l.tl_mat) {
	    bu_vls_printf(vls, " scale %g",
			  1.0/itp->tr_l.tl_mat[15]);
	}
	if (status & STAT_PERSP && itp->tr_l.tl_mat) {
	    bu_vls_printf(vls,
			  " Perspective=[%g, %g, %g]??",
			  itp->tr_l.tl_mat[12],
			  itp->tr_l.tl_mat[13],
			  itp->tr_l.tl_mat[14]);
	}
	bu_vls_printf(vls, "\n");
    }

    if (rt_tree_array) bu_free((void *)rt_tree_array, "rt_tree_array");
    db_free_tree(ntp, resp);
}


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
	    if (status & STAT_XLATE && tp->tr_l.tl_mat) {
		bu_vls_printf(vls, " [%g, %g, %g]",
			      tp->tr_l.tl_mat[MDX]*mm2local,
			      tp->tr_l.tl_mat[MDY]*mm2local,
			      tp->tr_l.tl_mat[MDZ]*mm2local);
	    }
	    if (status & STAT_SCALE && tp->tr_l.tl_mat) {
		bu_vls_printf(vls, " scale %g",
			      1.0/tp->tr_l.tl_mat[15]);
	    }
	    if (status & STAT_PERSP && tp->tr_l.tl_mat) {
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
	    bu_vls_printf(vls, "%c ", DB_OP_UNION);
	    goto bin;
	case OP_INTERSECT:
	    if (!indented) bu_vls_spaces(vls, 2*lvl);
	    bu_vls_printf(vls, "%c ", DB_OP_INTERSECT);
	    goto bin;
	case OP_SUBTRACT:
	    if (!indented) bu_vls_spaces(vls, 2*lvl);
	    bu_vls_printf(vls, "%c ", DB_OP_SUBTRACT);
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


void
db_comb_describe(
    struct bu_vls *str,
    const struct rt_comb_internal *comb,
    int verbose,
    double mm2local)
{
    RT_CK_COMB(comb);

    if (comb->region_flag) {
	bu_vls_printf(str,
		      "REGION id=%ld (air=%ld, los=%ld, GIFTmater=%ld) ",
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
	    db_tree_flatten_describe(str, comb->tree, 0, 1, mm2local, &rt_uniresource);
	} else {
	    rt_pr_tree_vls(str, comb->tree);
	}
    } else {
	bu_vls_strcat(str, "(empty tree)\n");
    }
}


/**
 * Free the storage associated with the rt_db_internal version of this combination.
 */
void
rt_comb_ifree(struct rt_db_internal *ip)
{
    register struct rt_comb_internal *comb;

    RT_CK_DB_INTERNAL(ip);
    comb = (struct rt_comb_internal *)ip->idb_ptr;

    if (comb) {
	/* Free the copy of the original name */
	if (comb->src_objname)
	    bu_free((void *)comb->src_objname, "comb src_objname");
	/* If tree hasn't been stolen, release it */
	db_free_tree(comb->tree, &rt_uniresource);
	RT_FREE_COMB_INTERNAL(comb);
	bu_free((void *)comb, "comb ifree");
    }
    ip->idb_ptr = ((void *)0);	/* sanity */
}


int
rt_comb_describe(
    struct bu_vls *str,
    const struct rt_db_internal *ip,
    int verbose,
    double mm2local)
{
    const struct rt_comb_internal *comb;

    RT_CK_DB_INTERNAL(ip);

    comb = (struct rt_comb_internal *)ip->idb_ptr;
    RT_CK_COMB(comb);

    db_comb_describe(str, comb, verbose, mm2local);
    return 0;
}


void
db_wrap_v4_external(struct bu_external *op, const char *name)
{
    union record *rec;

    BU_CK_EXTERNAL(op);

    rec = (union record *)op->ext_buf;
    NAMEMOVE(name, rec->s.s_name);
    rec->s.s_name[NAMESIZE-1] = '\0';
}


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
    for (i = howfar, inuse = 0, tlp = rt_tree_array; i > 0; i--, tlp++) {
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
	if (RT_G_DEBUG & RT_DEBUG_TREEWALK) {
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

	BU_GET(xtp, union tree);
	RT_TREE_INIT(xtp);
	xtp->tr_b.tb_left = curtree;
	xtp->tr_b.tb_right = tlp->tl_tree;
	xtp->tr_b.tb_regionp = (struct region *)0;
	xtp->tr_op = tlp->tl_op;
	curtree = xtp;
	tlp->tl_tree = TREE_NULL;	/* empty the input slot */
    }
    return curtree;
}


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

    BU_ASSERT(subtreecount < LONG_MAX);
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

	if (RT_G_DEBUG&RT_DEBUG_TREEWALK) {
	    bu_log("db_mkgift_tree() intermediate term:\n");
	    rt_pr_tree(tstart->tl_tree, 0);
	}

	/* tstart here at union */
	tstart = tnext;
    }

    curtree = db_mkbool_tree(trees, subtreecount, resp);
    if (RT_G_DEBUG&RT_DEBUG_TREEWALK) {
	bu_log("db_mkgift_tree() returns:\n");
	rt_pr_tree(curtree, 0);
    }
    return curtree;
}


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

int
db_comb_mvall(struct directory *dp, struct db_i *dbip, const char *old_name, const char *new_name, struct bu_ptbl *stack)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    union tree *comb_leaf;
    int done=0;
    int changed=0;

    /* Make sure the stack bu_ptbl is ready */
    bu_ptbl_reset(stack);

    if (!(dp->d_flags & RT_DIR_COMB)) return 0;

    if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) return 0;

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    comb_leaf = comb->tree;

    if (comb_leaf) {
	while (!done) {
	    while (comb_leaf->tr_op != OP_DB_LEAF) {
		bu_ptbl_ins(stack, (long *)comb_leaf);
		comb_leaf = comb_leaf->tr_b.tb_left;
	    }

	    if (BU_STR_EQUAL(comb_leaf->tr_l.tl_name, old_name)) {
		bu_free(comb_leaf->tr_l.tl_name, "comb_leaf->tr_l.tl_name");
		comb_leaf->tr_l.tl_name = bu_strdup(new_name);
		changed = 1;
	    }

	    if (BU_PTBL_LEN(stack) < 1) {
		break;
	    }
	    comb_leaf = (union tree *)BU_PTBL_GET(stack, BU_PTBL_LEN(stack)-1);
	    if (comb_leaf->tr_op != OP_DB_LEAF) {
		bu_ptbl_rm(stack, (long *)comb_leaf);
		comb_leaf = comb_leaf->tr_b.tb_right;
	    }
	}
    }

    if (changed) {
	if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource)) {
	    rt_db_free_internal(&intern);
	    return 2;
	}
    }

    rt_db_free_internal(&intern);

    /* success */
    return 1;
}

static void
_db_comb_find_invalid(int *inv_cnt, struct db_i *dbip, union tree *tp)
{
    if (!tp) return;

    RT_CHECK_DBI(dbip);
    RT_CK_TREE(tp);

    switch (tp->tr_op) {
        case OP_UNION:
        case OP_INTERSECT:
        case OP_SUBTRACT:
        case OP_XOR:
            _db_comb_find_invalid(inv_cnt, dbip, tp->tr_b.tb_right);
            /* fall through */
        case OP_NOT:
        case OP_GUARD:
        case OP_XNOP:
            _db_comb_find_invalid(inv_cnt, dbip, tp->tr_b.tb_left);
            break;
	case OP_DB_LEAF:
	    if (db_lookup(dbip, tp->tr_l.tl_name, LOOKUP_QUIET) == RT_DIR_NULL) {
		(*inv_cnt) = (*inv_cnt) + 1;
            }
	    break;
	default:
	    bu_log("_db_comb_get_children: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("_db_comb_get_children\n");
    }
}


static void
_db_comb_get_children(struct directory **children, int *curr_ind, int curr_bool, struct db_i *dbip, union tree *tp, int *bool_ops, matp_t *mats)
{
   struct directory *dp;
   int bool_op = curr_bool;

    if (!tp)
        return;

    RT_CHECK_DBI(dbip);
    RT_CK_TREE(tp);

    switch (tp->tr_op) {
        case OP_UNION:
        case OP_INTERSECT:
        case OP_SUBTRACT:
        case OP_XOR:
	    bool_op = tp->tr_op;
            _db_comb_get_children(children, curr_ind, bool_op, dbip, tp->tr_b.tb_right, bool_ops, mats);
            /* fall through */
        case OP_NOT:
        case OP_GUARD:
        case OP_XNOP:
            _db_comb_get_children(children, curr_ind, bool_op, dbip, tp->tr_b.tb_left, bool_ops, mats);
            break;
        case OP_DB_LEAF:
            if ((dp=db_lookup(dbip, tp->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
                return;
            } else {
                /* List the child, if it isn't hidden */
                if (!(dp->d_flags & RT_DIR_HIDDEN)) {
		    if (children) {
			children[*curr_ind] = dp;
		    }
		    if (bool_ops) {
			bool_ops[*curr_ind] = bool_op;
		    }
		    if (mats) {
			if (!mats[*curr_ind]) mats[*curr_ind] = (matp_t)bu_calloc(1, sizeof(mat_t), "mat copy");
			if (tp->tr_l.tl_mat) {
			    MAT_COPY(mats[*curr_ind], tp->tr_l.tl_mat);
			} else {
			    MAT_IDN(mats[*curr_ind]);
			}
		    }
		    (*curr_ind) = (*curr_ind) - 1;
		}
                break;
            }

        default:
            bu_log("_db_comb_get_children: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("_db_comb_get_children\n");
    }
}

int
db_comb_children(struct db_i *dbip, struct rt_comb_internal *comb, struct directory ***children, int **bool_ops, matp_t **mats)
{
    int dp_index = 0;
    int node_count = 0;
    int *bops = NULL;
    matp_t *ms = NULL;
    int invalid_cnt = 0;

    RT_CK_DBI(dbip);
    RT_CK_COMB(comb);

    if (!children)
	return 0;

    node_count = db_tree_nleaves(comb->tree);
    if (!node_count) return 0;

    _db_comb_find_invalid(&invalid_cnt, dbip, comb->tree);

    node_count = node_count - invalid_cnt;

    if (children) {
	if (!*children) (*children) = (struct directory **)bu_calloc(node_count + 1, sizeof(struct directory *), "directory array");
	if (!*children)
	    return 0;
    }
    if (bool_ops) {
	if (!*bool_ops) (*bool_ops) = (int *)bu_calloc(node_count + 1, sizeof(int), "bool ops");
	bops = (*bool_ops);
	bops[node_count] = 0;
    }
    if (mats) {
	if (!*mats) (*mats) = (matp_t *)bu_calloc(node_count + 1, sizeof(matp_t), "pointers to matrices");
	ms = (*mats);
	ms[node_count] = NULL;
    }

    dp_index = node_count - 1;
    _db_comb_get_children(*children, &dp_index, OP_UNION, dbip, comb->tree, bops, ms);

    (*children)[node_count] = RT_DIR_NULL;
    return node_count;
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
