/*	                  C L O N E . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2011 United States Government as represented by
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
 */
/** @file mged/clone.c
 *
 * routines related to performing deep object copies
 *
 * TODO:
 *   use bu_vls strings
 *   use bu_list lists
 *
 * ISSUES/TODO (for DK, ^D means done)
 *  1. No -c option.  This allows the increment given in the '-i' to
 *  act on the second number
 * D2. Remove 15 char name limit.  I ran into this today.
 *  3. No -p option.  I couldn't get this to work.  I re-centered the
 *     geometry, then it tried to work but I ran into the 15 char limit
 *     and had to kill the process (^C).
 *  4. Names - This tool is built around a naming convention.  Currently,
 *     the second number does not list properly (it just truncated the
 *     second number of the 'cut' prims so they ended up 'mess.s1.c' instead
 *     of 'mess.s1.c1').  And the '+' and '-' didn't work, I had to switch
 *     from 'mess.s1-1' to 'mess.s1.c1'.  Also, prims need to increment
 *     by the 'i' number but combs, regions, and assemblies (.c#, .r#, or
 *     just name with a # at the end) should increment by 1.  So you end
 *     up with widget_1, widget_2, widget_3   and not widget_1, widget_4,
 *     widget_7...
 *  5. Tree structure - please retain tree structure to the extent that
 *     you can and try not to re-create prims or combs used more than once.
 *     No warning needed for redundant copies.  Warnings can come later...
 * D6. Display - do display clones but do not resize or re-center view. 
 */

#include "common.h"

#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bio.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"

#include "./mged.h"
#include "./cmd.h"


#define CLONE_VERSION "Clone ver 4.0\n2006-08-08\n"
#define CLONE_BUFSIZE 512

/*
 * NOTE: in order to not shadow the global "dbip" pointer used
 * throughout mged, a "_dbip" is used for 'local' database instance
 * pointers to prevent proliferating the global dbip even further than
 * necessary.  the global is used at the hook functions only as a
 * starting point.
 */


/**
 * state structure used to keep track of what actions the user
 * requested and values necessary to perform the cloning operation.
 */
struct clone_state {
    Tcl_Interp *interp;         /* Stash a pointer to the tcl interpreter for output */
    struct directory *src;	/* Source object */
    size_t incr;		/* Amount to increment between copies */
    size_t n_copies;		/* Number of copies to make */
    int draw_obj;		/* 1 if draw copied object */
    hvect_t trans;		/* Translation between copies */
    hvect_t rot;		/* Rotation between copies */
    hvect_t rpnt;		/* Point to rotate about (default 0 0 0) */
    int miraxis;		/* Axis to mirror copy */
    fastf_t mirpos;		/* Point on axis to mirror copy */
    int autoview;		/* Execute autoview after drawing all objects */
    int updpos;			/* Position of number to update (for -c) */
};

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
    size_t name_size;
    size_t names_len;
    size_t names_used;
};


static struct nametbl obj_list;

/**
 * a polynamial value for representing knots
 */
struct knot {
    vect_t pt;
    fastf_t c[3][4];
};


/**
 * a spline path with various segments, break points, and polynamial
 * values.
 */
