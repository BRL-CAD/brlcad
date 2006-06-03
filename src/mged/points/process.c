/*                       P R O C E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
 *
 */
/** @file process.c
 *
 * Data structures for the comma-separated value point file parser.
 *
 * Author -
 *   Christopher Sean Morrison
 */

#include "common.h"

/* interface header */
#include "./process.h"

#include <stdio.h>
#ifdef HAVE_STDINT_H
#  include <stdint.h>
#else
#  ifdef HAVE_INTTYPES_H
#    include <inttypes.h>
#  endif
#endif

#include "bu.h"
#include "vmath.h"

extern Tcl_Interp *twerp;

/* some os's (like OpenBSD) lack an INT32_MAX define. Cross fingers and hope 
 * that INT_MAX exists and is 32b...  */
#ifndef INT32_MAX
#  ifdef INT_MAX
#    define INT32_MAX INT_MAX
#  else
#    define INT32_MAX 0x7fffffff
#  endif
#endif

#define TOL 1.5

#define PRINT_DEBUG 1
#define PRINT_SCRIPT 1
#define RUN_SCRIPT 1

static int print_array(point_line_t **plta, int count) {
    int i;
    point_line_t *plt = NULL;
    
    for (i = 0; i < count; i++) {
	plt = &(*plta)[i];
	if (plt && plt->type) {
	    printf("\t%s %d: (%f,%f,%f)\n", plt->type, plt->index, plt->val[X], plt->val[Y], plt->val[Z]);
	} else {
	    printf("\tNULL POINT\n");
	}
    }

    return 1;
}


void process_value(point_line_t *plt, double value)
{
    if (!plt) {
	printf("WARNING: Unexpected call to process_value with a NULL point structure\n");
	return;
    }

    if (plt->count > Z) {
	printf("WARNING: Unexpected call to process_value with a full point structure\n");
	return;
    }

    plt->val[plt->count++] = value;
    
    return;
}

void process_type(point_line_t *plt, const char *type, int code)
{
    if (!plt) {
	printf("WARNING: Unexpected call to process_value with a NULL point structure\n");
	return;
    }

    plt->type = type;
    plt->code = code;;
    
    return;
}

void process_point(point_line_t *plt) {
    static int code_state = INT32_MAX;
    static int points = 0;
    static point_line_t *plta = NULL;

    if (!plt) {
	printf("WARNING: Unexpected call to process_point with a NULL point structure\n");
	return;
    }

    /* state change, we're either starting or ending */
    if (code_state != plt->code) {
	if (points > 0) {
	    process_multi_group(&plta, points, TOL);
	    printf("END OF BLOCK %d\n", code_state);

	    /* finish up this batch */
	    bu_free((genptr_t)plta, "end point_line_t group");
	    plta = NULL;
	}

	if (plt->type) {
	    printf("BEGIN OF BLOCK %s (%d)\n", plt->type, plt->code);
	}

	/* get ready for the new batch */
	code_state = plt->code;
	points = 0;
    }

    /* allocate room for the new point */
    if (!plta) {
	plta = (point_line_t *) bu_malloc(sizeof(point_line_t), "begin point_line_t group");
    } else {
	plta = (point_line_t *) bu_realloc(plta, sizeof(point_line_t) * (points + 1), "add point_line_t");
    }
    COPY_POINT_LINE_T(plta[points], *plt);
    points++;
}


int condense_points(point_line_t **plta, int count) {
    int i;
    point_line_t *plt = NULL;
    int valid_count = 0;

    if (!plta) {
	printf("WARNING: Unexpected call to delete_points with a NULL point array\n");
	return 0;
    }

    for (i=0; i < count; i++) {
	plt = &(*plta)[i];

	if (plt && plt->type) {
	    if (valid_count != i) {
		COPY_POINT_LINE_T((*plta)[valid_count], *plt);
		/* zap */
		INITIALIZE_POINT_LINE_T(*plt);
	    }
	    valid_count++;
	}
	
    }

#if PRINT_DEBUG
    if (valid_count != count) {
	bu_log("Started with %d points, condensed to %d points\n", count, valid_count);
    }
#endif

    return valid_count;
}


