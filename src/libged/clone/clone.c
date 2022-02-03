/*                         C L O N E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file libged/clone.c
 *
 * The clone command.  Performs a deep copy of an object.
 *
 * ISSUES/TODO:
 *
 * * Fix bug when number is at beginning of a name, replaces with a
 *   number, sometimes crashes.
 *
 * * use libbu strings/lists
 *
 * * No -c option.  This allows the increment given in the '-i' to act
 *   on the second number
 *
 * * No -p option.  I couldn't get this to work.  I re-centered the
 *   geometry, then it tried to work but I ran into the 15 char limit
 *   and had to kill the process (^C).
 *
 * * Names - This tool is built around a naming convention.
 *   Currently, the second number does not list properly (it just
 *   truncated the second number of the 'cut' prims so they ended up
 *   'mess.s1.c' instead of 'mess.s1.c1').  And the '+' and '-' didn't
 *   work, I had to switch from 'mess.s1-1' to 'mess.s1.c1'.  Also,
 *   prims need to increment by the 'i' number but combs, regions, and
 *   assemblies (.c#, .r#, or just name with a # at the end) should
 *   increment by 1.  So you end up with widget_1, widget_2, widget_3
 *   and not widget_1, widget_4, widget_7...
 *
 * * Tree structure - retain tree structure to the extent that we can
 *   and try not to re-create prims or combs used more than once.  No
 *   warning needed for redundant copies.  Warnings can come later...
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#include "bu/getopt.h"
#include "vmath.h"
#include "rt/db4.h"
#include "raytrace.h"

#include "../ged_private.h"


#define CLONE_BUFPARTSIZE 511
#define CLONE_BUFSIZE 512


/**
 * state structure used to keep track of what actions the user
 * requested and values necessary to perform the cloning operation.
 */
struct ged_clone_state {
    struct ged *gedp;
    struct directory *src;	/* Source object */
    int incr;			/* Amount to increment between copies */
    size_t n_copies;		/* Number of copies to make */
    hvect_t trans;		/* Translation between copies */
    hvect_t rot;		/* Rotation between copies */
    hvect_t rpnt;		/* Point to rotate about (default 0 0 0) */
    int miraxis;		/* Axis to mirror copy */
    fastf_t mirpos;		/* Point on axis to mirror copy */
    int updpos;			/* Position of number to update (for -c) */
    struct bu_vls olist;        /* List of cloned object names */
};
#define GED_CLONE_STATE_INIT {NULL, NULL, 0, 0, HINIT_ZERO, HINIT_ZERO, HINIT_ZERO, 0, 0.0, 0, BU_VLS_INIT_ZERO}

struct name {
    struct bu_vls src;		/* source object name */
    struct bu_vls *dest;	/* dest object names */
};


/**
 * structure used to store the names of objects that are to be cloned.
 * space is preallocated via names with len and used keeping track of
 * space available and used.
 */
struct nametbl {
    struct name *names;
    int name_size;
    int names_len;
    int names_used;
};


static struct nametbl obj_list;

/**
 * a polynomial value for representing knots
 */
struct knot {
    vect_t pt;
    fastf_t c[3][4];
};


/**
 * a spline path with various segments, break points, and polynomial
 * values.
 */
struct spline {
    int n_segs;
    fastf_t *t; /* break points */
    struct knot *k; /* polynomials */
};


struct link {
    struct bu_vls name;
    fastf_t len;
    fastf_t pct;
};


/**
 * initialize the name list used for stashing destination names
 */
static void
init_list(struct nametbl *l, int s)
{
    int i, j;

    l->names = (struct name *)bu_calloc(10, sizeof(struct name), "alloc l->names");
    for (i = 0; i < 10; i++) {
	bu_vls_init(&l->names[i].src);
	l->names[i].dest = (struct bu_vls *)bu_calloc(s, sizeof(struct bu_vls), "alloc l->names.dest");
	for (j = 0; j < s; j++)
	    bu_vls_init(&l->names[i].dest[j]);
    }
    l->name_size = s;
    l->names_len = 10;
    l->names_used = 0;
}


/**
 * add a new name to the name list
 */
