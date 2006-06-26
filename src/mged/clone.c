/*	                  C L O N E . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file clone.c
 *
 * Functions -
 *	f_clone		clones an object, optionally 
 *			rotating or translating the copies
 *	f_tracker	clones an object, evenly
 *			spacing the copies along a spline
 *
 * Author -
 *	Adam Ross
 *
 * Source -
 *      Geometric Solutions, Inc.
 */

#include "common.h"
  
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./ged.h"
#include "./cmd.h"


#define CLONE_VERSION "Clone ver 3.0\n2005-12-12\n"

extern int is_dm_null(void);


/** state structure used to keep track of what actions the user
 *  requested and values necessary to perform the cloning operation.
 */
struct clone_state {
    struct directory	*src;		/* Source object */
    int			incr;		/* Amount to increment between copies */
    int			n_copies;	/* Number of copies to make */
    int			draw_obj;	/* 1 if draw copied object */
    hvect_t		trans;		/* Translation between copies */
    hvect_t		rot;		/* Rotation between copies */
    hvect_t		rpnt;		/* Point to rotate about (default 0 0 0) */
    int			miraxis;	/* Axis to mirror copy */
    fastf_t		mirpos;		/* Point on axis to mirror copy */
};

/* XXX change from NAMESIZE to bu_vls */
struct name {
    char src[NAMESIZE];	/* source object name */
    char **dest;		/* dest object names */
};

/** structure used to store the names of objects that are to be
 *  cloned.  space is preallocated via names with len and used keeping
 *  track of space available and used.
 */
struct nametbl {
    struct name *names;
    int name_size;
    int names_len;
    int names_used;
};

struct nametbl obj_list;

/** a polynamial value for representing knots */
struct knot {
    vect_t pt;
    fastf_t c[3][4];
};

/** a spline path with various segments, break points, and polynamial
 *  values.
 */
struct spline {
    int n_segs;
    fastf_t *t; /* break points */
    struct knot *k; /* polynomials */
};

struct link {
    char name[NAMESIZE];
    fastf_t len;
    fastf_t pct;
};


/** initialize the name list used for stashing destination names */
void
init_list(struct nametbl *l, int s)
{
    int i, j;

    l->names = (struct name *)bu_calloc(10, sizeof(struct name), "alloc l->names");
    for (i = 0; i < 10; i++) {
	l->names[i].dest = (char **)bu_calloc(s, sizeof(char *), "alloc l->names.dest");
	for (j = 0; j < s; j++) {
	    l->names[i].dest[j] = (char *)bu_calloc(NAMESIZE, sizeof(char), "alloc l->names.dest[j]");
	}
    }
    l->name_size = s;
    l->names_len = 10;
    l->names_used = 0;
}


/** add a new name to the name list */
int
add_to_list(struct nametbl *l, char name[NAMESIZE])
{
    int i, j;

    /* add more slots if adding 1 more new name will fill up all the
     * available slots.
     */
    if (l->names_len == (l->names_used+1)) {
	l->names_len += 10;
	l->names = (struct name *)bu_realloc(l->names, sizeof(struct name)*(l->names_len+1), "realloc l->names");
	for (i = l->names_used; i < l->names_len; i++) {
	    l->names[i].dest = (char **)bu_calloc(l->name_size, sizeof(char *), "alloc l->names.dest");
	    for (j = 0; j < l->name_size; j++) {
		l->names[i].dest[j] = (char *)bu_calloc(NAMESIZE, sizeof(char), "alloc l->names.dest[j]");
	    }
	}
    }
    strncpy(l->names[l->names_used++].src, name, NAMESIZE);
    return l->names_used-1; /* return number of available slots */
}


/** returns truthfully if 'name' exists in the list */
int
is_in_list(struct nametbl l, char name[NAMESIZE])
{
    int i;

    for (i = 0; i < l.names_used; i++) {
	if (!strcmp(l.names[i].src, name)) {
	    return 1;
	}
    }
    return 0;
}


/** returns the location of 'name' in the list if it exists, returns
 * -1 otherwise.
 */