int delete_points(point_line_t **plta, int count, double tolerance) {
    int i;
    point_line_t *plt = NULL;
    point_line_t *previous_plt = NULL;
    /*    point_line_t average_plt; */
    int repeats = 0;
    int repeat_counter = 0;
    int removed = 0;

    if (!plta) {
	printf("WARNING: Unexpected call to delete_points with a NULL point array\n");
	return 0;
    }
    
    if (count < 6) {
	printf("WARNING: Unexpected call to delete_points with insufficient points\n");
	return 0;
    }

    /*    INITIALIZE_POINT_LINE_T(average_plt); */
    previous_plt = &(*plta)[0];

    for (i=1; i < count; i++) {
	plt = &(*plta)[i];
	
	if (DIST_PT_PT(previous_plt->val, plt->val) < tolerance) {
	    repeats++;
	} else {
	    /* not a repeat, so check if we need to remove the repeats
             * and the previous as convention.
             */
	    if (repeats >= 4) {
		/* 5+ repeated values in a row */
		repeat_counter = 1;
		while (repeats >= 0 && repeat_counter <= count) {
		    plt = &(*plta)[i-repeat_counter];
		    if (plt && plt->type) {
			/* zap */
#if PRINT_DEBUG
			bu_log("removed point: %d\n", plt->index);
#endif
			INITIALIZE_POINT_LINE_T(*plt);
			repeats--;
		    }
		    repeat_counter++;
		}

		/* we're not necessarily condensed, so search for the
		 * first non-null point and delete it as well.
		 */
		plt = &(*plta)[i-repeat_counter];
		while (!plt || !plt->type) {
		    repeat_counter--;
		    plt = &(*plta)[i-repeat_counter];
		}
		/* zap */
		bu_log("removed REAL point: %d\n", plt->index);
		INITIALIZE_POINT_LINE_T(*plt);

		removed++;
	    }
	    repeats = 0;
	}

	previous_plt = plt;
    }

#if PRINT_DEBUG
    if (removed > 0) {
	bu_log("Found and removed %d invalid points\n", removed);
    }
#endif


#if 0
    bu_log("--- BEFORE ---\n");
    print_array(plta, count);
#endif

    /* resort the list, put nulls at the end */
    count = condense_points(plta, count);

#if 0
    bu_log("--- AFTER ---\n");
    print_array(plta, count);
#endif

    return count;
}


/**
 * handle a group of points of a particular type, with potentially
 * multiple sets delimited by triplicate points.
 */