static int
add_to_list(struct nametbl *l, char *name)
{
    int i, j;

    /*
     * add more slots if adding 1 more new name will fill up all the
     * available slots.
     */
    if (l->names_len == (l->names_used+1)) {
	l->names_len += 10;
	l->names = (struct name *)bu_realloc(l->names, sizeof(struct name)*(l->names_len+1), "realloc l->names");
	for (i = l->names_used; i < l->names_len; i++) {
	    bu_vls_init(&l->names[i].src);
	    l->names[i].dest = (struct bu_vls *)bu_calloc(l->name_size, sizeof(struct bu_vls), "alloc l->names.dest");
	    for (j = 0; j < l->name_size; j++)
		bu_vls_init(&l->names[i].dest[j]);
	}
    }
    bu_vls_strcpy(&l->names[l->names_used++].src, name);
    return l->names_used-1; /* return number of available slots */
}


/**
 * returns the location of 'name' in the list if it exists, returns -1
 * otherwise.
 */
static int
index_in_list(struct nametbl l, char *name)
{
    int i;

    for (i = 0; i < l.names_used; i++)
	if (BU_STR_EQUAL(bu_vls_addr(&l.names[i].src), name))
	    return i;
    return -1;
}


/**
 * returns truthfully if 'name' exists in the list
 */
static int
is_in_list(struct nametbl l, char *name)
{
    return index_in_list(l, name) != -1;
}


/**
 * returns the next available/unused name, using a consistent naming
 * convention specific to combinations/regions and solids.
 * state->incr is used for each number level increase.
 */
static struct bu_vls *
clone_get_name(struct directory *dp, struct ged_clone_state *state, size_t iter)
{
    struct bu_vls *newname;
    char prefix[CLONE_BUFPARTSIZE] = {0}, suffix[CLONE_BUFPARTSIZE] = {0}, buf[CLONE_BUFSIZE] = {0}, suffix2[CLONE_BUFPARTSIZE] = {0};
    int num = 0, i = 1, j = 0;

    newname = bu_vls_vlsinit();

    /* Ugh. This needs much repair/cleanup. */
    if (state->updpos == 0) {
	struct bu_vls tmpbuf = BU_VLS_INIT_ZERO;
	sscanf(dp->d_namep, "%[!-/, :-~]%d%[!-/, :-~]%510s", prefix, &num, suffix, suffix2); /* CLONE_BUFPARTSIZE - 1 */
	bu_vls_sprintf(&tmpbuf, "%s%s", suffix, suffix2);
	snprintf(suffix, sizeof(suffix), "%s", bu_vls_addr(&tmpbuf));
	bu_vls_free(&tmpbuf);
    } else if (state->updpos == 1) {
	int num2 = 0;
	struct bu_vls tmpbuf = BU_VLS_INIT_ZERO;
	sscanf(dp->d_namep, "%[!-/, :-~]%d%[!-/, :-~]%d%[!-/, :-~]", prefix, &num2, suffix2, &num, suffix);
	if (num > 0) {
	    bu_vls_sprintf(&tmpbuf, "%s%d%s", prefix, num2, suffix2);
	} else {
	    num = num2;
	    bu_vls_sprintf(&tmpbuf, "%s%s", prefix, suffix2);
	}
	snprintf(prefix, sizeof(prefix), "%s", bu_vls_addr(&tmpbuf));
	bu_vls_free(&tmpbuf);
    } else
	bu_exit(EXIT_FAILURE, "multiple -c options not supported yet.");

    do {
	/* choke the name back to the prefix */
	bu_vls_trunc(newname, 0);
	bu_vls_strcpy(newname, prefix);

	if ((dp->d_flags & RT_DIR_SOLID) || (dp->d_flags & RT_DIR_REGION)) {
	    /* primitives and regions */
	    if (suffix[0] != 0)
		if ((i == 1) && is_in_list(obj_list, buf)) {
		    j = index_in_list(obj_list, buf);
		    snprintf(buf, CLONE_BUFSIZE, "%s%d", prefix, num);	/* save the name for the next pass */
		    /* clear and set the name */
		    bu_vls_trunc(newname, 0);
		    bu_vls_printf(newname, "%s%s", bu_vls_addr(&(obj_list.names[j].dest[iter])), suffix);
		} else
		    bu_vls_printf(newname, "%d%s", num+i*state->incr, suffix);
	    else
		bu_vls_printf(newname, "%d", num + i*state->incr);
	} else /* non-region combinations */
	    bu_vls_printf(newname, "%d", (num == 0) ? i + 1 : i + num);
	i++;
    } while (db_lookup(state->gedp->dbip, bu_vls_addr(newname), LOOKUP_QUIET) != NULL);
    return newname;
}


/**
 * make a copy of a v4 solid by adding it to our book-keeping list,
 * adding it to the db directory, and writing it out to disk.
 */