int
index_in_list(struct nametbl l, char name[NAMESIZE])
{
    int i;

    for (i = 0; i < l.names_used; i++) {
	if (!strcmp(l.names[i].src, name)) {
	    return i;
	}
    }
    return -1;
}


/** returns the next available/unused name, using a naming convention
 *  that increments a trailing or first embedded number.  state.incr
 *  is used for each number level increase.
 */
char *
get_name(struct directory *dp, struct clone_state state, int iter)
{
    static char *newname = NULL;
    char tag[NAMESIZE] = {0}, post[NAMESIZE] = {0}, buf[NAMESIZE] = {0};
    int num = 0, i, j;

    if (!newname) {
	newname = (char *)bu_calloc(NAMESIZE, sizeof(char), "alloc newname");
    }
    sscanf(dp->d_namep, "%[!-/,:-~]%d%[!-/,:-~]", &tag, &num, &post);

    i = 1;
    if ((dp->d_flags & DIR_SOLID) || (dp->d_flags & DIR_REGION)) {
	do {
	    if (post[0] == '.') {
		
		if ((i == 1) && is_in_list(obj_list, buf)) {
		    j = index_in_list(obj_list, buf);
		    snprintf(buf, NAMESIZE, "%s%d", tag, num);
		    snprintf(newname, NAMESIZE, "%s%s", obj_list.names[j].dest[iter], post);
		} else {
		    snprintf(newname, NAMESIZE, "%s%d%s", tag, num+i*state.incr, post);
		}
	    } else {
		snprintf(newname, NAMESIZE, "%s%d", tag, num+i*state.incr);
	    }
	    i++;
	} while (db_lookup(dbip, newname, LOOKUP_QUIET) != NULL);
    } else {
	do {
	    snprintf(newname, NAMESIZE, "%s%d", tag, (num==0)?2:num+i);
	    i++;
	} while (db_lookup(dbip, newname, LOOKUP_QUIET) != NULL);
    }
    return newname;
}


/** make a copy of a combination by adding it to our book-keeping list,
 *  adding it to the db directory, then writing it out to the db.
 */
struct directory *
copy_comb(struct directory *proto, struct clone_state state)
{
    register struct directory *dp = (struct directory *)NULL;
    union record *rp = (union record *)NULL;
    int i, j, idx;

    if (is_in_list(obj_list, proto->d_namep)) {
	bu_log("Combination %s already cloned?\n", proto->d_namep);
	return NULL;
    }
    idx = add_to_list(&obj_list, proto->d_namep);

    /* sanity check that the item was really added */
    if ((idx < 0) || !is_in_list(obj_list, proto->d_namep)) {
	bu_log("ERROR: clone internal error copying %s\n", proto->d_namep);
	return NULL;
    }


    /* make n copies */
    for (i = 0; i < state.n_copies; i++) {

	/* get a v4 in-memory reference to the object being copied */
	if ((rp = db_getmrec(dbip, proto)) == (union record *)0) {
	    TCL_READ_ERR;
	    return NULL;
	}

	if (proto->d_flags & DIR_REGION) {
	    if (!is_in_list(obj_list, rp[1].M.m_instname)) {
		bu_log("ERROR: clone internal error looking up %s\n", rp[1].M.m_instname);
		return NULL;
	    }
	    strncpy(obj_list.names[idx].dest[i], obj_list.names[index_in_list(obj_list, rp[1].M.m_instname)].dest[i], NAMESIZE);
	    obj_list.names[idx].dest[i][0] = 'r';
	} else {
	    strncpy(obj_list.names[idx].dest[i], get_name(((i==0) ? proto : db_lookup(dbip, obj_list.names[idx].dest[i-1], LOOKUP_QUIET)), state, i), NAMESIZE);
	}
	strncpy(rp[0].c.c_name, obj_list.names[idx].dest[i], NAMESIZE);

	/* add the object to the directory */
	dp = db_diradd(dbip, rp->c.c_name, RT_DIR_PHONY_ADDR, proto->d_len, proto->d_flags, &proto->d_minor_type);
	if ((dp == NULL) || (db_alloc(dbip, dp, proto->d_len) < 0)) {
	    TCL_ALLOC_ERR;
	    return NULL;
	}

	for (j = 1; j < proto->d_len; j++) {
	    if (!is_in_list(obj_list, rp[j].M.m_instname)) {
		bu_log("ERROR: clone internal error looking up %s\n", rp[j].M.m_instname);
		return NULL;
	    }
	    snprintf(rp[j].M.m_instname, NAMESIZE, "%s", obj_list.names[index_in_list(obj_list, rp[j].M.m_instname)].dest[i]);
	}

	/* write the object to disk */
	if (db_put(dbip, dp, rp, 0, dp->d_len) < 0) {
	    bu_log("ERROR: clone internal error writing to the database\n");
	    return NULL;
	}

	bu_free((char *)rp, "copy_comb record[]");
    }

    return dp;
}