void process_multi_group(point_line_t **plta, int count, double tolerance) {
    int i;
    point_line_t *plt = NULL;

    int points = 0;
    point_line_t *pltg = NULL;

    int marker = 0;
    point_line_t *prev_plt = NULL;

    if (!plta) {
	printf("WARNING: Unexpected call to process_multi_group with a NULL point array\n");
	return;
    }

#if 0
    static int print_counter = 0;
    if (print_counter == 0) {
	bu_log("--- BEFORE ---\n");
	print_array(plta, count);
    }
#endif

    /* remove points marked as bogus, 5-identical points in succession */
    count = delete_points(plta, count, tolerance);

#if 0
    if (print_counter == 0) {
	print_counter++;
	bu_log("--- AFTER ---\n");
	print_array(plta, count);
    }
#endif

    /* isolate groups and pass them on to the group processing routine */
    for (i = 0; i < count; i++) {
	plt = &(*plta)[i];
	if (!plt || !plt->type) {
	    printf("WARNING: Unexpected NULL encountered while processing a point array (%d of %d)\n", i, count);
	    continue;
	}

	/* if this is the first point of a group, allocate and initialize */
	if (!prev_plt) {
	    prev_plt = &(*plta)[i];
	    pltg = (point_line_t *) bu_malloc(sizeof(point_line_t), "begin point_line_t subgroup");
	    COPY_POINT_LINE_T(*pltg, *prev_plt);
	    marker = 0;
	    continue;
	}

	if (marker) {
	    /* gobble up repeats points used as a marker, average new point */
	    if (DIST_PT_PT(prev_plt->val, plt->val) < tolerance) {
		prev_plt->val[X] = (prev_plt->val[X] + plt->val[X]) / 2.0;
		prev_plt->val[Y] = (prev_plt->val[Y] + plt->val[Y]) / 2.0;
		prev_plt->val[Z] = (prev_plt->val[Z] + plt->val[Z]) / 2.0;
		INITIALIZE_POINT_LINE_T(*plt); /* poof */
		continue;
	    }

	    if (process_group(&pltg, points+1)) {
		bu_free((genptr_t)pltg, "end subgroup: point_line_t");
		pltg = NULL;
		prev_plt = NULL;
		points = 0;
		marker = 0;
		--i;
		continue;
	    } else {
		/* process_group is allowed to return non-zero if
		   there are not enough points -- they get returned to
		   the stack for processing again */
		printf("warning, process_group returned 0\n"); 
	    }

	    marker = 0;
	    continue;
	}

	/* FIXME: shouldn't just average to the average, later points
	   get weighted too much.. */
	if (DIST_PT_PT(prev_plt->val, plt->val) < tolerance) {
	    /*	    printf("%d: CLOSE DISTANCE of %f\n", plt->index, DIST_PT_PT(prev_plt->val, plt->val));*/
	    marker = points;
	    (pltg[marker]).val[X] = (prev_plt->val[X] + plt->val[X]) / 2.0;
	    (pltg[marker]).val[Y] = (prev_plt->val[Y] + plt->val[Y]) / 2.0;
	    (pltg[marker]).val[Z] = (prev_plt->val[Z] + plt->val[Z]) / 2.0;
	    continue;
	}

	if (!pltg) {
	    printf("Blah! Error. Group array is null. Shouldn't be here!\n");
	    return;
	}
	
	pltg = (point_line_t *) bu_realloc(pltg, sizeof(point_line_t) * (points + 2), "add subgroup: point_line_t");

	points++;
	COPY_POINT_LINE_T(pltg[points], *plt);
	prev_plt = plt;
    }
    printf("i: %d, count: %d", i, count);

    /* make sure we're not at the end of a list (i.e. no end marker,
       but we're at the end of this group */
    if (points > 0) {
	if (process_group(&pltg, points+1)) {
	    bu_free((genptr_t)pltg, "end point_line_t subgroup");
	    pltg = NULL;
	    prev_plt = NULL;
	    points = 0;
	    marker = 0;
	} else {
	    /* this one shouldn't return zero, we're at the end of a multiblock */
	    printf("ERROR, process_group returned 0\n");
	}
    }

}


/** wrapper func to validate the block of points being processed and to
 * call the appropriate handler.
 */
int
process_group(point_line_t **plta, int count) {
    int valid_count = 0;

    if (!plta) {
	printf("WARNING: Unexpected call to process_multi_group with a NULL point array\n");
	return 0;
    } 
    plt = &(*plta)[0]; /* use first for reference */

    bu_log("processing a group!\n");

    /* resort the list, put nulls at the end */
    valid_count = condense_points(plta, count);

    /* ignore insufficient counts */
    if (valid_count <= 2) {
	switch((*plta)[0].code) {
	    case(PLATE): /* need at least 3 (triangle) */
		/*		printf("IGNORING PLATE POINT DUPLICATE(S)\n"); */
		return 0;
	    case(ARB): /* need 8 */
		/*		printf("IGNORING ARB POINT DUPLICATE(S)\n");*/
		return 0;
	    case(CYLINDER): /* need at least 3 (2 for length + diam) */
		/* printf("IGNORING CYLINDER POINT DUPLICATE(S)\n"); */
		return 0;
	}
    }

    /* FIXME: callbacks should really be registered in the lexer or
       parser when a point-line of that particular type is
       encountered
    */
    switch((*plta)[0].code) {
	case(PLATE):
	    return create_plate(plta, valid_count);
	case(ARB):
	    return create_arb(plta, valid_count);
	case(CYLINDER):
	    return create_cylinder(plta, valid_count);
	case(CYL):
	    return create_cyl(plta, valid_count);
	case(POINTS):
#if 0
    static int print_counter = 0;
    if (print_counter == 0) {
	bu_log("--- POINTS ---\n");
	print_array(plta, count);
    }
#endif
	    return create_points(plta, valid_count);
	case(SYMMETRY):
	    return create_points(plta, valid_count);
	case(PIPE):
	    return create_pipe(plta, valid_count);
	case(SPHERE):
	    return create_sphere(plta, valid_count);
    }

    printf("WARNING, unsupported point code encountered (%d)\n", (*plta)[0].code);
    return 0;
}