static void
copy_v4_solid(struct db_i *dbip, struct directory *proto, struct ged_clone_state *state, int idx)
{
    struct directory *dp = (struct directory *)NULL;
    union record *rp = (union record *)NULL;
    size_t i, j;

    /* make n copies */
    for (i = 0; i < state->n_copies; i++) {
	struct bu_vls *name;

	if (i == 0)
	    name = clone_get_name(proto, state, i);
	else {
	    dp = db_lookup(dbip, bu_vls_addr(&obj_list.names[idx].dest[i-1]), LOOKUP_QUIET);
	    if (!dp) {
		continue;
	    }
	    name = clone_get_name(dp, state, i);
	}

	/* XXX: this can probably be optimized. */
	bu_vls_strcpy(&obj_list.names[idx].dest[i], bu_vls_addr(name));
	bu_vls_free(name);

	/* add the object to the directory */
	dp = db_diradd(dbip, bu_vls_addr(&obj_list.names[idx].dest[i]), RT_DIR_PHONY_ADDR, proto->d_len, proto->d_flags, &proto->d_minor_type);
	if ((dp == RT_DIR_NULL) || db_alloc(dbip, dp, proto->d_len)) {
	    bu_vls_printf(state->gedp->ged_result_str, "Database alloc error, aborting\n");
	    return;
	}

	/* get an in-memory reference to the object being copied */
	if ((rp = db_getmrec(dbip, proto)) == (union record *)0) {
	    bu_vls_printf(state->gedp->ged_result_str, "Database read error, aborting\n");
	    return;
	}

	if (rp->u_id == ID_SOLID) {
	    bu_strlcpy(rp->s.s_name, dp->d_namep, NAMESIZE);

	    /* mirror */
	    if (state->miraxis != W) {
		/* XXX er, this seems rather wrong .. but it's v4 so punt */
		rp->s.s_values[state->miraxis] += 2 * (state->mirpos - rp->s.s_values[state->miraxis]);
		for (j = 3+state->miraxis; j < 24; j++)
		    rp->s.s_values[j] = -rp->s.s_values[j];
	    }
	    /* translate */
	    if (!ZERO(state->trans[W]))
		/* assumes primitive's first parameter is its position */
		VADD2(rp->s.s_values, rp->s.s_values, state->trans);
	    /* rotate */
	    if (!ZERO(state->rot[W])) {
		mat_t r;
		vect_t vec, ovec;

		if (!ZERO(state->rpnt[W]))
		    VSUB2(rp->s.s_values, rp->s.s_values, state->rpnt);
		MAT_IDN(r);
		bn_mat_angles(r, state->rot[X], state->rot[Y], state->rot[Z]);
		for (j = 0; j < 24; j+=3) {
		    VMOVE(vec, rp->s.s_values+j);
		    MAT4X3VEC(ovec, r, vec);
		    VMOVE(rp->s.s_values+j, ovec);
		}
		if (!ZERO(state->rpnt[W]))
		    VADD2(rp->s.s_values, rp->s.s_values, state->rpnt);
	    }
	} else
	    bu_vls_printf(state->gedp->ged_result_str, "mods not available on %s\n", proto->d_namep);

	/* write the object to disk */
	if (db_put(dbip, dp, rp, 0, dp->d_len)) {
	    bu_vls_printf(state->gedp->ged_result_str, "ERROR: clone internal error writing to the database\n");
	    return;
	}
    }
    if (rp)
	bu_free((char *)rp, "copy_solid record[]");

    return;
}


/**
 * make a copy of a v5 solid by adding it to our book-keeping list,
 * adding it to the db directory, and writing it out to disk.
 */