/** make a copy of a solid by adding it to our book-keeping list,
 *  adding it to the db directory, and writing it out to disk.
 */
struct directory *
copy_solid(struct directory *proto, struct clone_state state)
{
    register struct directory *dp = (struct directory *)NULL;
    union record *rp = (union record *)NULL;
    int i, j, idx;

    if (is_in_list(obj_list, proto->d_namep)) {
	bu_log("Primitive solid %s already cloned?\n", proto->d_namep);
	return NULL;
    }
    idx = add_to_list(&obj_list, proto->d_namep);

    /* sanity check that the item was really added */
    if ((idx < 0) || !is_in_list(obj_list, proto->d_namep)) {
	bu_log("ERROR: clone internal error copying %s\n", proto->d_namep);
	return NULL;
    }

    /* make n copies */
    for (i = 0; i < state.n_copies; i++) {

	/* get an in-memory reference to the object being copied */
	if ((rp = db_getmrec(dbip, proto)) == (union record *)0) {
	    TCL_READ_ERR;
	    return NULL;
	}

	strncpy(obj_list.names[idx].dest[i], get_name(((i==0) ? proto : db_lookup(dbip, obj_list.names[idx].dest[i-1], LOOKUP_QUIET)), state, i), NAMESIZE);

	/* add the object to the directory */
	dp = db_diradd(dbip, obj_list.names[idx].dest[i], RT_DIR_PHONY_ADDR, proto->d_len, proto->d_flags, &proto->d_minor_type);
	if ((dp == DIR_NULL) || (db_alloc(dbip, dp, proto->d_len) < 0)) {
	    TCL_ALLOC_ERR;
	    return NULL;
	}
	if (rp->u_id == ID_SOLID) {
	    strncpy(rp->s.s_name, dp->d_namep, NAMESIZE);
	    if (state.miraxis != W) {
		rp->s.s_values[state.miraxis] += 2 * (state.mirpos - rp->s.s_values[state.miraxis]);
		for (j = 3+state.miraxis; i < 24; i++) {
		    rp->s.s_values[j] = -rp->s.s_values[j];
		}
	    }
	    if (state.trans[W]) {
		VADD2(rp->s.s_values, rp->s.s_values, state.trans);
	    }
	    if (state.rot[W]) {
		mat_t r;
		vect_t vec, ovec;

		if (state.rpnt[W]) {
		    VSUB2(rp->s.s_values, rp->s.s_values, state.rpnt);
		}
		mat_idn(r);
		mat_angles(r, state.rot[X], state.rot[Y], state.rot[Z]);
		for (i = 0; i < 24; i+=3) {
		    VMOVE(vec, rp->s.s_values+i);
		    MAT4X3VEC(ovec, r, vec);
		    VMOVE(rp->s.s_values+i, ovec);
		}
		if (state.rpnt[W]) {
		    VADD2(rp->s.s_values, rp->s.s_values, state.rpnt);
		}
	    }
	} else {
	    bu_log("mods not available on %s\n", proto->d_namep);
	}

	/* write the object to disk */
	if (db_put(dbip, dp, rp, 0, dp->d_len) < 0) {
	    bu_log("ERROR: clone internal error writing to the database\n");
	    return NULL;
	}

    }
    if (rp) {
	bu_free((char *)rp, "copy_solid record[]");
    }

    return dp;
}


