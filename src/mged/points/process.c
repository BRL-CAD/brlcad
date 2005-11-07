/*                       P R O C E S S . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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

#define TOL 2.0

#define PRINT_SCRIPT 1


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

    for (i = 0; i < count; i++) {
	plt = &(*plta)[i];
	if (!plt || !plt->type) {
	    printf("WARNING: Unexpected NULL encountered while processing a point array\n");
	    continue;
	}

	if (!prev_plt) {
	    prev_plt = &(*plta)[i];
	    pltg = (point_line_t *) bu_malloc(sizeof(point_line_t), "begin point_line_t subgroup");
	    COPY_POINT_LINE_T(*pltg, *prev_plt);
	    points++;
	    marker = 0;
	    continue;
	}

	if (!pltg) {
	    pltg = (point_line_t *) bu_malloc(sizeof(point_line_t), "begin point_line_t subgroup");
	} else {
	    pltg = (point_line_t *) bu_realloc(pltg, sizeof(point_line_t) * (points + 1), "add subgroup point_line_t");
	}
	COPY_POINT_LINE_T(pltg[points], *plt);

	if (marker) {
	    /* gobble up repeats points used as a marker, average new point */
	    if (DIST_PT_PT(prev_plt->val, plt->val) < tolerance) {
		prev_plt->val[X] = (prev_plt->val[X] + plt->val[X]) / 2.0;
		prev_plt->val[Y] = (prev_plt->val[Y] + plt->val[Y]) / 2.0;
		prev_plt->val[Z] = (prev_plt->val[Z] + plt->val[Z]) / 2.0;
		INITIALIZE_POINT_LINE_T(*plt); /* poof */
		points++;
		continue;
	    }

	    if (process_group(&pltg, points - 1)) {
		bu_free((genptr_t)pltg, "end point_lint_t subgroup");
		pltg = NULL;
		prev_plt = NULL;
		points = 0;
		marker = 0;
		continue;
	    } else {
		/*		printf("process_group returned 0\n");*/
	    }

	    marker = 0;
	}

	if (DIST_PT_PT(prev_plt->val, plt->val) < tolerance) {
	    /*	    printf("%d: CLOSE DISTANCE of %f\n", plt->index, DIST_PT_PT(prev_plt->val, plt->val));*/
	    marker = points - 1;
	    (pltg[marker]).val[X] = (prev_plt->val[X] + plt->val[X]) / 2.0;
	    (pltg[marker]).val[Y] = (prev_plt->val[Y] + plt->val[Y]) / 2.0;
	    (pltg[marker]).val[Z] = (prev_plt->val[Z] + plt->val[Z]) / 2.0;
	    INITIALIZE_POINT_LINE_T(pltg[points]); /* poof */
	}

	prev_plt = plt;
	points++;
    }
}

int process_group(point_line_t **plta, int count) {
    int i;
    point_line_t *plt = NULL;
    int valid_count = 0;

    if (!plta) {
	printf("WARNING: Unexpected call to process_multi_group with a NULL point array\n");
	return 0;
    } 
    plt = &(*plta)[0]; /* use first for reference */

    /* count valid points and compress the array down */
    for (i = 0; i < count; i++) {
	plt = &(*plta)[i];

	if (plt && plt->type) {
	    if (valid_count != i) {
		COPY_POINT_LINE_T((*plta)[valid_count], *plt);
		INITIALIZE_POINT_LINE_T(*plt);
	    }
	    valid_count++;
	}
    }

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

    switch((*plta)[0].code) {
	case(PLATE):
	    return plate(plta, valid_count);
	case(ARB):
	    return arb(plta, valid_count);
	case(CYLINDER):
	    return cylinder(plta, valid_count);
	case(POINTS):
	    return points(plta, valid_count);
	case(SYMMETRY):
	    return points(plta, valid_count);
	case(PIPE):
	    return ppipe(plta, valid_count);
    }

    printf("WARNING, unsupported point code encountered (%d)\n", (*plta)[0].code);
    return 0;
}

static int print_array(point_line_t **plta, int count) {
    int i;
    point_line_t *plt = NULL;
    
    printf("\tBEGIN %s GROUP\n", (*plta)[0].type);
    for (i = 0; i < count; i++) {
	plt = &(*plta)[i];
	if (plt && plt->type) {
	    printf("\t\t%s %d: (%f,%f,%f)\n", plt->type, plt->index, plt->val[X], plt->val[Y], plt->val[Z]);
	} else {
	    printf("\t\tBOGUS POINT FOUND??\n");
	}
    }
    printf("\tEND %s GROUP\n", (*plta)[0].type);

    return 1;
}

int plate(point_line_t **plta, int count) {
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
#else
    Tcl_Eval(twerp, bu_vls_addr(&vls2));
    if (twerp->result[0] != '\0') {
	bu_log("plate failure: %s\n", twerp->result);
    } else {
	bu_log("plate created\n");
    }
#endif

    return 1;
}

int arb(point_line_t **plta, int count) {
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
#else
    Tcl_Eval(twerp, bu_vls_addr(&vls2));
    if (twerp->result[0] != '\0') {
	bu_log("arb failure: %s\n", twerp->result);
    } else {
	bu_log("arb created\n");
    }
#endif

    return 1;
}
int cylinder(point_line_t **plta, int count) {
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
#else
    Tcl_Eval(twerp, bu_vls_addr(&vls2));
    if (twerp->result[0] != '\0') {
	bu_log("cylinder failure: %s\n", twerp->result);
    } else {
	bu_log("cylinder created\n");
    }
#endif

    return 1;
}
int ppipe(point_line_t **plta, int count) {
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
#else
    Tcl_Eval(twerp, bu_vls_addr(&vls2));
    if (twerp->result[0] != '\0') {
	bu_log("pipe failure: %s\n", twerp->result);
    } else {
	bu_log("pipe created\n");
    }
#endif

    return 1;
}
int points(point_line_t **plta, int count) {
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
#else
    Tcl_Eval(twerp, bu_vls_addr(&vls2));
    if (twerp->result[0] != '\0') {
	bu_log("points failure: %s\n", twerp->result);
    } else {
	bu_log("points created\n");
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