static void
copy_v5_solid(struct db_i *dbip, struct directory *proto, struct ged_clone_state *state, int idx)
{
    size_t i;
    mat_t matrix;

    MAT_IDN(matrix);

    /* mirror */
    if (state->miraxis != W) {
	matrix[state->miraxis*5] = -1.0;
	matrix[3 + state->miraxis*4] -= 2 * (matrix[3 + state->miraxis*4] - state->mirpos);
    }

    /* translate */
    if (!ZERO(state->trans[W]))
	MAT_DELTAS_ADD_VEC(matrix, state->trans);

    /* rotation */
    if (!ZERO(state->rot[W])) {
	mat_t m2, t;

	bn_mat_angles(m2, state->rot[X], state->rot[Y], state->rot[Z]);
	if (!ZERO(state->rpnt[W])) {
	    mat_t m3;

	    bn_mat_xform_about_pnt(m3, m2, state->rpnt);
	    bn_mat_mul(t, matrix, m3);
	} else
	    bn_mat_mul(t, matrix, m2);

	MAT_COPY(matrix, t);
    }

    /* make n copies */
    for (i = 0; i < state->n_copies; i++) {
	char *argv[4];
	struct bu_vls *name;
	int ret;
	struct directory *dp = (struct directory *)NULL;
	struct rt_db_internal intern;

	if (i == 0)
	    dp = proto;
	else
	    dp = db_lookup(dbip, bu_vls_addr(&obj_list.names[idx].dest[i-1]), LOOKUP_QUIET);

	if (!dp) {
	    continue;
	}

	name = clone_get_name(dp, state, i); /* get new name */
	bu_vls_strcpy(&obj_list.names[idx].dest[i], bu_vls_addr(name));

	/* actually copy the primitive to the new name */
	argv[0] = "copy";
	argv[1] = proto->d_namep;
	argv[2] = bu_vls_addr(name);
	argv[3] = (char *)0;
	ret = ged_copy(state->gedp, 3, (const char **)argv);
	if (ret != BRLCAD_OK)
	    bu_vls_printf(state->gedp->ged_result_str, "WARNING: failure cloning \"%s\" to \"%s\"\n",
			  proto->d_namep, bu_vls_addr(name));

	/* get the original objects matrix */
	if (rt_db_get_internal(&intern, dp, dbip, matrix, &rt_uniresource) < 0) {
	    bu_vls_printf(state->gedp->ged_result_str, "ERROR: clone internal error copying %s\n", proto->d_namep);
	    bu_vls_free(name);
	    return;
	}
	RT_CK_DB_INTERNAL(&intern);
	/* pull the new name */
	dp = db_lookup(dbip, bu_vls_addr(name), LOOKUP_QUIET);
	if (!dp) {
	    rt_db_free_internal(&intern);
	    bu_vls_free(name);
	    continue;
	}

	/* write the new matrix to the new object */
	if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0)
	    bu_vls_printf(state->gedp->ged_result_str, "ERROR: clone internal error copying %s\n", proto->d_namep);

	bu_vls_printf(&state->olist, "%s ", bu_vls_addr(name));
	bu_vls_free(name);
    } /* end make n copies */

    return;
}


/**
 * make n copies of a database combination by adding it to our
 * book-keeping list, adding it to the directory, then writing it out
 * to the db.
 */
static void
copy_solid(struct db_i *dbip, struct directory *proto, void *clientData)
{
    struct ged_clone_state *state = (struct ged_clone_state *)clientData;
    int idx;

    if (is_in_list(obj_list, proto->d_namep)) {
	bu_vls_printf(state->gedp->ged_result_str, "Solid primitive %s already cloned?\n", proto->d_namep);
	return;
    }

    idx = add_to_list(&obj_list, proto->d_namep);

    /* sanity check that the item was really added */
    if ((idx < 0) || !is_in_list(obj_list, proto->d_namep)) {
	bu_vls_printf(state->gedp->ged_result_str, "ERROR: clone internal error copying %s\n", proto->d_namep);
	return;
    }

    if (db_version(dbip) < 5)
	(void)copy_v4_solid(dbip, proto, (struct ged_clone_state *)state, idx);
    else
	(void)copy_v5_solid(dbip, proto, (struct ged_clone_state *)state, idx);
    return;
}


/**
 * make n copies of a v4 combination.
 */