/** recursively copy a tree of geometry */
struct directory *
copy_tree(Tcl_Interp *interp, struct clone_state state, struct directory *dp)
{
    register int i;
    register union record   *rp = (union record *)NULL;
    register struct directory *mdp = (struct directory *)NULL;
    register struct directory *copy = (struct directory *)NULL;

    /* copy the object */
    if (dp->d_flags & DIR_COMB) {
	
	/* A v4 method of getting the geometry */
	if (dbip->dbi_version < 5) {
	    /* get an in-memory record of this object */
	    if ((rp = db_getmrec(dbip, dp)) == (union record *)0) {
		TCL_READ_ERR;
		return NULL;
	    }
	    /* if it is a combination/region, recursively copy the objects
	       that make up the object */
	    for (i = 1; i < dp->d_len; i++ ) {
		if ((mdp = db_lookup(dbip, rp[i].M.m_instname, LOOKUP_NOISY)) == DIR_NULL) {
		    continue;
		}
		copy = copy_tree(interp, state, mdp);
	    }
	    
	} else {
	    /* A v5 method of getting the geometry */
	    Tcl_AppendResult(interp, "clone: command currently unimplemented for v5 geometry databases\n");
	}

	/* copy the combination itself */
	copy = copy_comb(dp, state);
    } else if (dp->d_flags & DIR_SOLID) {
	/* leaf node -- make a copy the object */
	copy = copy_solid(dp, state);
    } else {
	Tcl_AppendResult(interp, "clone:  ", dp->d_namep, " is neither COMB nor SOLID?\n", (char *)NULL);
    }

    if (rp) {
	bu_free((char *)rp, "copy_tree record[]");
    }
    return copy;
}


/** copy an object, recursivley copying all of the object's contents
 *  if it's a combination/region.
 */
struct directory *
copy_object(Tcl_Interp *interp, struct clone_state state)
{
    struct directory *copy = (struct directory *)NULL;
    struct nametbl *curr = (struct nametbl *)NULL;
    int i, j, idx;

    init_list(&obj_list, state.n_copies);

    /* do the actual copying */
    copy = copy_tree(interp, state, state.src);

    /* make sure it made what we hope/think it made */
    if (!copy || !is_in_list(obj_list, state.src->d_namep)) {
	bu_log("ERROR: clone internal error, cannot find %s for editing\n", state.src->d_namep);
	return copy;
    }

    /* display the cloned object(s) */
    if (state.draw_obj) {
	char *av[3] = {"e", NULL, NULL};

	idx = index_in_list(obj_list, state.src->d_namep);
	for (i = 0; i < (state.n_copies > obj_list.names_used ? obj_list.names_used : state.n_copies) ; i++) {
	    av[1] = obj_list.names[idx].dest[i];
	    /* draw does not use clientdata */
	    cmd_draw( (ClientData)NULL, interp, 2, av );
	}
    }

    /* release our name allocations */
    for (i = 0; i < obj_list.names_len; i++) {
	for (j = 0; j < obj_list.name_size; j++) {
	    bu_free((char *)obj_list.names[i].dest[j], "free dest[j]");
	}
	bu_free((char **)obj_list.names[i].dest, "free dest");
    }
    bu_free((struct name *)obj_list.names, "free names");

    /* better safe than sorry */
    obj_list.names = NULL;
    obj_list.name_size = obj_list.names_used = obj_list.names_len = 0;

    return copy;
}


