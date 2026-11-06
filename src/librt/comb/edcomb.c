/*                        E D C O M B . C
 * BRL-CAD
 *
 * Copyright (c) 2025-2026 United States Government as represented by
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
/** @file comb/edcomb.c
 *
 * Editing operations for combination (boolean tree) primitives.
 *
 * Flatten-then-rebuild pattern (same as red.c / combmem.c):
 *   1. db_tree_nleaves() + db_flatten_tree() → rt_tree_array[]
 *   2. Mutate the array (add / delete / change op / change matrix)
 *   3. db_mkbool_tree() → new tree
 *
 * Like all librt edit handlers, these operations mutate only s->es_int.
 * The owning application decides when to checkpoint, commit, or cancel the
 * shared intermediate.  A primitive handler must never write the database or
 * invalidate the rt_edit it is operating on.
 *
 * ECMD constants
 * --------------
 * ECMD_COMB_ADD_MEMBER   (12001) – append a new leaf
 * ECMD_COMB_DEL_MEMBER   (12002) – delete a leaf by 0-based index
 * ECMD_COMB_SET_OP       (12003) – change a leaf's boolean operator
 * ECMD_COMB_SET_MATRIX   (12004) – set a leaf's transformation matrix
 *
 * Parameter conventions (all values go in e_para[])
 * --------------------------------------------------
 *  ADD_MEMBER:
 *    s->e_str[0]    = name of the new member
 *    s->e_para[0]   = bool op (OP_UNION=2, OP_INTERSECT=3, OP_SUBTRACT=4)
 *    s->e_inpara    = 1
 *    ce->es_mat_valid / ce->es_mat = optional transform matrix (0 → identity)
 *
 *  DEL_MEMBER:
 *    s->e_para[0]   = 0-based member index
 *    s->e_inpara    = 1
 *
 *  SET_OP:
 *    s->e_para[0]   = 0-based member index
 *    s->e_para[1]   = new bool op (OP_UNION / OP_INTERSECT / OP_SUBTRACT)
 *    s->e_inpara    = 2
 *
 *  SET_MATRIX:
 *    s->e_para[0]      = 0-based member index
 *    s->e_para[1..16]  = 16 matrix elements in row-major order (mat_t)
 *    s->e_inpara       = 17
 *    (requires RT_EDIT_MAXPARA >= 17; we have bumped it to 20)
 */

#include "common.h"

#include <limits.h>
#include <math.h>
#include <string.h>

#include "bu/malloc.h"
#include "bu/avs.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/nongeom.h"
#include "rt/op.h"
#include "rt/tree.h"
#include "rt/db_attr.h"
#include "../primitives/edit_private.h"

/* ------------------------------------------------------------------ */
/* ECMD constants                                                       */
/* ------------------------------------------------------------------ */

#define ECMD_COMB_ADD_MEMBER   12001
#define ECMD_COMB_DEL_MEMBER   12002
#define ECMD_COMB_SET_OP       12003
#define ECMD_COMB_SET_MATRIX   12004
/* Material / region property ECMDs */
#define ECMD_COMB_SET_REGION   12005  /* set region_flag (0/1) */
#define ECMD_COMB_SET_COLOR    12006  /* set RGB color (e_para[0..2]=R,G,B 0-255; e_inpara=0 clears) */
#define ECMD_COMB_SET_SHADER   12007  /* set shader string (e_str[0]) */
#define ECMD_COMB_SET_MATERIAL 12008  /* set material string (e_str[0]) */
#define ECMD_COMB_SET_REGION_ID 12009 /* set region_id (e_para[0]) */
#define ECMD_COMB_SET_AIRCODE  12010  /* set aircode  (e_para[0]) */
#define ECMD_COMB_SET_GIFTMATER 12011 /* set GIFTmater (e_para[0]) */
#define ECMD_COMB_SET_LOS      12012  /* set los (e_para[0]) */

static int
comb_integer_param(struct rt_edit *s, int parameter, fastf_t minimum,
	fastf_t maximum, const char *name, long *value)
{
    fastf_t input = s->e_para[parameter];
    if (input <= (fastf_t)LONG_MIN || input >= (fastf_t)LONG_MAX ||
	input < minimum || input > maximum || !EQUAL(input, floor(input))) {
	bu_vls_printf(s->log_str,
		"ERROR: %s must be an integer in [%g, %g]\n",
		name, minimum, maximum);
	return BRLCAD_ERROR;
    }

    *value = (long)input;
    return BRLCAD_OK;
}

/* ------------------------------------------------------------------ */
/* Per-instance edit state                                              */
/* ------------------------------------------------------------------ */

struct rt_comb_edit {
    /* When non-zero, es_mat supplies the 4x4 xform for ADD_MEMBER */
    int es_mat_valid;
    mat_t es_mat;
};

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

/* Validate the session internal before modifying it. */
static int
comb_edit_validate(struct rt_edit *s)
{
    if (!s || !s->es_int.idb_ptr)
	return BRLCAD_ERROR;
    struct rt_comb_internal *comb = (struct rt_comb_internal *)s->es_int.idb_ptr;
    RT_CK_COMB(comb);
    return BRLCAD_OK;
}


static void
comb_edit_sync_attributes(struct rt_edit *s,
	const struct rt_comb_internal *comb)
{
    if (s->es_int.idb_avs.magic != BU_AVS_MAGIC)
	bu_avs_init_empty(&s->es_int.idb_avs);
    db5_sync_comb_to_attr(&s->es_int.idb_avs, comb);
}

/* Flatten comb->tree into a freshly allocated rt_tree_array.
 * *count_out receives the number of leaves.
 * Returns NULL if the tree is empty (count == 0). */
static struct rt_tree_array *
comb_flatten(struct rt_comb_internal *comb, size_t *count_out,
	     struct bu_vls *log_str)
{
    size_t node_count = db_tree_nleaves(comb->tree);
    *count_out = node_count;