static struct directory *
copy_v4_comb(struct db_i *dbip, struct directory *proto, struct ged_clone_state *state, int idx)
{
    struct directory *dp = (struct directory *)NULL;
    union record *rp = (union record *)NULL;
    size_t i, j;

    /* make n copies */
    for (i = 0; i < (size_t)state->n_copies; i++) {

	/* get a v4 in-memory reference to the object being copied */
	if ((rp = db_getmrec(dbip, proto)) == (union record *)0) {
	    bu_vls_printf(state->gedp->ged_result_str, "Database read error, aborting\n");
	    return NULL;
	}

	if (proto->d_flags & RT_DIR_REGION) {
	    if (!is_in_list(obj_list, rp[1].M.m_instname)) {
		bu_vls_printf(state->gedp->ged_result_str, "ERROR: clone internal error looking up %s\n", rp[1].M.m_instname);
		return NULL;
	    }
	    bu_vls_strcpy(&obj_list.names[idx].dest[i], bu_vls_addr(&obj_list.names[index_in_list(obj_list, rp[1].M.m_instname)].dest[i]));
	    /* bleh, odd convention going on here.. prefix regions with an 'r' */
	    *bu_vls_addr(&obj_list.names[idx].dest[i]) = 'r';
	} else {
	    struct bu_vls *name;
	    if (i == 0)
		name = clone_get_name(proto, state, i);
	    else {
		dp = db_lookup(dbip, bu_vls_addr(&obj_list.names[idx].dest[i-1]), LOOKUP_QUIET);
		if (!dp) {
		    continue;
		}
		name = clone_get_name(dp, state, i);
	    }
	    bu_vls_strcpy(&obj_list.names[idx].dest[i], bu_vls_addr(name));
	    bu_vls_free(name);
	}
	bu_strlcpy(rp[0].c.c_name, bu_vls_addr(&obj_list.names[idx].dest[i]), NAMESIZE);

	/* add the object to the directory */
	dp = db_diradd(dbip, rp->c.c_name, RT_DIR_PHONY_ADDR, proto->d_len, proto->d_flags, &proto->d_minor_type);
	if ((dp == NULL) || db_alloc(dbip, dp, proto->d_len)) {
	    bu_vls_printf(state->gedp->ged_result_str, "Database alloc error, aborting\n");
	    return NULL;
	}

	for (j = 1; j < proto->d_len; j++) {
	    struct bu_vls *vp;
	    if (!is_in_list(obj_list, rp[j].M.m_instname)) {
		bu_vls_printf(state->gedp->ged_result_str, "ERROR: clone internal error looking up %s\n", rp[j].M.m_instname);
		return NULL;
	    }
	    vp = &obj_list.names[index_in_list(obj_list, rp[j].M.m_instname)].dest[i];
	    snprintf(rp[j].M.m_instname, NAMESIZE, "%s", bu_vls_addr(vp));
	}

	/* write the object to disk */
	if (db_put(dbip, dp, rp, 0, dp->d_len)) {
	    bu_vls_printf(state->gedp->ged_result_str, "ERROR: clone internal error writing to the database\n");
	    return NULL;
	}

	/* our responsibility to free the record */
	bu_free((char *)rp, "deallocate copy_v4_comb() db_getmrec() record");
    }

    return dp;
}


/*
 * update the v5 combination tree with the new names.
 *
 * DESTRUCTIVE RECURSIVE
 */
static int
copy_v5_comb_tree(struct ged_clone_state *state, union tree *tree, size_t idx)
{
    char *buf;
    switch (tree->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    /* copy right */
	    copy_v5_comb_tree(state, tree->tr_b.tb_right, idx);

	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    /* copy left */
	    copy_v5_comb_tree(state, tree->tr_b.tb_left, idx);
	    break;
	case OP_DB_LEAF:
	    buf = tree->tr_l.tl_name;
	    tree->tr_l.tl_name = bu_strdup(bu_vls_addr(&obj_list.names[index_in_list(obj_list, buf)].dest[idx]));
	    bu_free(buf, "node name");
	    break;
	default:
	    bu_vls_printf(state->gedp->ged_result_str, "clone v5 - OPCODE NOT IMPLEMENTED: %d\n", tree->tr_op);
	    return -1;
    }
    return 0;
}


/**
 * make n copies of a v5 combination.
 */
static struct directory *
copy_v5_comb(struct db_i *dbip, struct directory *proto, struct ged_clone_state *state, size_t idx)
{
    struct directory *dp = (struct directory *)NULL;
    struct bu_vls *name;
    size_t i;

    /* sanity */
    if (!proto) {
	bu_vls_printf(state->gedp->ged_result_str, "ERROR: clone internal consistency error\n");
	return (struct directory *)NULL;
    }

    /* make n copies */
    for (i = 0; i < state->n_copies; i++) {
	if (i == 0)
	    name = clone_get_name(proto, state, i);
	else {
	    dp = db_lookup(dbip, bu_vls_addr(&obj_list.names[idx].dest[i-1]), LOOKUP_QUIET);
	    if (!dp) {
		continue;
	    }
	    name = clone_get_name(dp, state, i);
	}
	bu_vls_strcpy(&obj_list.names[idx].dest[i], bu_vls_addr(name));

	/* we have a before and an after, do the copy */
	if (proto->d_namep && bu_vls_addr(name)) {
	    struct rt_db_internal dbintern;
	    struct rt_comb_internal *comb;

	    dp = db_lookup(dbip, proto->d_namep, LOOKUP_QUIET);
	    if (!dp) {
		bu_vls_free(name);
		continue;
	    }
	    if (rt_db_get_internal(&dbintern, dp, dbip, bn_mat_identity, &rt_uniresource) < 0) {
		bu_vls_printf(state->gedp->ged_result_str, "ERROR: clone internal error copying %s\n", proto->d_namep);
		return NULL;
	    }

	    dp = db_diradd(dbip, bu_vls_addr(name), RT_DIR_PHONY_ADDR, 0, proto->d_flags, (void *)&proto->d_minor_type);
	    if (dp == RT_DIR_NULL) {
		bu_vls_printf(state->gedp->ged_result_str, "An error has occurred while adding a new object to the database.");
		return NULL;
	    }

	    RT_CK_DB_INTERNAL(&dbintern);
	    comb = (struct rt_comb_internal *)dbintern.idb_ptr;
	    RT_CK_COMB(comb);
	    RT_CK_TREE(comb->tree);

	    /* recursively update the tree */
	    copy_v5_comb_tree(state, comb->tree, i);

	    if (rt_db_put_internal(dp, dbip, &dbintern, &rt_uniresource) < 0) {
		bu_vls_printf(state->gedp->ged_result_str, "ERROR: clone internal error copying %s\n", proto->d_namep);
		bu_vls_free(name);
		return NULL;
	    }
	    bu_vls_printf(&state->olist, "%s ", bu_vls_addr(name));
	    bu_vls_free(name);
	    rt_db_free_internal(&dbintern);
	}

	/* done with this name */
	bu_vls_free(name);
    }

    return dp;
}