/** how to use clone.  blissfully simple interface. */
void
print_usage(Tcl_Interp *interp)
{
    Tcl_AppendResult(interp, "Usage: clone [-abfhimnprtv] <object>\n\n", (char *)NULL);
    Tcl_AppendResult(interp, "-a <n> <x> <y> <z>\t- Specifies a translation split between n copies.\n", (char*)NULL);
    Tcl_AppendResult(interp, "-b <n> <x> <y> <z>\t- Specifies a rotation around x, y, and z axes \n\t\t\t  split between n copies.\n", (char*)NULL);
    Tcl_AppendResult(interp, "-f\t\t\t- Don't draw the new object.\n", (char *)NULL);
    Tcl_AppendResult(interp, "-h\t\t\t- Prints this message.\n", (char*)NULL);
    Tcl_AppendResult(interp, "-i <n>\t\t\t- Specifies the increment between each copy.\n", (char*)NULL);
    Tcl_AppendResult(interp, "-m <axis> <pos>\t\t- Specifies the axis and point to mirror the group.\n", (char*)NULL);
    Tcl_AppendResult(interp, "-n <# copies>\t\t- Specifies the number of copies to make.\n", (char*)NULL);
    Tcl_AppendResult(interp, "-p <x> <y> <z>\t\t- Specifies point to rotate around for -r. \n\t\t\t  Default is 0 0 0.\n", (char*)NULL);
    Tcl_AppendResult(interp, "-r <x> <y> <z>\t\t- Specifies the rotation around x, y, and z axes.\n", (char*)NULL);
    Tcl_AppendResult(interp, "-t <x> <y> <z>\t\t- Specifies translation between each copy.\n", (char*)NULL);
    Tcl_AppendResult(interp, "-v\t\t\t- Prints version info.\n", (char*)NULL);
    return;
}


/** process the user-provided arguments. stash their operations into
 *  our state structure.
 */
int
get_args(Tcl_Interp *interp, int argc, char **argv, struct clone_state *state)
{
    int i, k;

    bu_optind = 1;

    state->incr = 100;
    state->n_copies = 1;
    state->draw_obj = 1;
    state->rot[W] = 0;
    state->rpnt[W] = 0;
    state->trans[W] = 0;
    state->miraxis = W;
    while ((k = bu_getopt(argc, argv, "a:b:fhi:m:n:p:r:t:v")) != EOF) {
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
	    case 'f':
		state->draw_obj = 0;
		break;
	    case 'h':
		print_usage(interp);
		return TCL_ERROR;
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
	    case 'v':
		Tcl_AppendResult(interp, CLONE_VERSION, (char *)NULL);
		return TCL_ERROR;
		break;
	    default:
		print_usage(interp);
		return TCL_ERROR;
	}
    }

    /* make sure not too few/many args */
    if ((argc - bu_optind) == 0) {
	Tcl_AppendResult(interp, "clone:  Need to specify an <object> to be cloned.\n", (char*)NULL);
	print_usage(interp);
	return TCL_ERROR;
    } else if (bu_optind + 1 < argc) {
	Tcl_AppendResult(interp, "clone:  Can only clone exactly one <object> at a time right now.\n", (char*)NULL);
	print_usage(interp);
	return TCL_ERROR;
    }

    /* sanity */
    if (!argv[bu_optind]) {
	return TCL_ERROR;
    }
    
    /* we make sure the lookup succeeded in f_clone() */
    state->src = db_lookup(dbip, argv[bu_optind], LOOKUP_QUIET);
    if (!state->src) {
	Tcl_AppendResult(interp, "clone:  Cannot find source object\n", (char*)NULL);
	return TCL_ERROR;
    }

    VSCALE(state->trans, state->trans, local2base);
    VSCALE(state->rpnt, state->rpnt, local2base);
    state->mirpos *= local2base;

    return TCL_OK;
}


/** master hook function for the 'clone' command. */
int
f_clone(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    struct clone_state	state;

    /* allow interrupts */
    if( setjmp( jmp_env ) == 0 ) {
	(void)signal( SIGINT, sig3);
    } else {
	return TCL_OK;
    }

    if (argc < 2) {
	Tcl_AppendResult(interp, "clone:  Not enough args.  Use -h for help\n", (char*)NULL);
	return TCL_ERROR;
    }

    /* validate user options */
    if (get_args(interp, argc, argv, &state) == TCL_ERROR) {
	return TCL_ERROR;
    }

    /* do it */
    (void)copy_object(interp, state);

    (void)signal( SIGINT, SIG_IGN );
    return TCL_OK;
}


/** helper function that computes where a point is along a spline
 * given some distance 't'.
 *
 * i.e. sets pt = Q(t) for the specified spline
 */