    if (!node_count)
	return NULL;

    struct rt_tree_array *arr = (struct rt_tree_array *)bu_calloc(
	    node_count, sizeof(struct rt_tree_array), "comb_flatten arr");

    size_t actual = (size_t)((struct rt_tree_array *)db_flatten_tree(
		arr, comb->tree, OP_UNION, 0) - arr);

    if (actual != node_count) {
	bu_vls_printf(log_str,
		"WARNING: comb_flatten: expected %zu leaves, got %zu\n",
		node_count, actual);
	*count_out = actual;
    }

    /* db_flatten_tree with freeflag=0 does NOT free the old tree — but it
     * reuses the leaf nodes by pointer.  We rebuild a new tree below, so
     * set comb->tree to NULL to avoid a double-free later. */
    comb->tree = TREE_NULL;

    return arr;
}

/* Rebuild comb->tree from a (possibly modified) rt_tree_array. */
static void
comb_unflatten(struct rt_comb_internal *comb,
	       struct rt_tree_array *arr, size_t count)
{
    if (!count || !arr) {
	comb->tree = TREE_NULL;
	return;
    }

    /* db_mkbool_tree consumes arr entries (sets their tl_tree to TREE_NULL) */
    comb->tree = db_mkbool_tree(arr, count);
}

/* ------------------------------------------------------------------ */
/* ECMD implementations                                                 */
/* ------------------------------------------------------------------ */

static int
ecmd_comb_add_member(struct rt_edit *s)
{
    struct rt_comb_edit *ce = (struct rt_comb_edit *)s->ipe_ptr;
    struct rt_comb_internal *comb = (struct rt_comb_internal *)s->es_int.idb_ptr;

    if (comb_edit_validate(s) != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (!s->e_inpara || s->e_inpara < 1) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_COMB_ADD_MEMBER: boolean op required in e_para[0]\n");
	return BRLCAD_ERROR;
    }

    const char *member_name = s->e_nstr > 0 ? s->e_str[0] : NULL;
    if (!member_name || !member_name[0]) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_COMB_ADD_MEMBER: member name required\n");
	return BRLCAD_ERROR;
    }

    long op_value;
    if (comb_integer_param(s, 0, INT_MIN, INT_MAX, "boolean op",
	    &op_value) != BRLCAD_OK)
	return BRLCAD_ERROR;
    int op = (int)op_value;
    if (op != OP_UNION && op != OP_INTERSECT && op != OP_SUBTRACT) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_COMB_ADD_MEMBER: invalid boolean op %d "
		"(use OP_UNION=%d, OP_INTERSECT=%d, OP_SUBTRACT=%d)\n",
		op, OP_UNION, OP_INTERSECT, OP_SUBTRACT);
	return BRLCAD_ERROR;
    }

    /* Build a new OP_DB_LEAF node */
    union tree *leaf;
    BU_GET(leaf, union tree);
    RT_TREE_INIT(leaf);
    leaf->tr_l.tl_op   = OP_DB_LEAF;
    leaf->tr_l.tl_name = bu_strdup(member_name);
    leaf->tr_l.tl_mat  = NULL;

    if (ce->es_mat_valid) {
	leaf->tr_l.tl_mat = (matp_t)bu_malloc(sizeof(mat_t), "tl_mat");
	MAT_COPY(leaf->tr_l.tl_mat, ce->es_mat);
    }

    /* Flatten the existing tree, append the new leaf, rebuild */
    size_t count;
    struct rt_tree_array *arr = comb_flatten(comb, &count, s->log_str);

    struct rt_tree_array *newarr = (struct rt_tree_array *)bu_realloc(
	    arr, (count + 1) * sizeof(struct rt_tree_array),
	    "comb add member");
    newarr[count].tl_tree = leaf;
    newarr[count].tl_op   = op;
    count++;

    comb_unflatten(comb, newarr, count);
    bu_free(newarr, "comb add member arr");

    s->e_inpara = 0;

    bu_vls_printf(s->log_str,
	"ADD_MEMBER: '%s' added to intermediate combination (op=%d)\n",
	member_name, op);
    return BRLCAD_OK;
}


static int
ecmd_comb_del_member(struct rt_edit *s)
{
    struct rt_comb_internal *comb = (struct rt_comb_internal *)s->es_int.idb_ptr;

    if (comb_edit_validate(s) != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (!s->e_inpara || s->e_inpara < 1) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_COMB_DEL_MEMBER: member index required in e_para[0]\n");
	return BRLCAD_ERROR;
    }

    long target_value;
    if (comb_integer_param(s, 0, 0.0, INT_MAX, "member index",
	    &target_value) != BRLCAD_OK)
	return BRLCAD_ERROR;
    int target = (int)target_value;

    /* Pre-validate the index before flattening (comb_flatten NULLs comb->tree) */
    size_t node_count = db_tree_nleaves(comb->tree);
    if (!node_count || (size_t)target >= node_count) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_COMB_DEL_MEMBER: index %d out of range [0, %zu)\n",
		target, node_count);
	return BRLCAD_ERROR;
    }

    size_t count;
    struct rt_tree_array *arr = comb_flatten(comb, &count, s->log_str);

    if (!arr) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_COMB_DEL_MEMBER: failed to flatten tree\n");
	return BRLCAD_ERROR;
    }

    /* Free the leaf node being removed */
    db_free_tree(arr[target].tl_tree);

    /* Shift remaining entries down */
    for (size_t i = (size_t)target; i < count - 1; i++)
	arr[i] = arr[i + 1];
    count--;

    comb_unflatten(comb, arr, count);
    bu_free(arr, "comb del arr");

    s->e_inpara = 0;

    bu_vls_printf(s->log_str,
	"DEL_MEMBER: index %d deleted from intermediate combination\n",
	target);
    return BRLCAD_OK;
}