/**
 * make n copies of a database combination by adding it to our
 * book-keeping list, adding it to the directory, then writing it out
 * to the db.
 */
static void
copy_comb(struct db_i *dbip, struct directory *proto, void *clientData)
{
    struct ged_clone_state *state = (struct ged_clone_state *)clientData;
    int idx;

    if (is_in_list(obj_list, proto->d_namep)) {
	bu_vls_printf(state->gedp->ged_result_str, "Combination %s already cloned?\n", proto->d_namep);
	return;
    }

    idx = add_to_list(&obj_list, proto->d_namep);

    /* sanity check that the item was really added to our bookkeeping */
    if ((idx < 0) || !is_in_list(obj_list, proto->d_namep)) {
	bu_vls_printf(state->gedp->ged_result_str, "ERROR: clone internal error copying %s\n", proto->d_namep);
	return;
    }

    if (db_version(dbip) < 5)
	(void)copy_v4_comb(dbip, proto, (struct ged_clone_state *)state, idx);
    else
	(void)copy_v5_comb(dbip, proto, (struct ged_clone_state *)state, idx);

    return;
}


/**
 * recursively copy a tree of geometry
 */
static struct directory *
copy_tree(struct directory *dp, struct resource *resp, struct ged_clone_state *state)
{
    size_t i;
    union record *rp = (union record *)NULL;
    struct directory *mdp = (struct directory *)NULL;
    struct directory *copy = (struct directory *)NULL;

    struct bu_vls *copyname = NULL;
    struct bu_vls *nextname = NULL;

    /* get the name of what the object "should" get cloned to */
    copyname = clone_get_name(dp, state, 0);

    /* copy the object */
    if (dp->d_flags & RT_DIR_COMB) {

	if (db_version(state->gedp->dbip) < 5) {
	    /* A v4 method of peeking into a combination */

	    int errors = 0;

	    /* get an in-memory record of this object */
	    if ((rp = db_getmrec(state->gedp->dbip, dp)) == (union record *)0) {
		bu_vls_printf(state->gedp->ged_result_str, "Database read error, aborting\n");
		goto done_copy_tree;
	    }
	    /*
	     * if it is a combination/region, copy the objects that
	     * make up the object.
	     */
	    for (i = 1; i < dp->d_len; i++) {
		if ((mdp = db_lookup(state->gedp->dbip, rp[i].M.m_instname, LOOKUP_NOISY)) == RT_DIR_NULL) {
		    errors++;
		    bu_vls_printf(state->gedp->ged_result_str, "WARNING: failed to locate \"%s\"\n", rp[i].M.m_instname);
		    continue;
		}
		copy = copy_tree(mdp, resp, state);
		if (!copy) {
		    errors++;
		    bu_vls_printf(state->gedp->ged_result_str, "WARNING: unable to fully clone \"%s\"\n", rp[i].M.m_instname);
		}
	    }

	    if (errors) {
		bu_vls_printf(state->gedp->ged_result_str, "WARNING: some elements of \"%s\" could not be cloned\n", dp->d_namep);
	    }

	    /* copy this combination itself */
	    copy_comb(state->gedp->dbip, dp, (void *)state);
	} else
	    /* A v5 method of peeking into a combination */
	    db_functree(state->gedp->dbip, dp, copy_comb, copy_solid, resp, (void *)state);
    } else if (dp->d_flags & RT_DIR_SOLID)
	/* leaf node -- make a copy the object */
	copy_solid(state->gedp->dbip, dp, (void *)state);
    else {
	bu_vls_printf(state->gedp->ged_result_str, "%s is neither a combination or a primitive?\n", dp->d_namep);
	goto done_copy_tree;
    }

    nextname = clone_get_name(dp, state, 0);
    if (bu_vls_strcmp(copyname, nextname) == 0)
	bu_vls_printf(state->gedp->ged_result_str, "ERROR: unable to successfully clone \"%s\" to \"%s\"\n",
		      dp->d_namep, bu_vls_addr(copyname));
    else
	copy = db_lookup(state->gedp->dbip, bu_vls_addr(copyname), LOOKUP_QUIET);

done_copy_tree:
    if (rp)
	bu_free((char *)rp, "copy_tree record[]");
    if (copyname)
	bu_free((char *)copyname, "free clone_get_name() copyname");
    if (nextname)
	bu_free((char *)nextname, "free clone_get_name() copyname");

    return copy;
}