/***
 * process each of the individual creation types
 ***/

int create_plate(point_line_t **plta, int count) {
    int i;
    point_line_t *plt = NULL;

    struct bu_vls vls;
    struct bu_vls vls2;

    bu_vls_init(&vls);
    bu_vls_init(&vls2);

    for (i = 0; i < count; i++) {
	plt = &(*plta)[i];
	if (plt && plt->type) {
	    bu_vls_printf(&vls, "{ %f %f %f } ", plt->val[X], plt->val[Y], plt->val[Z]);
	}
    }
    bu_vls_printf(&vls2, "plate { %S }", &vls);
#if PRINT_SCRIPT
    fprintf(stderr, "%s\n", bu_vls_addr(&vls2));
#endif
#if RUN_SCRIPT
    Tcl_Eval(twerp, bu_vls_addr(&vls2));
    if (twerp->result[0] != '\0') {
	bu_log("create_plate failure: %s\n", twerp->result);
    } else {
	bu_log("create_plate created\n");
    }
#endif

    return 1;
}

int create_arb(point_line_t **plta, int count) {
    int i;
    point_line_t *plt = NULL;

    struct bu_vls vls;
    struct bu_vls vls2;

    bu_vls_init(&vls);
    bu_vls_init(&vls2);

    for (i = 0; i < count; i++) {
	plt = &(*plta)[i];
	if (plt && plt->type) {
	    bu_vls_printf(&vls, "{ %f %f %f } ", plt->val[X], plt->val[Y], plt->val[Z]);
	}
    }
    bu_vls_printf(&vls2, "arb { %S }", &vls);
#if PRINT_SCRIPT
    fprintf(stderr, "%s\n", bu_vls_addr(&vls2));
#endif
#if RUN_SCRIPT
    Tcl_Eval(twerp, bu_vls_addr(&vls2));
    if (twerp->result[0] != '\0') {
	bu_log("create_arb failure: %s\n", twerp->result);
    } else {
	bu_log("create_arb created\n");
    }
#endif

    return 1;
}
int create_cylinder(point_line_t **plta, int count) {
    int i;
    point_line_t *plt = NULL;

    struct bu_vls vls;
    struct bu_vls vls2;

    bu_vls_init(&vls);
    bu_vls_init(&vls2);

    for (i = 0; i < count; i++) {
	plt = &(*plta)[i];
	if (plt && plt->type) {
	    bu_vls_printf(&vls, "{ %f %f %f } ", plt->val[X], plt->val[Y], plt->val[Z]);
	}
    }
    bu_vls_printf(&vls2, "cyls { %S }", &vls);
#if PRINT_SCRIPT
    fprintf(stderr, "%s\n", bu_vls_addr(&vls2));
#endif
#if RUN_SCRIPT
    Tcl_Eval(twerp, bu_vls_addr(&vls2));
    if (twerp->result[0] != '\0') {
	bu_log("create_cylinder failure: %s\n", twerp->result);
    } else {
	bu_log("create_cylinder created\n");
    }
#endif

    return 1;
}
/* FIXME: not verified in the least bit */
int create_cyl(point_line_t **plta, int count) {
    int i;
    point_line_t *plt = NULL;

    struct bu_vls vls;
    struct bu_vls vls2;

    bu_vls_init(&vls);
    bu_vls_init(&vls2);

    for (i = 0; i < count; i++) {
	plt = &(*plta)[i];
	if (plt && plt->type) {
	    bu_vls_printf(&vls, "{ %f %f %f } ", plt->val[X], plt->val[Y], plt->val[Z]);
	}
    }
    bu_vls_printf(&vls2, "cylinder { %S }", &vls);
#if PRINT_SCRIPT
    fprintf(stderr, "%s\n", bu_vls_addr(&vls2));
#endif
#if RUN_SCRIPT
    Tcl_Eval(twerp, bu_vls_addr(&vls2));
    if (twerp->result[0] != '\0') {
	bu_log("create_cyl failure: %s\n", twerp->result);
    } else {
	bu_log("create_cyl created\n");
    }
#endif

    return 1;
}
int create_pipe(point_line_t **plta, int count) {
    int i;
    point_line_t *plt = NULL;

    struct bu_vls vls;
    struct bu_vls vls2;

    bu_vls_init(&vls);
    bu_vls_init(&vls2);

    for (i = 0; i < count; i++) {
	plt = &(*plta)[i];
	if (plt && plt->type) {
	    bu_vls_printf(&vls, "{ %f %f %f } ", plt->val[X], plt->val[Y], plt->val[Z]);
	}
    }
    bu_vls_printf(&vls2, "pipe { %S }", &vls);
#if PRINT_SCRIPT
    fprintf(stderr, "%s\n", bu_vls_addr(&vls2));
#endif
#if RUN_SCRIPT
    Tcl_Eval(twerp, bu_vls_addr(&vls2));
    if (twerp->result[0] != '\0') {
	bu_log("create_pipe failure: %s\n", twerp->result);
    } else {
	bu_log("create_pipe created\n");
    }
#endif

    return 1;
}
/* FIXME: takes a list of points, not triplets */
int create_sphere(point_line_t **plta, int count) {
    int i;
    point_line_t *plt = NULL;

    struct bu_vls vls;
    struct bu_vls vls2;

    bu_vls_init(&vls);
    bu_vls_init(&vls2);

    for (i = 0; i < count; i++) {
	plt = &(*plta)[i];
	if (plt && plt->type) {
	    bu_vls_printf(&vls, "{ %f %f %f } ", plt->val[X], plt->val[Y], plt->val[Z]);
	}
    }
    bu_vls_printf(&vls2, "sph { %S }", &vls);
#if PRINT_SCRIPT
    fprintf(stderr, "%s\n", bu_vls_addr(&vls2));
#endif
#if RUN_SCRIPT
    Tcl_Eval(twerp, bu_vls_addr(&vls2));
    if (twerp->result[0] != '\0') {
	bu_log("create_cylinder failure: %s\n", twerp->result);
    } else {
	bu_log("create_cylinder created\n");
    }
#endif

    return 1;
}
int create_points(point_line_t **plta, int count) {
    int i;
    point_line_t *plt = NULL;

    struct bu_vls vls;
    struct bu_vls vls2;

    bu_vls_init(&vls);
    bu_vls_init(&vls2);

    for (i = 0; i < count; i++) {
	plt = &(*plta)[i];
	if (plt && plt->type) {
	    bu_vls_printf(&vls, "{ %f %f %f } ", plt->val[X], plt->val[Y], plt->val[Z]);
	}
    }
    bu_vls_printf(&vls2, "points { %S }", &vls);
#if PRINT_SCRIPT
    fprintf(stderr, "%s\n", bu_vls_addr(&vls2));
#endif
#if RUN_SCRIPT
    Tcl_Eval(twerp, bu_vls_addr(&vls2));
    if (twerp->result[0] != '\0') {
	bu_log("create_points failure: %s\n", twerp->result);
    } else {
	bu_log("create_points created\n");
    }
#endif

    return 1;
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