static int
ecmd_comb_set_op(struct rt_edit *s)
{
    struct rt_comb_internal *comb = (struct rt_comb_internal *)s->es_int.idb_ptr;

    if (comb_edit_validate(s) != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (!s->e_inpara || s->e_inpara < 2) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_COMB_SET_OP: need member_index and new_op "
		"in e_para[0..1] (e_inpara=%d)\n", s->e_inpara);
	return BRLCAD_ERROR;
    }

    long target_value;
    long op_value;
    if (comb_integer_param(s, 0, 0.0, INT_MAX, "member index",
	    &target_value) != BRLCAD_OK ||
	comb_integer_param(s, 1, INT_MIN, INT_MAX, "boolean op",
	    &op_value) != BRLCAD_OK)
	return BRLCAD_ERROR;
    int target = (int)target_value;
    int new_op = (int)op_value;

    if (new_op != OP_UNION && new_op != OP_INTERSECT && new_op != OP_SUBTRACT) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_COMB_SET_OP: invalid boolean op %d\n", new_op);
	return BRLCAD_ERROR;
    }

    /* Pre-validate index before flattening */
    size_t node_count_op = db_tree_nleaves(comb->tree);
    if (!node_count_op || (size_t)target >= node_count_op) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_COMB_SET_OP: index %d out of range [0, %zu)\n",
		target, node_count_op);
	return BRLCAD_ERROR;
    }

    size_t count;
    struct rt_tree_array *arr = comb_flatten(comb, &count, s->log_str);

    if (!arr) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_COMB_SET_OP: failed to flatten tree\n");
	return BRLCAD_ERROR;
    }

    arr[target].tl_op = new_op;

    comb_unflatten(comb, arr, count);
    bu_free(arr, "comb set_op arr");

    s->e_inpara = 0;

    bu_vls_printf(s->log_str,
	"SET_OP: intermediate member %d op changed to %d\n",
	target, new_op);
    return BRLCAD_OK;
}


static int
ecmd_comb_set_matrix(struct rt_edit *s)
{
    struct rt_comb_internal *comb = (struct rt_comb_internal *)s->es_int.idb_ptr;

    if (comb_edit_validate(s) != BRLCAD_OK)
	return BRLCAD_ERROR;

    /* e_para[0] = member index, e_para[1..16] = 16 matrix elements */
    if (!s->e_inpara || s->e_inpara < 17) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_COMB_SET_MATRIX: member_index + 16 matrix values "
		"required (e_inpara=%d, need 17)\n", s->e_inpara);
	return BRLCAD_ERROR;
    }

    long target_value;
    if (comb_integer_param(s, 0, 0.0, INT_MAX, "member index",
	    &target_value) != BRLCAD_OK)
	return BRLCAD_ERROR;
    int target = (int)target_value;

    /* Pre-validate index before flattening */
    size_t node_count_mat = db_tree_nleaves(comb->tree);
    if (!node_count_mat || (size_t)target >= node_count_mat) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_COMB_SET_MATRIX: index %d out of range [0, %zu)\n",
		target, node_count_mat);
	return BRLCAD_ERROR;
    }

    size_t count;
    struct rt_tree_array *arr = comb_flatten(comb, &count, s->log_str);

    if (!arr) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_COMB_SET_MATRIX: failed to flatten tree\n");
	return BRLCAD_ERROR;
    }

    union tree *leaf = arr[target].tl_tree;
    RT_CK_TREE(leaf);

    if (leaf->tr_l.tl_mat == NULL)
	leaf->tr_l.tl_mat = (matp_t)bu_malloc(sizeof(mat_t), "tl_mat");

    /* Copy e_para[1..16] into the matrix (RT_EDIT_MAXPARA >= 20) */
    {
	matp_t dst = leaf->tr_l.tl_mat;
	const fastf_t *src = &s->e_para[1];
	dst[ 0] = src[ 0]; dst[ 1] = src[ 1]; dst[ 2] = src[ 2]; dst[ 3] = src[ 3];
	dst[ 4] = src[ 4]; dst[ 5] = src[ 5]; dst[ 6] = src[ 6]; dst[ 7] = src[ 7];
	dst[ 8] = src[ 8]; dst[ 9] = src[ 9]; dst[10] = src[10]; dst[11] = src[11];
	dst[12] = src[12]; dst[13] = src[13]; dst[14] = src[14]; dst[15] = src[15];
    }

    comb_unflatten(comb, arr, count);
    bu_free(arr, "comb set_mat arr");

    s->e_inpara = 0;

    bu_vls_printf(s->log_str,
	"SET_MATRIX: intermediate member %d matrix updated\n", target);
    return BRLCAD_OK;
}


static union tree *
comb_instance_leaf(union tree *tree, const char *name, int target, int *current)
{
    if (!tree || !name || !current)
	return TREE_NULL;
    switch (tree->tr_op) {
	case OP_DB_LEAF:
	    if (BU_STR_EQUAL(tree->tr_l.tl_name, name)) {
		if (*current == target)
		    return tree;
		(*current)++;
	    }
	    return TREE_NULL;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT: {
	    union tree *leaf = comb_instance_leaf(tree->tr_b.tb_left, name,
		target, current);
	    return leaf ? leaf : comb_instance_leaf(tree->tr_b.tb_right, name,
		target, current);
	}
	default:
	    return TREE_NULL;
    }
}


int
rt_edit_comb_instance_matrix_get(struct rt_edit *s, const char *name,
	int instance, mat_t matrix)
{
    if (!s || !name || !matrix || instance < 0 || !s->es_int.idb_ptr ||
	s->es_int.idb_type != ID_COMBINATION)
	return BRLCAD_ERROR;
    struct rt_comb_internal *comb =
	(struct rt_comb_internal *)s->es_int.idb_ptr;
    int current = 0;
    union tree *leaf = comb_instance_leaf(comb->tree, name, instance,
	&current);
    if (!leaf)
	return BRLCAD_ERROR;
    if (leaf->tr_l.tl_mat)
	MAT_COPY(matrix, leaf->tr_l.tl_mat);
    else
	MAT_IDN(matrix);
    return BRLCAD_OK;
}