/**
 * copy an object, recursively copying all of the object's contents if
 * it's a combination/region.
 */
static struct directory *
deep_copy_object(struct resource *resp, struct ged_clone_state *state)
{
    struct directory *copy = (struct directory *)NULL;
    int i, j;

    if (!resp || !state || !state->n_copies) return RT_DIR_NULL;

    init_list(&obj_list, state->n_copies);

    /* do the actual copying */
    copy = copy_tree(state->src, resp, state);

    /* make sure it made what we hope/think it made */
    if (!copy || !is_in_list(obj_list, state->src->d_namep))
	return copy;

    /* release our name allocations */
    for (i = 0; i < obj_list.names_len; i++) {
	for (j = 0; j < obj_list.name_size; j++)
	    bu_vls_free(&obj_list.names[i].dest[j]);
	bu_free((char **)obj_list.names[i].dest, "free dest");
    }
    bu_free((struct name *)obj_list.names, "free names");

    /* better safe than sorry */
    obj_list.names = NULL;
    obj_list.name_size = obj_list.names_used = obj_list.names_len = 0;

    return copy;
}


/**
 * how to use clone.  blissfully simple interface.
 */
static void
print_usage(struct bu_vls *str)
{
    bu_vls_printf(str,\
		  "Usage: clone [-h]\n"
		  "       clone [-i #] [-c] [-n #] [-t x y z] [-r x y z] [-p x y z] {object}\n"
		  "       clone [-i #] [-c] [-a n x y z] [-b n x y z] {object}\n"
		  "       clone [-i #] [-c] [-m [xyz] #] {object}\n"
		  "\n");

    bu_vls_printf(str,
		  "\t-h           - Prints this help message.\n"
		  "\n");

    bu_vls_printf(str,
		  "\t-n #_copies  - Specifies the number of copies to make.\n"
		  "\t               (Default is 1)\n");
    bu_vls_printf(str,
		  "\t-t x y z     - Specifies translation between each copy.\n"
		  "\t               (Default is 0 0 0)\n");
    bu_vls_printf(str,
		  "\t-r x y z     - Specifies rotation around x, y, and z axes.\n"
		  "\t               (Default is 0 0 0)\n");
    bu_vls_printf(str,
		  "\t-p x y z     - Specifies point to rotate around for -r.\n"
		  "\t               (Default is 0 0 0)\n"
		  "\n");

    bu_vls_printf(str,
		  "\t-a n x y z   - Specifies total translation divided over n copies.\n");
    bu_vls_printf(str,
		  "\t-b n x y z   - Specifies total rotation around x, y, & z axes\n"
		  "\t               divided over n copies.\n");
    bu_vls_printf(str,
		  "\t-m [xyz] d   - Specifies x/y/z axis and distance to mirror across.\n"
		  "\n");

    bu_vls_printf(str,
		  "\t-i #_incr    - Specifies increment to use when naming copies.\n"
		  "\t               (Default is 100)\n");
    bu_vls_printf(str,
		  "\t-c           - Increment the next number in object names.\n");

    return;
}


/**
 * process the user-provided arguments. stash their operations into
 * our state structure.
 */