struct spline {
    size_t n_segs;
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
init_list(struct nametbl *l, size_t s)
{
    size_t i, j;

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
    size_t i, j;

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
 * returns the location of 'name' in the list if it exists, returns
 * -1 otherwise.
 */
static int
index_in_list(struct nametbl l, char *name)
{
    size_t i;

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
get_name(struct db_i *_dbip, struct directory *dp, struct clone_state *state, int iter)
{
    struct bu_vls *newname;
    char prefix[CLONE_BUFSIZE] = {0}, suffix[CLONE_BUFSIZE] = {0}, buf[CLONE_BUFSIZE] = {0}, suffix2[CLONE_BUFSIZE] = {0};
    int num = 0, i = 1, j = 0;

    newname = bu_vls_vlsinit();

    /* Ugh. This needs much repair/cleanup. */
    if (state->updpos == 0) {
	sscanf(dp->d_namep, "%[!-/,:-~]%d%[!-/,:-~]%512s", prefix, &num, suffix, suffix2); /* CLONE_BUFSIZE */
	snprintf(suffix, CLONE_BUFSIZE, "%s", suffix2);
    } else if (state->updpos == 1) {
	int num2 = 0;
	sscanf(dp->d_namep, "%[!-/,:-~]%d%[!-/,:-~]%d%[!-/,:-~]", prefix, &num2, suffix2, &num, suffix);
	snprintf(prefix, CLONE_BUFSIZE, "%s%d%s", prefix, num2, suffix2);
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
		    bu_vls_printf(newname, "%V%s", obj_list.names[j].dest[iter], suffix);
    		} else
		    bu_vls_printf(newname, "%zu%s", num+i*state->incr, suffix);
    	    else
    		bu_vls_printf(newname, "%zu", num + i*state->incr);
	} else /* non-region combinations */
    	    bu_vls_printf(newname, "%d", (num==0)?i+1:i+num);
	i++;
    } while (db_lookup(_dbip, bu_vls_addr(newname), LOOKUP_QUIET) != NULL);
    return newname;
}


/**
 * make a copy of a v4 solid by adding it to our book-keeping list,
 * adding it to the db directory, and writing it out to disk.
 */
static void
copy_v4_solid(struct db_i *_dbip, struct directory *proto, struct clone_state *state, int idx)
{
    struct directory *dp = (struct directory *)NULL;
    union record *rp = (union record *)NULL;
    size_t i, j;

    /* make n copies */
    for (i = 0; i < state->n_copies; i++) {
	struct bu_vls *name;

	if (i==0)
	    name = get_name(_dbip, proto, state, i);
	else {
	    dp = db_lookup(_dbip, bu_vls_addr(&obj_list.names[idx].dest[i-1]), LOOKUP_QUIET);
	    if (!dp) {
		continue;
	    }
	    name = get_name(_dbip, dp, state, i);
	}

	/* XXX: this can probably be optimized. */
	bu_vls_strcpy(&obj_list.names[idx].dest[i], bu_vls_addr(name));
	bu_vls_free(name);

	/* add the object to the directory */
	dp = db_diradd(_dbip, bu_vls_addr(&obj_list.names[idx].dest[i]), RT_DIR_PHONY_ADDR, proto->d_len, proto->d_flags, &proto->d_minor_type);
	if ((dp == RT_DIR_NULL) || (db_alloc(_dbip, dp, proto->d_len) < 0)) {
	    TCL_ALLOC_ERR;
	    return;
	}

	/* get an in-memory reference to the object being copied */
	if ((rp = db_getmrec(_dbip, proto)) == (union record *)0) {
	    TCL_READ_ERR;
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
	    if (state->trans[W] > SMALL_FASTF)
		/* assumes primitive's first parameter is its position */
		VADD2(rp->s.s_values, rp->s.s_values, state->trans);
	    /* rotate */
	    if (state->rot[W] > SMALL_FASTF) {
		mat_t r;
		vect_t vec, ovec;

		if (state->rpnt[W] > SMALL_FASTF)
		    VSUB2(rp->s.s_values, rp->s.s_values, state->rpnt);
		MAT_IDN(r);
		bn_mat_angles(r, state->rot[X], state->rot[Y], state->rot[Z]);
		for (j = 0; j < 24; j+=3) {
		    VMOVE(vec, rp->s.s_values+j);
		    MAT4X3VEC(ovec, r, vec);
		    VMOVE(rp->s.s_values+j, ovec);
		}
		if (state->rpnt[W] > SMALL_FASTF)
		    VADD2(rp->s.s_values, rp->s.s_values, state->rpnt);
	    }
	} else
	    bu_log("mods not available on %s\n", proto->d_namep);

	/* write the object to disk */
	if (db_put(_dbip, dp, rp, 0, dp->d_len) < 0) {
	    bu_log("ERROR: clone internal error writing to the database\n");
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
copy_v5_solid(struct db_i *_dbip, struct directory *proto, struct clone_state *state, int idx)
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
    if (state->trans[W] > SMALL_FASTF)
	MAT_DELTAS_ADD_VEC(matrix, state->trans);

    /* rotation */
    if (state->rot[W] > SMALL_FASTF) {
    	mat_t m2, t;

	bn_mat_angles(m2, state->rot[X], state->rot[Y], state->rot[Z]);
	if (state->rpnt[W] > SMALL_FASTF) {
	    mat_t m3;

	    bn_mat_xform_about_pt(m3, m2, state->rpnt);
	    bn_mat_mul(t, matrix, m3);
	} else
	    bn_mat_mul(t, matrix, m2);

	MAT_COPY(matrix, t);
    }

    /* make n copies */
    for (i = 0; i < state->n_copies; i++) {
	char *argv[6] = {"wdb_copy", (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL};
	struct bu_vls *name;
	int ret;
	struct directory *dp = (struct directory *)NULL;
	struct rt_db_internal intern;

	if (i==0)
	    dp = proto;
	else
	    dp = db_lookup(_dbip, bu_vls_addr(&obj_list.names[idx].dest[i-1]), LOOKUP_QUIET);

	if (!dp) {
	    continue;
	}

	name = get_name(_dbip, dp, state, i); /* get new name */
	bu_vls_strcpy(&obj_list.names[idx].dest[i], bu_vls_addr(name));

	/* actually copy the primitive to the new name */
	argv[1] = proto->d_namep;
	argv[2] = bu_vls_addr(name);
	ret = wdb_copy_cmd(_dbip->dbi_wdbp, state->interp, 3, argv);
	if (ret != TCL_OK)
	    bu_log("WARNING: failure cloning \"%s\" to \"%s\"\n", proto->d_namep, bu_vls_addr(name));

	/* get the original objects matrix */
	if (rt_db_get_internal(&intern, dp, _dbip, matrix, &rt_uniresource) < 0) {
	    bu_log("ERROR: clone internal error copying %s\n", proto->d_namep);
	    bu_vls_free(name);
	    return;
	}
	RT_CK_DB_INTERNAL(&intern);
	/* pull the new name */
	dp = db_lookup(_dbip, bu_vls_addr(name), LOOKUP_QUIET);
	bu_vls_free(name);
	if (!dp) {
	    bu_vls_free(name);
	    continue;
	}

	/* write the new matrix to the new object */
	if (rt_db_put_internal(dp, wdbp->dbip, &intern, &rt_uniresource) < 0)
	    bu_log("ERROR: clone internal error copying %s\n", proto->d_namep);
	rt_db_free_internal(&intern);
    } /* end iteration over each copy */

    return;
}


/**
 * make n copies of a database combination by adding it to our
 * book-keeping list, adding it to the directory, then writing it out
 * to the db.
 */
static void
copy_solid(struct db_i *_dbip, struct directory *proto, genptr_t state)
{
    int idx;

    if (is_in_list(obj_list, proto->d_namep)) {
	bu_log("Solid primitive %s already cloned?\n", proto->d_namep);
	return;
    }

    idx = add_to_list(&obj_list, proto->d_namep);

    /* sanity check that the item was really added */
    if ((idx < 0) || !is_in_list(obj_list, proto->d_namep)) {
	bu_log("ERROR: clone internal error copying %s\n", proto->d_namep);
	return;
    }

    if (db_version(_dbip) < 5)
	(void)copy_v4_solid(_dbip, proto, (struct clone_state *)state, idx);
    else
	(void)copy_v5_solid(_dbip, proto, (struct clone_state *)state, idx);
    return;
}


/**
 * make n copies of a v4 combination.
 */
static struct directory *
copy_v4_comb(struct db_i *_dbip, struct directory *proto, struct clone_state *state, int idx)
{
    struct directory *dp = (struct directory *)NULL;
    union record *rp = (union record *)NULL;
    size_t i;
    size_t j;

    /* make n copies */
    for (i = 0; i < state->n_copies; i++) {

	/* get a v4 in-memory reference to the object being copied */
	if ((rp = db_getmrec(_dbip, proto)) == (union record *)0) {
	    TCL_READ_ERR;
	    return NULL;
	}

	if (proto->d_flags & RT_DIR_REGION) {
	    if (!is_in_list(obj_list, rp[1].M.m_instname)) {
		bu_log("ERROR: clone internal error looking up %s\n", rp[1].M.m_instname);
		return NULL;
	    }
	    bu_vls_strcpy(&obj_list.names[idx].dest[i], bu_vls_addr(&obj_list.names[index_in_list(obj_list, rp[1].M.m_instname)].dest[i]));
	    /* bleh, odd convention going on here.. prefix regions with an 'r' */
	    *bu_vls_addr(&obj_list.names[idx].dest[i]) = 'r';
	} else {
	    struct bu_vls *name;
	    if (i==0)
		name = get_name(_dbip, proto, state, i);
	    else {
		dp = db_lookup(_dbip, bu_vls_addr(&obj_list.names[idx].dest[i-1]), LOOKUP_QUIET);
		if (!dp) {
		    continue;
		}
		name = get_name(_dbip, dp, state, i);
	    }
	    bu_vls_strcpy(&obj_list.names[idx].dest[i], bu_vls_addr(name));
	    bu_vls_free(name);
	}
	bu_strlcpy(rp[0].c.c_name, bu_vls_addr(&obj_list.names[idx].dest[i]), NAMESIZE);

	/* add the object to the directory */
	dp = db_diradd(_dbip, rp->c.c_name, RT_DIR_PHONY_ADDR, proto->d_len, proto->d_flags, &proto->d_minor_type);
	if ((dp == NULL) || (db_alloc(_dbip, dp, proto->d_len) < 0)) {
	    TCL_ALLOC_ERR;
	    return NULL;
	}

	for (j = 1; j < proto->d_len; j++) {
	    if (!is_in_list(obj_list, rp[j].M.m_instname)) {
		bu_log("ERROR: clone internal error looking up %s\n", rp[j].M.m_instname);
		return NULL;
	    }
	    snprintf(rp[j].M.m_instname, NAMESIZE, "%s", bu_vls_addr(&obj_list.names[index_in_list(obj_list, rp[j].M.m_instname)].dest[i]));
	}

	/* write the object to disk */
	if (db_put(_dbip, dp, rp, 0, dp->d_len) < 0) {
	    bu_log("ERROR: clone internal error writing to the database\n");
	    return NULL;
	}

	/* our responsibility to free the record */
	bu_free((char *)rp, "deallocate copy_v4_comb() db_getmrec() record");
    }

    return dp;
}


/*
 * update the v5 combination tree with the new names.
 * DESTRUCTIVE RECURSIVE
 */
int
copy_v5_comb_tree(union tree *tree, int idx)
{
    char *buf;
    switch (tree->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    /* copy right */
	    copy_v5_comb_tree(tree->tr_b.tb_right, idx);
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    /* copy left */
	    copy_v5_comb_tree(tree->tr_b.tb_left, idx);
	    break;
	case OP_DB_LEAF:
	    buf = tree->tr_l.tl_name;
	    tree->tr_l.tl_name = bu_strdup(bu_vls_addr(&obj_list.names[index_in_list(obj_list, buf)].dest[idx]));
	    bu_free(buf, "node name");
	    break;
	default:
	    bu_log("clone v5 - OPCODE NOT IMPLEMENTED: %d\n", tree->tr_op);
	    return -1;
    }
    return 0;
}


/**
 * make n copies of a v5 combination.
 */
static struct directory *
copy_v5_comb(struct db_i *_dbip, struct directory *proto, struct clone_state *state, int idx)
{
    struct directory *dp = (struct directory *)NULL;
    struct bu_vls *name;
    size_t i;

    /* sanity */
    if (!proto) {
	bu_log("ERROR: clone internal consistency error\n");
	return (struct directory *)NULL;
    }

    /* make n copies */
    for (i = 0; i < state->n_copies; i++) {
	if (i==0)
	    name = get_name(_dbip, proto, state, i);
	else {
	    dp = db_lookup(_dbip, bu_vls_addr(&obj_list.names[idx].dest[i-1]), LOOKUP_QUIET);
	    if (!dp) {
		continue;
	    }
	    name = get_name(_dbip, dp, state, i);
	}
	bu_vls_strcpy(&obj_list.names[idx].dest[i], bu_vls_addr(name));

	/* we have a before and an after, do the copy */
	if (proto->d_namep && bu_vls_addr(name)) {
	    struct rt_db_internal dbintern;
	    struct rt_comb_internal *comb;

	    dp = db_lookup(_dbip, proto->d_namep, LOOKUP_QUIET);
	    if (!dp) {
		bu_vls_free(name);
		continue;
	    }
	    if (rt_db_get_internal(&dbintern, dp, _dbip, bn_mat_identity, &rt_uniresource) < 0) {
		bu_log("ERROR: clone internal error copying %s\n", proto->d_namep);
		return NULL;
	    }

	    if ((dp=db_diradd(wdbp->dbip, bu_vls_addr(name), -1, 0, proto->d_flags, (genptr_t)&proto->d_minor_type)) == RT_DIR_NULL) {
		bu_log("An error has occured while adding a new object to the database.");
		return NULL;
	    }

	    RT_CK_DB_INTERNAL(&dbintern);
	    comb = (struct rt_comb_internal *)dbintern.idb_ptr;
	    RT_CK_COMB(comb);
	    RT_CK_TREE(comb->tree);

	    /* recursively update the tree */
	    copy_v5_comb_tree(comb->tree, i);

	    if (rt_db_put_internal(dp, wdbp->dbip, &dbintern, &rt_uniresource) < 0) {
		bu_log("ERROR: clone internal error copying %s\n", proto->d_namep);
		bu_vls_free(name);
		return NULL;
	    }
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
copy_comb(struct db_i *_dbip, struct directory *proto, genptr_t state)
{
    int idx;

    if (is_in_list(obj_list, proto->d_namep)) {
	bu_log("Combination %s already cloned?\n", proto->d_namep);
	return;
    }

    idx = add_to_list(&obj_list, proto->d_namep);

    /* sanity check that the item was really added to our bookkeeping */
    if ((idx < 0) || !is_in_list(obj_list, proto->d_namep)) {
	bu_log("ERROR: clone internal error copying %s\n", proto->d_namep);
	return;
    }

    if (db_version(_dbip) < 5)
	(void)copy_v4_comb(_dbip, proto, (struct clone_state *)state, idx);
    else
	(void)copy_v5_comb(_dbip, proto, (struct clone_state *)state, idx);

    return;
}


/**
 * recursively copy a tree of geometry
 */
static struct directory *
copy_tree(struct db_i *_dbip, struct directory *dp, struct resource *resp, struct clone_state *state)
{
    size_t i;
    union record *rp = (union record *)NULL;
    struct directory *mdp = (struct directory *)NULL;
    struct directory *copy = (struct directory *)NULL;

    struct bu_vls *copyname = NULL;
    struct bu_vls *nextname = NULL;

    /* get the name of what the object "should" get cloned to */
    copyname = get_name(_dbip, dp, state, 0);

    /* copy the object */
    if (dp->d_flags & RT_DIR_COMB) {

	if (db_version(_dbip) < 5) {
	    /* A v4 method of peeking into a combination */

	    int errors = 0;

	    /* get an in-memory record of this object */
	    if ((rp = db_getmrec(_dbip, dp)) == (union record *)0) {
		TCL_READ_ERR;
		goto done_copy_tree;
	    }
	    /*
	     * if it is a combination/region, copy the objects that
	     * make up the object.
	     */
	    for (i = 1; i < dp->d_len; i++) {
		if ((mdp = db_lookup(_dbip, rp[i].M.m_instname, LOOKUP_NOISY)) == RT_DIR_NULL) {
		    errors++;
		    bu_log("WARNING: failed to locate \"%s\"\n", rp[i].M.m_instname);
		    continue;
		}
		copy = copy_tree(_dbip, mdp, resp, state);
		if (!copy) {
		    errors++;
		    bu_log("WARNING: unable to fully clone \"%s\"\n", rp[i].M.m_instname);
		}
	    }

	    if (errors) {
		bu_log("WARNING: some elements of \"%s\" could not be cloned\n", dp->d_namep);
	    }

	    /* copy this combination itself */
	    copy_comb(_dbip, dp, (genptr_t)state);
	} else
	    /* A v5 method of peeking into a combination */
	    db_functree(_dbip, dp, copy_comb, copy_solid, resp, (genptr_t)state);
    } else if (dp->d_flags & RT_DIR_SOLID)
	/* leaf node -- make a copy the object */
	copy_solid(_dbip, dp, (genptr_t)state);
    else {
	Tcl_AppendResult(state->interp, "clone:  ", dp->d_namep, " is neither a combination or a primitive?\n", (char *)NULL);
	goto done_copy_tree;
    }

    nextname = get_name(_dbip, dp, state, 0);
    if (bu_vls_strcmp(copyname, nextname) == 0)
	bu_log("ERROR: unable to successfully clone \"%s\" to \"%s\"\n", dp->d_namep, bu_vls_addr(copyname));
    else
	copy = db_lookup(_dbip, bu_vls_addr(copyname), LOOKUP_QUIET);

 done_copy_tree:
    if (rp)
	bu_free((char *)rp, "copy_tree record[]");
    if (copyname)
	bu_free((char *)copyname, "free get_name() copyname");
    if (nextname)
	bu_free((char *)nextname, "free get_name() copyname");

    return copy;
}


/**
 * copy an object, recursivley copying all of the object's contents
 * if it's a combination/region.
 */
static struct directory *
copy_object(struct db_i *_dbip, struct resource *resp, struct clone_state *state)
{
    struct directory *copy = (struct directory *)NULL;
    size_t i, j, idx;

    init_list(&obj_list, state->n_copies);

    /* do the actual copying */
    copy = copy_tree(_dbip, state->src, resp, state);

    /* make sure it made what we hope/think it made */
    if (!copy || !is_in_list(obj_list, state->src->d_namep))
	return copy;

    /* display the cloned object(s) */
    if (state->draw_obj) {
	const char *av[3] = {"e", NULL, NULL};

	idx = index_in_list(obj_list, state->src->d_namep);
	for (i = 0; i < (state->n_copies > obj_list.name_size ? obj_list.name_size : state->n_copies); i++) {
	    av[1] = bu_vls_addr(&obj_list.names[idx].dest[i]);
	    /* draw does not use clientdata */
	    cmd_draw((ClientData)NULL, state->interp, 2, av);
	}
	if (state->autoview) {
	    av[0] = "autoview";
	    cmd_autoview((ClientData)NULL, state->interp, 1, av);
	}
    }

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
 * helper function that computes where a point is along a spline
 * given some distance 't'.
 *
 * i.e. sets pt = Q(t) for the specified spline
 */
void
interp_spl(fastf_t t, struct spline spl, vect_t pt)
{
    int i = 0;
    fastf_t s, s2, s3;

    if (EQUAL(t, spl.t[spl.n_segs]))
	t -= VUNITIZE_TOL;

    /* traverse to the spline segment interval */
    while (t >= spl.t[i+1])
	i++;

    /* compute the t offset */
    t -= spl.t[i];
    s = t; s2 = t*t; s3 = t*t*t;

    /* solve for the position */
    pt[X] = spl.k[i].c[X][0] + spl.k[i].c[X][1]*s + spl.k[i].c[X][2]*s2 + spl.k[i].c[X][3]*s3;
    pt[Y] = spl.k[i].c[Y][0] + spl.k[i].c[Y][1]*s + spl.k[i].c[Y][2]*s2 + spl.k[i].c[Y][3]*s3;
    pt[Z] = spl.k[i].c[Z][0] + spl.k[i].c[Z][1]*s + spl.k[i].c[Z][2]*s2 + spl.k[i].c[Z][3]*s3;
}


/**
 * master hook function for the 'tracker' command used to create
 * copies of objects along a spline path.
 */
int
f_tracker(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    size_t ret;
    struct spline s;
    vect_t *verts  = (vect_t *)NULL;
    struct link *links = (struct link *)NULL;
    int opt;
    size_t i, j, k, inc;
    size_t n_verts, n_links;
    int arg = 1;
    FILE *points = (FILE *)NULL;
    char tok[81] = {0}, line[81] = {0};
    char ch;
    fastf_t totlen = 0.0;
    fastf_t len, olen;
    fastf_t dist_to_next;
    fastf_t min, max, mid;
    fastf_t pt[3] = {0};
    int no_draw = 0;

    /* allow interrupts */
    if (setjmp(jmp_env) == 0)
	(void)signal(SIGINT, sig3);
    else
	return TCL_OK;

    bu_optind = 1;
    opt = bu_getopt(argc, (char * const *)argv, "fh");
    while (opt != EOF) {
	switch (opt) {
	    case 'f':
		no_draw = 1;
		arg++;
		break;
	    case 'h':
		Tcl_AppendResult(interp, "tracker [-fh] [# links] [increment] [spline.iges] [link...]\n\n", (char *)NULL);
		Tcl_AppendResult(interp, "-f:\tDo not draw the links as they are made.\n", (char *)NULL);
		Tcl_AppendResult(interp, "-h:\tPrint this message.\n\n", (char *)NULL);
		Tcl_AppendResult(interp, "\tThe prototype link(s) should be placed so that one\n", (char *)NULL);
		Tcl_AppendResult(interp, "\tpin's vertex lies on the origin and points along the\n", (char *)NULL);
		Tcl_AppendResult(interp, "\ty-axis, and the link should lie along the positive x-axis.\n\n", (char *)NULL);
		Tcl_AppendResult(interp, "\tIf two or more sublinks comprise the link, they are specified in this manner:\n", (char *)NULL);
		Tcl_AppendResult(interp, "\t<link1> <%% of total link> <link2> <%% of total link> ....\n", (char *)NULL);
		return TCL_OK;
	}
    }

    if (argc < arg+1) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter number of links: ", (char *)NULL);
	return TCL_ERROR;
    }
    n_verts = atoi(argv[arg++])+1;

    if (argc < arg+1) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter amount to increment parts by: ", (char *)NULL);
	return TCL_ERROR;
    }
    inc = atoi(argv[arg++]);

    if (argc < arg+1) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter spline file name: ", (char *)NULL);
	return TCL_ERROR;
    }
    if ((points = fopen(argv[arg++], "r")) == NULL) {
	fprintf(stdout, "tracker:  couldn't open points file %s.\n", argv[arg-1]);
	return TCL_ERROR;
    }

    if (argc < arg+1) {
	Tcl_AppendResult(interp, MORE_ARGS_STR, "Enter prototype link name: ", (char *)NULL);
	return TCL_ERROR;
    }


    /* Prepare vert list *****************************/
    n_links = ((argc-3)/2)>1?((argc-3)/2):1;
    verts = (vect_t *)malloc(sizeof(vect_t) * n_verts * (n_links+2));

    /* Read in links names and link lengths **********/
    links = (struct link *)malloc(sizeof(struct link)*n_links);
    for (i = arg; i < (size_t)argc; i+=2) {
	bu_vls_strcpy(&links[(i-arg)/2].name, argv[i]);
	if (argc > arg+1)
	    sscanf(argv[i+1], "%lf", &links[(i-arg)/2].pct);
	else
	    links[(i-arg)/2].pct = 1.0;
	totlen += links[(i-arg)/2].pct;
    }
    if (!ZERO(totlen - 1.0))
	fprintf(stdout, "ERROR\n");

    /* Read in knots from specified file *************/
    do
	bu_fgets(line, 81, points);
    while (!BU_STR_EQUAL(strtok(line, ","), "112"));

    bu_strlcpy(tok, strtok(NULL, ","), sizeof(tok));
    bu_strlcpy(tok, strtok(NULL, ","), sizeof(tok));
    bu_strlcpy(tok, strtok(NULL, ","), sizeof(tok));
    bu_strlcpy(tok, strtok(NULL, ","), sizeof(tok));
    s.n_segs = atoi(tok);
    s.t = (fastf_t *)bu_malloc(sizeof(fastf_t) * (s.n_segs+1), "t");
    s.k = (struct knot *)bu_malloc(sizeof(struct knot) * (s.n_segs+1), "k");
    for (i = 0; i <= s.n_segs; i++) {
	bu_strlcpy(tok, strtok(NULL, ","), sizeof(tok));
	if (strstr(tok, "P") != NULL) {
	    bu_fgets(line, 81, points);
	    bu_fgets(line, 81, points);
	    bu_strlcpy(tok, strtok(line, ","), sizeof(tok));
	}
	s.t[i] = atof(tok);
    }
    for (i = 0; i <= s.n_segs; i++)
	for (j = 0; j < 3; j++) {
	    for (k = 0; k < 4; k++) {
		bu_strlcpy(tok, strtok(NULL, ","), sizeof(tok));
		if (strstr(tok, "P") != NULL) {
		    bu_fgets(line, 81, points);
		    bu_fgets(line, 81, points);
		    bu_strlcpy(tok, strtok(line, ","), sizeof(tok));
		}
		s.k[i].c[j][k] = atof(tok);
	    }
	    s.k[i].pt[j] = s.k[i].c[j][0];
	}
    fclose(points);

    /* Interpolate link vertices *********************/
    for (i = 0; i < s.n_segs; i++) /* determine initial track length */
	totlen += DIST_PT_PT(s.k[i].pt, s.k[i+1].pt);
    len = totlen/(n_verts-1);
    VMOVE(verts[0], s.k[0].pt);
    olen = 2*len;

    for (i = 0; (fabs(olen-len) >= VUNITIZE_TOL) && (i < 250); i++) {
	/* number of track iterations */
	fprintf(stdout, ".");
	fflush(stdout);
	for (j = 0; j < n_links; j++) /* set length of each link based on current track length */
	    links[j].len = len * links[j].pct;
	min = 0;
	max = s.t[s.n_segs];
	mid = 0;

	for (j = 0; j < n_verts+1; j++) /* around the track once */
	    for (k = 0; k < n_links; k++) {
		/* for each sub-link */
		if ((k == 0) && (j == 0)) {continue;} /* the first sub-link of the first link is already in position */
		min = mid;
		max = s.t[s.n_segs];
		mid = (min+max)/2;
		interp_spl(mid, s, pt);
		dist_to_next = (k > 0) ? links[k-1].len : links[n_links-1].len; /* links[k].len;*/
		while (fabs(DIST_PT_PT(verts[n_links*j+k-1], pt) - dist_to_next) >= VUNITIZE_TOL) {
		    if (DIST_PT_PT(verts[n_links*j+k-1], pt) > dist_to_next) {
			max = mid;
			mid = (min+max)/2;
		    } else {
			min = mid;
			mid = (min+max)/2;
		    }
		    interp_spl(mid, s, pt);
		    if (fabs(min-max) <= VUNITIZE_TOL) {break;}
		}
		interp_spl(mid, s, verts[n_links*j+k]);
	    }

	interp_spl(s.t[s.n_segs], s, verts[n_verts*n_links-1]);
	totlen = 0.0;
	for (j = 0; j < n_verts*n_links-1; j++)
	    totlen += DIST_PT_PT(verts[j], verts[j+1]);
	olen = len;
	len = totlen/(n_verts-1);
    }
    fprintf(stdout, "\n");

    /* Write out interpolation info ******************/
    fprintf(stdout, "%ld Iterations; Final link lengths:\n", (unsigned long)i);
    for (i = 0; i < n_links; i++)
	fprintf(stdout, "  %s\t%.15f\n", bu_vls_addr(&links[i].name), links[i].len);
    fflush(stdin);
    /* Place links on vertices ***********************/
    fprintf(stdout, "Continue? [y/n]  ");
    ret = fscanf(stdin, "%c", &ch);
    if (ret != 1)
	perror("fscanf");

    if (ch == 'y') {
	struct clone_state state;
	struct directory **dps = (struct directory **)NULL;
	char *vargs[3];

	for (i = 0; i < 2; i++)
	    vargs[i] = (char *)bu_calloc(CLONE_BUFSIZE, sizeof(char), "alloc vargs[i]");
	vargs[0][0] = 'e';

	state.interp = interp;
	state.incr = inc;
	state.n_copies = 1;
	state.draw_obj = 0;
	state.miraxis = W;

	dps = (struct directory **)bu_calloc(n_links, sizeof(struct directory *), "alloc dps array");
	/* rots = (vect_t *)bu_malloc(sizeof(vect_t)*n_links, "alloc rots");*/
	for (i = 0; i < n_links; i++) {
	    /* global dbip */
	    dps[i] = db_lookup(dbip, bu_vls_addr(&links[i].name), LOOKUP_QUIET);
	    /* VSET(rots[i], 0, 0, 0);*/
	}

	for (i = 0; i < n_verts-1; i++) {
	    for (j = 0; j < n_links; j++) {
		if (i == 0) {
		    VSCALE(state.trans, verts[n_links*i+j], local2base);
		} else
		    VSUB2SCALE(state.trans, verts[n_links*(i-1)+j], verts[n_links*i+j], local2base);
		VSCALE(state.rpnt, verts[n_links*i+j], local2base);

		VSUB2(pt, verts[n_links*i+j], verts[n_links*i+j+1]);
		VSET(state.rot, 0, (M_PI - atan2(pt[Z], pt[X])),
		     -atan2(pt[Y], sqrt(pt[X]*pt[X]+pt[Z]*pt[Z])));
		VSCALE(state.rot, state.rot, RAD2DEG);
		/*
		  VSUB2(state.rot, state.rot, rots[j]);
		  VADD2(rots[j], state.rot, rots[j]);
		*/

		state.src = dps[j];
		/* global dbip */
		dps[j] = copy_object(dbip, &rt_uniresource, &state);
		bu_strlcpy(vargs[1], dps[j]->d_namep, CLONE_BUFSIZE);

		if (!no_draw || !is_dm_null()) {
		    drawtrees(2, (const char **)vargs, 1);
		    size_reset();
		    new_mats();
		    color_soltab();
		    refresh();
		}
		fprintf(stdout, ".");
		fflush(stdout);
	    }
	}
	fprintf(stdout, "\n");
	bu_free(dps, "free dps array");

	for (i = 0; i < 2; i++)
	    bu_free(vargs[i], "free vargs[i]");
    }

    free(s.t);
    free(s.k);
    free(links);
    free(verts);
    (void)signal(SIGINT, SIG_IGN);
    return TCL_OK;
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