int
rt_edit_comb_instance_matrix_set(struct rt_edit *s, const char *name, int instance, const mat_t matrix)
{
    if (!s || !name || !matrix || instance < 0 || !s->es_int.idb_ptr || s->es_int.idb_type != ID_COMBINATION)
	return BRLCAD_ERROR;
    struct rt_comb_internal *comb = (struct rt_comb_internal *)s->es_int.idb_ptr;
    int current = 0;
    union tree *leaf = comb_instance_leaf(comb->tree, name, instance, &current);
    if (!leaf)
	return BRLCAD_ERROR;
    if (!leaf->tr_l.tl_mat)
	leaf->tr_l.tl_mat = (matp_t)bu_malloc(sizeof(mat_t), "comb instance matrix");
    MAT_COPY(leaf->tr_l.tl_mat, matrix);
    return BRLCAD_OK;
}


/* ------------------------------------------------------------------ */
/* Material / region property ECMDs                                     */
/* ------------------------------------------------------------------ */

static int
ecmd_comb_set_region(struct rt_edit *s)
{
    struct rt_comb_internal *comb = (struct rt_comb_internal *)s->es_int.idb_ptr;

    if (comb_edit_validate(s) != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (!s->e_inpara) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_COMB_SET_REGION: 0 or 1 required in e_para[0]\n");
	return BRLCAD_ERROR;
    }

    long flag_value;
    if (comb_integer_param(s, 0, 0.0, 1.0, "region flag",
	    &flag_value) != BRLCAD_OK)
	return BRLCAD_ERROR;
    char flag = (char)flag_value;
    comb->region_flag = flag;
    comb_edit_sync_attributes(s, comb);
    s->e_inpara = 0;
    bu_vls_printf(s->log_str,
	"SET_REGION: intermediate region_flag=%d\n", (int)flag);
    return BRLCAD_OK;
}