void
interp_spl(fastf_t t, struct spline spl, vect_t pt)
{
    int i = 0;
    fastf_t s, s2, s3;

    if (t == spl.t[spl.n_segs]) {
	t -= VUNITIZE_TOL;
    }

    /* traverse to the spline segment interval */
    while (t >= spl.t[i+1]) {
	i++;
    }

    /* compute the t offset */
    t -= spl.t[i];
    s = t; s2 = t*t; s3 = t*t*t;

    /* solve for the position */
    pt[X] = spl.k[i].c[X][0] + spl.k[i].c[X][1]*s + spl.k[i].c[X][2]*s2 + spl.k[i].c[X][3]*s3;
    pt[Y] = spl.k[i].c[Y][0] + spl.k[i].c[Y][1]*s + spl.k[i].c[Y][2]*s2 + spl.k[i].c[Y][3]*s3;
    pt[Z] = spl.k[i].c[Z][0] + spl.k[i].c[Z][1]*s + spl.k[i].c[Z][2]*s2 + spl.k[i].c[Z][3]*s3;
}


/** master hook function for the 'tracker' command used to create
 *  copies of objects along a spline path.
 */
int
f_tracker(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    struct spline s;
    vect_t *verts  = (vect_t *)NULL;
    struct link *links = (struct link *)NULL;
    int i, j, k, inc;
    int n_verts, n_links, arg = 1;
    FILE *points = (FILE *)NULL;
    char tok[81] = {0}, line[81] = {0};
    char ch;
    fastf_t totlen = 0.0;
    fastf_t len, olen;
    fastf_t dist_to_next;
    fastf_t min, max, mid;
    fastf_t pt[3] = {0}, rot[3] = {0};
    int no_draw = 0;

    /* allow interrupts */
    if( setjmp( jmp_env ) == 0 ) {
	(void)signal( SIGINT, sig3 );
    } else {
	return TCL_OK;
    }

    bu_optind = 1;
    while ((i = bu_getopt(argc, argv, "fh")) != EOF) {
	switch (i) {
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
    for (i = arg; i < argc; i+=2) {
	strcpy(links[(i-arg)/2].name, argv[i]);
	if (argc > arg+1) {
	    sscanf(argv[i+1], "%lf", &links[(i-arg)/2].pct);
	} else {
	    links[(i-arg)/2].pct = 1.0;
	}
	totlen += links[(i-arg)/2].pct;
    }
    if (totlen != 1.0) {
	fprintf(stdout, "ERROR\n");
    }


    /* Read in knots from specified file *************/
    do {
	fgets(line, 81, points);
    } while (strcmp(strtok(line, ","), "112") != 0);

    strcpy(tok, strtok(NULL, ","));
    strcpy(tok, strtok(NULL, ","));
    strcpy(tok, strtok(NULL, ","));
    strcpy(tok, strtok(NULL, ","));
    s.n_segs = atoi(tok);
    s.t = (fastf_t *)bu_malloc(sizeof(fastf_t) * (s.n_segs+1), "t");
    s.k = (struct knot *)bu_malloc(sizeof(struct knot) * (s.n_segs+1), "k");
    for (i = 0; i <= s.n_segs; i++) {
	strcpy(tok, strtok(NULL, ","));
	if (strstr(tok, "P") != NULL) {
	    fgets(line, 81, points);
	    fgets(line, 81, points);
	    strcpy(tok, strtok(line, ","));
	}
	s.t[i] = atof(tok);
    }
    for (i = 0; i <= s.n_segs; i++) {
	for (j = 0; j < 3; j++) {
	    for (k = 0; k < 4; k++) {
		strcpy(tok, strtok(NULL, ","));
		if (strstr(tok, "P") != NULL) {
		    fgets(line, 81, points);
		    fgets(line, 81, points);
		    strcpy(tok, strtok(line, ","));
		}
		s.k[i].c[j][k] = atof(tok);
	    }
	    s.k[i].pt[j] = s.k[i].c[j][0];
	}
    }
    fclose(points);



    /* Interpolate link vertices *********************/
    for (i = 0; i < s.n_segs; i++) { /* determine initial track length */
	totlen += DIST_PT_PT(s.k[i].pt, s.k[i+1].pt);
    }
    len = totlen/(n_verts-1);
    VMOVE(verts[0], s.k[0].pt);
    olen = 2*len;

    for (i = 0; (fabs(olen-len) >= VUNITIZE_TOL) && (i < 250); i++) { /* number of track iterations */
	fprintf(stdout, ".");
	fflush(stdout);
	for (j = 0; j < n_links; j++) { /* set length of each link based on current track length */
	    links[j].len = len * links[j].pct;
	}
	min = 0;
	max = s.t[s.n_segs];
	mid = 0;

	for (j = 0; j < n_verts+1; j++) { /* around the track once */
	    for (k = 0; k < n_links; k++) { /* for each sub-link */
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
	}

	interp_spl(s.t[s.n_segs], s, verts[n_verts*n_links-1]);
	totlen = 0.0;
	for (j = 0; j < n_verts*n_links-1; j++) {
	    totlen += DIST_PT_PT(verts[j], verts[j+1]);
	}
	olen = len;
	len = totlen/(n_verts-1);
    }
    fprintf(stdout, "\n");

    /* Write out interpolation info ******************/
    fprintf(stdout, "%d Iterations; Final link lengths:\n", i);
    for (i = 0; i < n_links; i++) {
	fprintf(stdout, "  %s\t%.15lf\n", links[i].name, links[i].len);
    }
    fflush(stdin);
    /* Place links on vertices ***********************/
    fprintf(stdout, "Continue? [y/n]  ");
    fscanf(stdin, "%c", &ch);
    if (ch == 'y') {
	struct clone_state state;
	struct directory **dps = (struct directory **)NULL;
	fastf_t units[6] = {1, 1, 10, 1000, 25.4, 304.8};
	char *vargs[3];
	vect_t *rots;

	for (i = 0; i < 2; i++) {
	    vargs[i] = (char *)bu_malloc(sizeof(char)*NAMESIZE, "alloc vargs1");
	}

	strcpy(vargs[0], "e");
	strcpy(vargs[1], links[j].name);
	vargs[2] = NULL;

	state.incr = inc;
	state.n_copies = 1;
	state.draw_obj = 0;
	state.miraxis = W;

	dps = (struct directory **)bu_malloc(sizeof(struct directory *)*n_links, "alloc dps");
	/*		rots = (vect_t *)bu_malloc(sizeof(vect_t)*n_links, "alloc rots");*/
	for (i = 0; i < n_links; i++) {
	    dps[i] = db_lookup(dbip, links[i].name, LOOKUP_QUIET);
	    /*			VSET(rots[i], 0,0,0);*/
	}

	for (i = 0; i < n_verts-1; i++) {
	    for (j = 0; j < n_links; j++) {
		if (i == 0) {
		    VSCALE(state.trans, verts[n_links*i+j], local2base);
		} else {
		    VSUB2SCALE(state.trans, verts[n_links*(i-1)+j], verts[n_links*i+j], local2base);
		}
		VSCALE(state.rpnt, verts[n_links*i+j], local2base);

		VSUB2(pt, verts[n_links*i+j], verts[n_links*i+j+1]);
		VSET(state.rot, 0, (M_PI - atan2(pt[Z], pt[X])), 
		     -atan2(pt[Y], sqrt(pt[X]*pt[X]+pt[Z]*pt[Z])));
		VSCALE(state.rot, state.rot, radtodeg);
		/*				VSUB2(state.rot, state.rot, rots[j]);
						VADD2(rots[j], state.rot, rots[j]);*/

					
		state.src = dps[j];
		dps[j] = copy_object(interp, state);
		strcpy(vargs[1], dps[j]->d_namep);
		/*				strcpy(vargs[1], obj_list.names[index_in_list(obj_list, links[j].name)].dest[0]);*/

		if (!no_draw || !is_dm_null()) {
		    drawtrees(2, vargs, 1);
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
	for (i = 0; i < 2; i++) {
	    bu_free((char *)vargs[i], "free vargs[i]");
	}
	free(dps);
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
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