static int
get_args(struct ged *gedp, int argc, char **argv, struct ged_clone_state *state)
{
    int k;

    bu_optind = 1;

    state->gedp = gedp;
    state->incr = 100;
    state->n_copies = 1;
    state->rot[W] = 0;
    state->rpnt[W] = 0;
    state->trans[W] = 0;
    state->miraxis = W;
    state->updpos = 0;

    while ((k = bu_getopt(argc, argv, "a:b:cgi:m:n:p:r:t:h?")) != -1) {

	if (bu_optopt == '?')
	    k = 'h';

	switch (k) {
	    case 'a':
		state->n_copies = atoi(bu_optarg);
		state->trans[X] = atof(argv[bu_optind++]) / state->n_copies;
		state->trans[Y] = atof(argv[bu_optind++]) / state->n_copies;
		state->trans[Z] = atof(argv[bu_optind++]) / state->n_copies;
		state->trans[W] = 1;
		break;
	    case 'b':
		state->n_copies = atoi(bu_optarg);
		state->rot[X] = atof(argv[bu_optind++]) / state->n_copies;
		state->rot[Y] = atof(argv[bu_optind++]) / state->n_copies;
		state->rot[Z] = atof(argv[bu_optind++]) / state->n_copies;
		state->rot[W] = 1;
		break;
	    case 'c':
		/* I'd like to have an optional argument to -c, but
		 * for now, just let multiple -c's add it up as a
		 * hack. I believe the variant of this that was lost
		 * used this as a binary operation, so it SHOULD be
		 * functionally equivalent for a user who's dealt with
		 * this before. */
		state->updpos++;
		break;
	    case 'i':
		state->incr = atoi(bu_optarg);
		break;
	    case 'm':
		state->miraxis = bu_optarg[0] - 'x';
		state->mirpos = atof(argv[bu_optind++]);
		break;
	    case 'n':
		state->n_copies = atoi(bu_optarg);
		break;
	    case 'p':
		state->rpnt[X] = atof(bu_optarg);
		state->rpnt[Y] = atof(argv[bu_optind++]);
		state->rpnt[Z] = atof(argv[bu_optind++]);
		state->rpnt[W] = 1;
		break;
	    case 'r':
		state->rot[X] = atof(bu_optarg);
		state->rot[Y] = atof(argv[bu_optind++]);
		state->rot[Z] = atof(argv[bu_optind++]);
		state->rot[W] = 1;
		break;
	    case 't':
		state->trans[X] = atof(bu_optarg);
		state->trans[Y] = atof(argv[bu_optind++]);
		state->trans[Z] = atof(argv[bu_optind++]);
		state->trans[W] = 1;
		break;
	    default:
		print_usage(gedp->ged_result_str);
		return BRLCAD_ERROR;
	}
    }

    /* make sure not too few/many args */
    if ((argc - bu_optind) == 0) {
	bu_vls_printf(gedp->ged_result_str, "Need to specify an object to be cloned.\n");
	print_usage(gedp->ged_result_str);
	return BRLCAD_ERROR;
    } else if (bu_optind + 1 < argc) {
	bu_vls_printf(gedp->ged_result_str, "clone:  Can only clone exactly one object at a time right now.\n");
	print_usage(gedp->ged_result_str);
	return BRLCAD_ERROR;
    }

    /* sanity */
    if (!argv[bu_optind])
	return BRLCAD_ERROR;

    GED_DB_LOOKUP(gedp, state->src, argv[bu_optind], LOOKUP_QUIET, BRLCAD_ERROR);

    VSCALE(state->trans, state->trans, gedp->dbip->dbi_local2base);
    VSCALE(state->rpnt, state->rpnt, gedp->dbip->dbi_local2base);
    state->mirpos *= gedp->dbip->dbi_local2base;

    return BRLCAD_OK;
}


int
ged_clone_core(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_clone_state state = GED_CLONE_STATE_INIT;
    struct directory *copy;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	print_usage(gedp->ged_result_str);
	return BRLCAD_HELP;
    }

    /* validate user options */
    if (get_args(gedp, argc, (char **)argv, &state) & BRLCAD_ERROR)
	return BRLCAD_ERROR;

    bu_vls_init(&state.olist);

    if ((copy = deep_copy_object(&rt_uniresource, &state)) != (struct directory *)NULL)
	bu_vls_printf(gedp->ged_result_str, "%s", copy->d_namep);

    bu_vls_printf(gedp->ged_result_str, " {%s}", bu_vls_addr(&state.olist));
    bu_vls_free(&state.olist);

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl clone_cmd_impl = {
    "clone",
    ged_clone_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd clone_cmd = { &clone_cmd_impl };
const struct ged_cmd *clone_cmds[] = { &clone_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  clone_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