static int
ecmd_comb_set_color(struct rt_edit *s)
{
    struct rt_comb_internal *comb = (struct rt_comb_internal *)s->es_int.idb_ptr;

    if (comb_edit_validate(s) != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (s->e_inpara == 0) {
	/* Clear the color: zero the struct field and also remove the
	 * attribute from the avs under both the legacy "rgb" key and
	 * the canonical "color" key so the subsequent export is clean. */
	comb->rgb_valid = 0;
	comb->rgb[0] = comb->rgb[1] = comb->rgb[2] = 0;
	if (s->es_int.idb_avs.magic == BU_AVS_MAGIC) {
	    bu_avs_remove(&s->es_int.idb_avs, "rgb");
	    bu_avs_remove(&s->es_int.idb_avs, "color");
	}
    } else if (s->e_inpara == 3) {
	long rgb[ELEMENTS_PER_VECT];
	for (int i = 0; i < ELEMENTS_PER_VECT; ++i) {
	    if (comb_integer_param(s, i, 0.0, UCHAR_MAX, "color component",
		    &rgb[i]) != BRLCAD_OK)
		return BRLCAD_ERROR;
	}
	comb->rgb_valid = 1;
	comb->rgb[0] = (unsigned char)rgb[0];
	comb->rgb[1] = (unsigned char)rgb[1];
	comb->rgb[2] = (unsigned char)rgb[2];
    } else {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_COMB_SET_COLOR: e_inpara must be 0 (clear) or 3 (R G B)\n");
	return BRLCAD_ERROR;
    }

    int rgb_r = comb->rgb[0], rgb_g = comb->rgb[1], rgb_b = comb->rgb[2];
    int rgb_v = comb->rgb_valid;
    comb_edit_sync_attributes(s, comb);
    s->e_inpara = 0;

    if (rgb_v)
	bu_vls_printf(s->log_str,
	    "SET_COLOR: intermediate rgb=(%d,%d,%d)\n",
	    rgb_r, rgb_g, rgb_b);
    else
	bu_vls_printf(s->log_str,
	    "SET_COLOR: intermediate color cleared\n");
    return BRLCAD_OK;
}


static int
ecmd_comb_set_shader(struct rt_edit *s)
{
    struct rt_comb_internal *comb = (struct rt_comb_internal *)s->es_int.idb_ptr;

    if (comb_edit_validate(s) != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (s->e_nstr < 1) {
	bu_vls_printf(s->log_str,
	    "ERROR: ECMD_COMB_SET_SHADER: shader string required\n");
	return BRLCAD_ERROR;
    }
    bu_vls_sprintf(&comb->shader, "%s", s->e_str[0]);
    s->e_inpara = 0;

    bu_vls_printf(s->log_str,
	"SET_SHADER: intermediate shader='%s'\n", s->e_str[0]);
    return BRLCAD_OK;
}


static int
ecmd_comb_set_material(struct rt_edit *s)
{
    struct rt_comb_internal *comb = (struct rt_comb_internal *)s->es_int.idb_ptr;

    if (comb_edit_validate(s) != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (s->e_nstr < 1) {
	bu_vls_printf(s->log_str,
	    "ERROR: ECMD_COMB_SET_MATERIAL: material string required\n");
	return BRLCAD_ERROR;
    }
    bu_vls_sprintf(&comb->material, "%s", s->e_str[0]);
    /* Sync the material_name field to the avs so rt_comb_export5 will
     * persist it (the export function only reads the avs for material). */
    comb_edit_sync_attributes(s, comb);
    s->e_inpara = 0;

    bu_vls_printf(s->log_str,
	"SET_MATERIAL: intermediate material='%s'\n", s->e_str[0]);
    return BRLCAD_OK;
}


static int
ecmd_comb_set_region_id(struct rt_edit *s)
{
    struct rt_comb_internal *comb = (struct rt_comb_internal *)s->es_int.idb_ptr;

    if (comb_edit_validate(s) != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (!s->e_inpara) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_COMB_SET_REGION_ID: region_id required in e_para[0]\n");
	return BRLCAD_ERROR;
    }

    long rid;
    if (comb_integer_param(s, 0, -(fastf_t)LONG_MAX, (fastf_t)LONG_MAX,
	    "region_id", &rid) != BRLCAD_OK)
	return BRLCAD_ERROR;
    comb->region_id = rid;
    comb_edit_sync_attributes(s, comb);
    s->e_inpara = 0;
    bu_vls_printf(s->log_str,
	"SET_REGION_ID: intermediate region_id=%ld\n", rid);
    return BRLCAD_OK;
}


static int
ecmd_comb_set_aircode(struct rt_edit *s)
{
    struct rt_comb_internal *comb = (struct rt_comb_internal *)s->es_int.idb_ptr;

    if (comb_edit_validate(s) != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (!s->e_inpara) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_COMB_SET_AIRCODE: aircode required in e_para[0]\n");
	return BRLCAD_ERROR;
    }

    long ac;
    if (comb_integer_param(s, 0, -(fastf_t)LONG_MAX, (fastf_t)LONG_MAX,
	    "aircode", &ac) != BRLCAD_OK)
	return BRLCAD_ERROR;
    comb->aircode = ac;
    comb_edit_sync_attributes(s, comb);
    s->e_inpara = 0;
    bu_vls_printf(s->log_str,
	"SET_AIRCODE: intermediate aircode=%ld\n", ac);
    return BRLCAD_OK;
}


static int
ecmd_comb_set_giftmater(struct rt_edit *s)
{
    struct rt_comb_internal *comb = (struct rt_comb_internal *)s->es_int.idb_ptr;

    if (comb_edit_validate(s) != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (!s->e_inpara) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_COMB_SET_GIFTMATER: GIFTmater required in e_para[0]\n");
	return BRLCAD_ERROR;
    }

    long gm;
    if (comb_integer_param(s, 0, -(fastf_t)LONG_MAX, (fastf_t)LONG_MAX,
	    "GIFTmater", &gm) != BRLCAD_OK)
	return BRLCAD_ERROR;
    comb->GIFTmater = gm;
    comb_edit_sync_attributes(s, comb);
    s->e_inpara = 0;
    bu_vls_printf(s->log_str,
	"SET_GIFTMATER: intermediate GIFTmater=%ld\n", gm);
    return BRLCAD_OK;
}


static int
ecmd_comb_set_los(struct rt_edit *s)
{
    struct rt_comb_internal *comb = (struct rt_comb_internal *)s->es_int.idb_ptr;

    if (comb_edit_validate(s) != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (!s->e_inpara) {
	bu_vls_printf(s->log_str,
		"ERROR: ECMD_COMB_SET_LOS: los required in e_para[0]\n");
	return BRLCAD_ERROR;
    }

    long los_val;
    if (comb_integer_param(s, 0, -(fastf_t)LONG_MAX, (fastf_t)LONG_MAX,
	    "los", &los_val) != BRLCAD_OK)
	return BRLCAD_ERROR;
    comb->los = los_val;
    comb_edit_sync_attributes(s, comb);
    s->e_inpara = 0;
    bu_vls_printf(s->log_str,
	"SET_LOS: intermediate los=%ld\n", los_val);
    return BRLCAD_OK;
}


/* ------------------------------------------------------------------ */
/* rt_functab entry points                                              */
/* ------------------------------------------------------------------ */

C_DECL void *
rt_edit_comb_prim_edit_create(struct rt_edit *s)
{
    struct rt_comb_edit *ce;
    BU_GET(ce, struct rt_comb_edit);
    ce->es_mat_valid = 0;
    MAT_IDN(ce->es_mat);

    (void)s;

    return (void *)ce;
}

C_DECL void
rt_edit_comb_prim_edit_destroy(void *ptr)
{
    struct rt_comb_edit *ce = (struct rt_comb_edit *)ptr;
    if (!ce)
	return;
    BU_PUT(ce, struct rt_comb_edit);
}

C_DECL void
rt_edit_comb_set_edit_mode(struct rt_edit *s, int mode)
{
    rt_edit_set_edflag(s, mode);

    switch (mode) {
	case ECMD_COMB_ADD_MEMBER:
	case ECMD_COMB_SET_MATRIX:
	    s->edit_mode = RT_PARAMS_EDIT_TRANS;
	    break;
	case ECMD_COMB_DEL_MEMBER:
	case ECMD_COMB_SET_OP:
	    s->edit_mode = RT_PARAMS_EDIT_PICK;
	    break;
	case ECMD_COMB_SET_REGION:
	case ECMD_COMB_SET_COLOR:
	case ECMD_COMB_SET_SHADER:
	case ECMD_COMB_SET_MATERIAL:
	case ECMD_COMB_SET_REGION_ID:
	case ECMD_COMB_SET_AIRCODE:
	case ECMD_COMB_SET_GIFTMATER:
	case ECMD_COMB_SET_LOS:
	    s->edit_mode = RT_PARAMS_EDIT_SCALE;
	    break;
	default:
	    break;
    }
}

static void
comb_ed(struct rt_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    rt_edit_set_edflag(s, arg);
    switch (arg) {
	case ECMD_COMB_ADD_MEMBER:
	case ECMD_COMB_SET_MATRIX:
	    s->edit_mode = RT_PARAMS_EDIT_TRANS;
	    break;
	case ECMD_COMB_DEL_MEMBER:
	case ECMD_COMB_SET_OP:
	    s->edit_mode = RT_PARAMS_EDIT_PICK;
	    break;
	case ECMD_COMB_SET_REGION:
	case ECMD_COMB_SET_COLOR:
	case ECMD_COMB_SET_SHADER:
	case ECMD_COMB_SET_MATERIAL:
	case ECMD_COMB_SET_REGION_ID:
	case ECMD_COMB_SET_AIRCODE:
	case ECMD_COMB_SET_GIFTMATER:
	case ECMD_COMB_SET_LOS:
	    s->edit_mode = RT_PARAMS_EDIT_SCALE;
	    break;
	default:
	    break;
    }
    rt_edit_process(s);
}

struct rt_edit_menu_item comb_menu[] = {
    { "COMB MENU", NULL, 0 },
    { "Add Member", comb_ed, ECMD_COMB_ADD_MEMBER },
    { "Delete Member", comb_ed, ECMD_COMB_DEL_MEMBER },
    { "Set Member Op", comb_ed, ECMD_COMB_SET_OP },
    { "Set Member Matrix", comb_ed, ECMD_COMB_SET_MATRIX },
    { "Set Region Flag", comb_ed, ECMD_COMB_SET_REGION },
    { "Set Color", comb_ed, ECMD_COMB_SET_COLOR },
    { "Set Shader", comb_ed, ECMD_COMB_SET_SHADER },
    { "Set Material", comb_ed, ECMD_COMB_SET_MATERIAL },
    { "Set Region ID", comb_ed, ECMD_COMB_SET_REGION_ID },
    { "Set Aircode", comb_ed, ECMD_COMB_SET_AIRCODE },
    { "Set GIFTmater", comb_ed, ECMD_COMB_SET_GIFTMATER },
    { "Set LOS", comb_ed, ECMD_COMB_SET_LOS },
    { "", NULL, 0 }
};

C_DECL struct rt_edit_menu_item *
rt_edit_comb_menu_item(const struct bn_tol *UNUSED(tol))
{
    return comb_menu;
}

/* ------------------------------------------------------------------ */
/* ft_edit_desc descriptor for the Combination / Region primitive     */
/* ------------------------------------------------------------------ */

/* ECMD_COMB_ADD_MEMBER: member name (string) + boolean op (enum) */
static const char * const comb_bool_op_labels[] = { "Union", "Intersect", "Subtract" };
static const int comb_bool_op_ids[] = { 2 /* OP_UNION */, 3 /* OP_INTERSECT */, 4 /* OP_SUBTRACT */ };

static const struct rt_edit_param_desc comb_add_member_params[] = {
    {
	"member_name",        /* name         */
	"Member Name",        /* label        */
	RT_EDIT_PARAM_STRING, /* type         */
	0,                    /* index (unused for STRING) */
	RT_EDIT_PARAM_NO_LIMIT, /* range_min  */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	NULL,                 /* units        */
	0, NULL, NULL,        /* enum (unused) */
	"member_name"         /* prim_field   */
    },
    {
	"op",                 /* name         */
	"Boolean Operation",  /* label        */
	RT_EDIT_PARAM_ENUM,   /* type         */
	0,                    /* index        */
	RT_EDIT_PARAM_NO_LIMIT, /* range_min  */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	NULL,                 /* units        */
	3,                    /* nenum        */
	comb_bool_op_labels,  /* enum_labels  */
	comb_bool_op_ids,     /* enum_ids     */
	NULL                  /* prim_field   */
    }
};

/* ECMD_COMB_DEL_MEMBER: member index */
static const struct rt_edit_param_desc comb_del_member_params[] = {
    {
	"member_index",       /* name         */
	"Member Index",       /* label        */
	RT_EDIT_PARAM_INTEGER, /* type        */
	0,                    /* index        */
	0.0,                  /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"none",               /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

/* ECMD_COMB_SET_OP: member index + new boolean op */
static const struct rt_edit_param_desc comb_set_op_params[] = {
    {
	"member_index",       /* name         */
	"Member Index",       /* label        */
	RT_EDIT_PARAM_INTEGER, /* type        */
	0,                    /* index        */
	0.0,                  /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"none",               /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    },
    {
	"op",                 /* name         */
	"Boolean Operation",  /* label        */
	RT_EDIT_PARAM_ENUM,   /* type         */
	1,                    /* index        */
	RT_EDIT_PARAM_NO_LIMIT, /* range_min  */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	NULL,                 /* units        */
	3,                    /* nenum        */
	comb_bool_op_labels,  /* enum_labels  */
	comb_bool_op_ids,     /* enum_ids     */
	NULL                  /* prim_field   */
    }
};

/* ECMD_COMB_SET_REGION: is-region flag */
static const struct rt_edit_param_desc comb_set_region_params[] = {
    {
	"region",             /* name         */
	"Is Region",          /* label        */
	RT_EDIT_PARAM_BOOLEAN, /* type        */
	0,                    /* index        */
	RT_EDIT_PARAM_NO_LIMIT, /* range_min  */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	NULL,                 /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

/* ECMD_COMB_SET_COLOR: RGB color */
static const struct rt_edit_param_desc comb_set_color_params[] = {
    {
	"color",              /* name         */
	"Color (RGB)",        /* label        */
	RT_EDIT_PARAM_COLOR,  /* type         */
	0,                    /* index        */
	RT_EDIT_PARAM_NO_LIMIT, /* range_min  */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	NULL,                 /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

/* ECMD_COMB_SET_SHADER: shader string */
static const struct rt_edit_param_desc comb_set_shader_params[] = {
    {
	"shader",             /* name         */
	"Shader",             /* label        */
	RT_EDIT_PARAM_STRING, /* type         */
	0,                    /* index (unused for STRING) */
	RT_EDIT_PARAM_NO_LIMIT, /* range_min  */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	NULL,                 /* units        */
	0, NULL, NULL,        /* enum (unused) */
	"shader"              /* prim_field   */
    }
};

/* ECMD_COMB_SET_MATERIAL: material string */
static const struct rt_edit_param_desc comb_set_material_params[] = {
    {
	"material",           /* name         */
	"Material",           /* label        */
	RT_EDIT_PARAM_STRING, /* type         */
	0,                    /* index (unused for STRING) */
	RT_EDIT_PARAM_NO_LIMIT, /* range_min  */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	NULL,                 /* units        */
	0, NULL, NULL,        /* enum (unused) */
	"material"            /* prim_field   */
    }
};

/* ECMD_COMB_SET_REGION_ID: region id */
static const struct rt_edit_param_desc comb_set_region_id_params[] = {
    {
	"region_id",          /* name         */
	"Region ID",          /* label        */
	RT_EDIT_PARAM_INTEGER, /* type        */
	0,                    /* index        */
	0.0,                  /* range_min    */
	65535.0,              /* range_max    */
	"none",               /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

/* ECMD_COMB_SET_AIRCODE: aircode */
static const struct rt_edit_param_desc comb_set_aircode_params[] = {
    {
	"aircode",            /* name         */
	"Air Code",           /* label        */
	RT_EDIT_PARAM_INTEGER, /* type        */
	0,                    /* index        */
	0.0,                  /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"none",               /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

/* ECMD_COMB_SET_GIFTMATER: GIFTmater code */
static const struct rt_edit_param_desc comb_set_giftmater_params[] = {
    {
	"giftmater",          /* name         */
	"GIFTmater",          /* label        */
	RT_EDIT_PARAM_INTEGER, /* type        */
	0,                    /* index        */
	0.0,                  /* range_min    */
	RT_EDIT_PARAM_NO_LIMIT, /* range_max  */
	"none",               /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

/* ECMD_COMB_SET_LOS: line-of-sight percentage */
static const struct rt_edit_param_desc comb_set_los_params[] = {
    {
	"los",                /* name         */
	"LOS (%)",            /* label        */
	RT_EDIT_PARAM_INTEGER, /* type        */
	0,                    /* index        */
	0.0,                  /* range_min    */
	100.0,                /* range_max    */
	"fraction",           /* units        */
	0, NULL, NULL,        /* enum (unused) */
	NULL                  /* prim_field   */
    }
};

static const struct rt_edit_cmd_desc comb_cmds[] = {
    {
	ECMD_COMB_ADD_MEMBER,     /* cmd_id       */
	RT_EDIT_CMD_NAME(ECMD_COMB_ADD_MEMBER),     /* name       */
	"Add Member",             /* label        */
	"tree",                   /* category     */
	2,                        /* nparam       */
	comb_add_member_params,   /* params       */
	0,                        /* interactive  */
	10                        /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_COMB_DEL_MEMBER,     /* cmd_id       */
	RT_EDIT_CMD_NAME(ECMD_COMB_DEL_MEMBER),     /* name       */
	"Delete Member",          /* label        */
	"tree",                   /* category     */
	1,                        /* nparam       */
	comb_del_member_params,   /* params       */
	0,                        /* interactive  */
	20                        /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_COMB_SET_OP,         /* cmd_id       */
	RT_EDIT_CMD_NAME(ECMD_COMB_SET_OP),         /* name       */
	"Set Boolean Op",         /* label        */
	"tree",                   /* category     */
	2,                        /* nparam       */
	comb_set_op_params,       /* params       */
	0,                        /* interactive  */
	30                        /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_COMB_SET_REGION,     /* cmd_id       */
	RT_EDIT_CMD_NAME(ECMD_COMB_SET_REGION),     /* name       */
	"Set Region Flag",        /* label        */
	"material",               /* category     */
	1,                        /* nparam       */
	comb_set_region_params,   /* params       */
	0,                        /* interactive  */
	10                        /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_COMB_SET_COLOR,      /* cmd_id       */
	RT_EDIT_CMD_NAME(ECMD_COMB_SET_COLOR),      /* name       */
	"Set Color",              /* label        */
	"material",               /* category     */
	1,                        /* nparam       */
	comb_set_color_params,    /* params       */
	0,                        /* interactive  */
	20                        /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_COMB_SET_SHADER,     /* cmd_id       */
	RT_EDIT_CMD_NAME(ECMD_COMB_SET_SHADER),     /* name       */
	"Set Shader",             /* label        */
	"material",               /* category     */
	1,                        /* nparam       */
	comb_set_shader_params,   /* params       */
	0,                        /* interactive  */
	30                        /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_COMB_SET_MATERIAL,   /* cmd_id       */
	RT_EDIT_CMD_NAME(ECMD_COMB_SET_MATERIAL),   /* name       */
	"Set Material",           /* label        */
	"material",               /* category     */
	1,                        /* nparam       */
	comb_set_material_params, /* params       */
	0,                        /* interactive  */
	40                        /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_COMB_SET_REGION_ID,  /* cmd_id       */
	RT_EDIT_CMD_NAME(ECMD_COMB_SET_REGION_ID),  /* name       */
	"Set Region ID",          /* label        */
	"material",               /* category     */
	1,                        /* nparam       */
	comb_set_region_id_params, /* params      */
	0,                        /* interactive  */
	50                        /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_COMB_SET_AIRCODE,    /* cmd_id       */
	RT_EDIT_CMD_NAME(ECMD_COMB_SET_AIRCODE),    /* name       */
	"Set Aircode",            /* label        */
	"material",               /* category     */
	1,                        /* nparam       */
	comb_set_aircode_params,  /* params       */
	0,                        /* interactive  */
	60                        /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_COMB_SET_GIFTMATER,  /* cmd_id       */
	RT_EDIT_CMD_NAME(ECMD_COMB_SET_GIFTMATER),  /* name       */
	"Set GIFTmater",          /* label        */
	"material",               /* category     */
	1,                        /* nparam       */
	comb_set_giftmater_params, /* params      */
	0,                        /* interactive  */
	70                        /* display_order */,
	NULL                  /* req_types */
    },
    {
	ECMD_COMB_SET_LOS,        /* cmd_id       */
	RT_EDIT_CMD_NAME(ECMD_COMB_SET_LOS),        /* name       */
	"Set LOS",                /* label        */
	"material",               /* category     */
	1,                        /* nparam       */
	comb_set_los_params,      /* params       */
	0,                        /* interactive  */
	80                        /* display_order */,
	NULL                  /* req_types */
    }
};

static const struct rt_edit_prim_desc comb_prim_desc = {
    "comb",               /* prim_type    */
    "Combination / Region", /* prim_label */
    11,                   /* ncmd         */
    comb_cmds             /* cmds         */,
    0,                    /* nopt         */
    NULL                  /* opts         */
};

C_DECL const struct rt_edit_prim_desc *
rt_edit_comb_edit_desc(void)
{
    return &comb_prim_desc;
}

C_DECL int
rt_edit_comb_get_values(struct rt_edit *s, int cmd_id,
	struct rt_edit_cmd_values *result)
{
    if (!s || !result || !s->es_int.idb_ptr)
	return RT_EDIT_VALUE_ERROR;
    struct rt_comb_internal *comb =
	(struct rt_comb_internal *)s->es_int.idb_ptr;
    RT_CK_COMB(comb);

    switch (cmd_id) {
	case ECMD_COMB_SET_REGION:
	    rt_edit_cmd_values_set_value(result, 0,
		comb->region_flag ? 1.0 : 0.0);
	    return RT_EDIT_VALUE_OK;
	case ECMD_COMB_SET_COLOR:
	    if (comb->rgb_valid) {
		for (int i = 0; i < ELEMENTS_PER_VECT; ++i)
		    rt_edit_cmd_values_set_value(result, i,
			(fastf_t)comb->rgb[i]);
	    }
	    return RT_EDIT_VALUE_OK;
	case ECMD_COMB_SET_SHADER:
	    rt_edit_cmd_values_set_string(result, 0,
		bu_vls_cstr(&comb->shader));
	    return RT_EDIT_VALUE_OK;
	case ECMD_COMB_SET_MATERIAL:
	    rt_edit_cmd_values_set_string(result, 0,
		bu_vls_cstr(&comb->material));
	    return RT_EDIT_VALUE_OK;
	case ECMD_COMB_SET_REGION_ID:
	    rt_edit_cmd_values_set_value(result, 0,
		(fastf_t)comb->region_id);
	    return RT_EDIT_VALUE_OK;
	case ECMD_COMB_SET_AIRCODE:
	    rt_edit_cmd_values_set_value(result, 0,
		(fastf_t)comb->aircode);
	    return RT_EDIT_VALUE_OK;
	case ECMD_COMB_SET_GIFTMATER:
	    rt_edit_cmd_values_set_value(result, 0,
		(fastf_t)comb->GIFTmater);
	    return RT_EDIT_VALUE_OK;
	case ECMD_COMB_SET_LOS:
	    rt_edit_cmd_values_set_value(result, 0, (fastf_t)comb->los);
	    return RT_EDIT_VALUE_OK;
	default:
	    return RT_EDIT_VALUE_UNAVAILABLE;
    }
}

C_DECL int
rt_edit_comb_edit(struct rt_edit *s)
{
    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	    return edit_sscale(s);
	case RT_PARAMS_EDIT_TRANS:
	    edit_stra(s);
	    break;
	case RT_PARAMS_EDIT_ROT:
	    edit_srot(s);
	    break;
	case ECMD_COMB_ADD_MEMBER:
	    return ecmd_comb_add_member(s);
	case ECMD_COMB_DEL_MEMBER:
	    return ecmd_comb_del_member(s);
	case ECMD_COMB_SET_OP:
	    return ecmd_comb_set_op(s);
	case ECMD_COMB_SET_MATRIX:
	    return ecmd_comb_set_matrix(s);
	case ECMD_COMB_SET_REGION:
	    return ecmd_comb_set_region(s);
	case ECMD_COMB_SET_COLOR:
	    return ecmd_comb_set_color(s);
	case ECMD_COMB_SET_SHADER:
	    return ecmd_comb_set_shader(s);
	case ECMD_COMB_SET_MATERIAL:
	    return ecmd_comb_set_material(s);
	case ECMD_COMB_SET_REGION_ID:
	    return ecmd_comb_set_region_id(s);
	case ECMD_COMB_SET_AIRCODE:
	    return ecmd_comb_set_aircode(s);
	case ECMD_COMB_SET_GIFTMATER:
	    return ecmd_comb_set_giftmater(s);
	case ECMD_COMB_SET_LOS:
	    return ecmd_comb_set_los(s);
	default:
	    return edit_generic(s);
    }

    return 0;
}

C_DECL int
rt_edit_comb_edit_xy(
	struct rt_edit *s,
	const vect_t mousevec
	)
{
    vect_t pos_view = VINIT_ZERO;

    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	case ECMD_COMB_DEL_MEMBER:
	case ECMD_COMB_SET_OP:
	case ECMD_COMB_SET_REGION:
	case ECMD_COMB_SET_COLOR:
	case ECMD_COMB_SET_SHADER:
	case ECMD_COMB_SET_MATERIAL:
	case ECMD_COMB_SET_REGION_ID:
	case ECMD_COMB_SET_AIRCODE:
	case ECMD_COMB_SET_GIFTMATER:
	case ECMD_COMB_SET_LOS:
	    edit_sscale_xy(s, mousevec);
	    return 0;
	case RT_PARAMS_EDIT_TRANS:
	case ECMD_COMB_ADD_MEMBER:
	case ECMD_COMB_SET_MATRIX:
	    edit_stra_xy(&pos_view, s, mousevec);
	    break;
	default:
	    return edit_generic_xy(s, mousevec);
    }

    edit_abs_tra(s, pos_view);

    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
