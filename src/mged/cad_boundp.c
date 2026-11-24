/*                    C A D _ B O U N D P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @file mged/cad_boundp.c
 *
 * cad_boundp -- bounding polygon of two-dimensional view
 *
 * This program is somewhat slow when operating on large input sets.
 * The following ideas should improve the speed, possibly at the
 * expense of maximum data set size:
 *
 * (1) Merge the Chop() and Build() phases.  Since Chop() examines
 * segment endpoints anyway, it is in a good position to build the
 * linked lists used by Search().
 *
 * (2) Sort the input endpoints into a double tree on X and Y.  This
 * should cut Chop() from order N ^ 2 to N * log(N).
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "vmath.h"
#include "bu/arg.h"
#include "bu/app.h"
#include "bu/str.h"
#include "bu/exit.h"
#include "bu/log.h"


typedef struct
{
    /* The following are float instead of double, to save space: */
    float x;		/* X coordinate */
    float y;		/* Y coordinate */
} coords; 		/* view-coordinates of point */

typedef struct segment
{
    struct segment *links; 	/* segment list thread */
    coords sxy;		/* (X, Y) coordinates of start */
    coords exy;		/* (X, Y) coordinates of end */
} segment;		/* entry in list of segments */

struct queue;				/* (to clear from name space) */

typedef struct point
{
    struct point *linkp; 	/* point list thread */
    struct queue *firstq;	/* NULL once output */
    coords xy;		/* (X, Y) coordinates of point */
} point;			/* entry in list of points */

typedef struct queue
{
    struct queue *nextq; 	/* endpoint list thread */
    point *endpoint;	/* -> endpt of ray from point */
} queue;			/* entry in list of endpoints */

static int Build(void), Chop(void), EndPoint(coords *p, segment *segp), GetArgs(int argc, const char *argv[]), Input(segment *inp),
    Near(coords *ap, coords *bp), Search(void), Split(coords *p, segment *oldp, segment *listh);
static coords *Intersect(segment *a, segment *b);
static point *LookUp(coords *coop), *NewPoint(coords *coop), *PutList(coords *coop);
static void * Alloc(unsigned int size);
static queue *Enqueue(point *addp, point *startp);
static void Output(coords *coop), Toss(void * ptr);

static int initial = 1; 	/* false after first Output */
static int vflag = 0;		/* set if "-v" option found */
static double tolerance = 0.0;	/* point matching slop */
static double tolsq = 0.0;		/* `tolerance' ^ 2 */
static point *headp = NULL;		/* head of list of points */
static segment seghead = {
    &seghead,
    {0.0f, 0.0f},
    {0.0f, 0.0f}
};	/* segment list head */

struct cad_boundp_args {
    int help;
    int verbose;
    const char *input;
    const char *output;
    double tolerance;
};

static const struct bu_arg_option cad_boundp_options[] = {
    BU_ARG_FLAG("h", "help", struct cad_boundp_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_VALUE("i", "input", struct cad_boundp_args, input,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file", "Read segments from a file"),
    BU_ARG_VALUE("o", "output", struct cad_boundp_args, output,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file", "Write polygon data to a file"),
    BU_ARG_NUMBER_RANGE("t", "tolerance", struct cad_boundp_args, tolerance,
	1, 0.0, 0, 0.0, "distance", "Point matching tolerance"),
    BU_ARG_FLAG("v", "segments", struct cad_boundp_args, verbose,
	"Write closed polygon segments instead of vertices"),
    BU_ARG_OPTION_NULL
};
static const char * const cad_boundp_input_option[] = {"input", NULL};
static const char * const cad_boundp_output_option[] = {"output", NULL};
static const char * const cad_boundp_tolerance_option[] = {"tolerance", NULL};
static const struct bu_arg_constraint cad_boundp_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_OCCURRENCE_COUNT,
	BU_ARG_CONDITION_ALWAYS, cad_boundp_input_option, 0, 1,
	"--input may be specified only once"),
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_OCCURRENCE_COUNT,
	BU_ARG_CONDITION_ALWAYS, cad_boundp_output_option, 0, 1,
	"--output may be specified only once"),
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_OCCURRENCE_COUNT,
	BU_ARG_CONDITION_ALWAYS, cad_boundp_tolerance_option, 0, 1,
	"--tolerance may be specified only once"),
    BU_ARG_CONSTRAINT_NULL
};

static const struct bu_arg_schema cad_boundp_schema = BU_ARG_SCHEMA(
    "cad_boundp", "Find the bounding polygon of two-dimensional segments",
    cad_boundp_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, cad_boundp_constraints, NULL, NULL, NULL));

static const struct bu_arg_grammar cad_boundp_grammar = {
    &cad_boundp_schema, NULL, NULL
};


int
main(int argc, const char *argv[])			/* "cad_boundp" entry point */
    /* argument count */
    /* argument strings */
{
    int arg_status;

    bu_setprogname(argv[0]);

    arg_status = GetArgs(argc, argv);
    if (arg_status <= 0)
	return arg_status < 0 ? 1 : 0;

    if (!Chop())		/* input; chop into segments */
	return 2;

    if (!Build()) 		/* build linked lists */
	return 3;

    if (!Search())		/* output bounding polygon */
	return 4;

    return 0;			/* success! */
}


static int
GetArgs(int argc, const char *argv[])	/* process command arguments */
    /* argument count */
    /* argument strings */
{
    struct cad_boundp_args args = {0, 0, NULL, NULL, 0.0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int introspection;
    int operand_index;

#ifdef CAD_BOUNDP_DEBUG
    fprintf(stderr, "\n\t\tGetArgs\n");
#endif
    introspection = bu_arg_introspect(&msg, &cad_boundp_grammar, argc - 1,
	argv + 1);
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : -1;
    }
    operand_index = bu_arg_schema_parse_complete(&cad_boundp_schema, &args,
	&msg, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_log("%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return -1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&cad_boundp_schema, "cad_boundp");
	if (help) {
	    bu_flog(stdout, "%s", help);
	    bu_free(help, "cad_boundp help");
	}
	return 0;
    }
    if (args.input && !BU_STR_EQUAL(args.input, "-") &&
	freopen(args.input, "r", stdin) == NULL) {
	bu_log("cad_boundp: cannot open \"%s\" for reading\n", args.input);
	return -1;
    }
    if (args.output && !BU_STR_EQUAL(args.output, "-") &&
	freopen(args.output, "w", stdout) == NULL) {
	bu_log("cad_boundp: cannot open \"%s\" for writing\n", args.output);
	return -1;
	}
    tolerance = args.tolerance;
    tolsq = tolerance * tolerance;
    vflag = args.verbose;

    return 1;
}


/*
  The following phase is required to chop up line segments that
  intersect; this guarantees that all polygon vertices will be found
  at segment endpoints.  Unfortunately, this is an N^2 process.  It is
  also hard to do right; thus the elaborateness.
*/

static int
Chop(void)					/* chop vectors into segments */
{
    segment *inp;			/* -> input whole segment */

#ifdef CAD_BOUNDP_DEBUG
    fprintf(stderr, "\n\t\tChop\n");
#endif
    /* Although we try to Alloc() when no input remains, it is more
       efficient than repeatedly copying the input into *inp.     */
    while ((inp = (segment *)Alloc((unsigned)sizeof(segment))) != NULL
	   && Input(inp)
	) {
	segment *segp;	/* segment list entry */
	segment piecehead;
	/* list of inp pieces */
	segment *pp;	/* -> pieces of `inp' */

	/* Reverse the segment if necessary to get endpoints
	   in left-to-right order; speeds up range check. */

	if (inp->sxy.x > inp->exy.x) {
	    coords temp;		/* store for swapping */

#ifdef CAD_BOUNDP_DEBUG
	    fprintf(stderr, "  endpoints swapped\n");
#endif
	    temp = inp->sxy;
	    inp->sxy = inp->exy;
	    inp->exy = temp;
	}

	/* Split input and segment list entries into pieces. */

	inp->links = &piecehead;
	piecehead.links = inp;	/* initially, one piece */

	/* Note:  Although the split-off pieces of `segp' are
	   linked in front of `segp', that's okay since they
	   cannot intersect `inp' again! */
	for (segp = seghead.links; segp != &seghead;
	     segp = segp->links
	    )
	    /* Similarly, once a piece of `inp' intersects
	       `segp', we can stop looking at the pieces. */
	    for (pp = piecehead.links; pp != &piecehead;
		 pp = pp->links
		) {
		coords *i;	/* intersection */

		/* Be careful; `i' -> static storage
		   internal to Intersect().	      */
		i = Intersect(pp, segp);
		if (i != NULL) {
		    if ((!EndPoint(i, pp) && !Split(i, pp, &piecehead))
			    || (!EndPoint(i, segp) && !Split(i, segp, &seghead))) {
			/* out of memory */
			Toss((void *)piecehead.links);
			return 0;
		    }

		    break;	/* next `segp' */
		}
	    }
#ifdef CAD_BOUNDP_DEBUG
	fprintf(stderr, "  new input pieces:");
	for (pp = piecehead.links; pp != &piecehead;
	     pp = pp->links
	    )
	    fprintf(stderr, "\t(%g, %g) -> (%g, %g)",
		    (double)pp->sxy.x,
		    (double)pp->sxy.y,
		    (double)pp->exy.x,
		    (double)pp->exy.y
		);
	fprintf(stderr, "\n  other segments:");
	for (segp = seghead.links; segp != &seghead;
	     segp = segp->links
	    )
	    fprintf(stderr, "\t(%g, %g) -> (%g, %g)",
		    (double)segp->sxy.x,
		    (double)segp->sxy.y,
		    (double)segp->exy.x,
		    (double)segp->exy.y
		);
#endif
	fprintf(stderr, "\n");

	/* Put input segment pieces into segment list. */

	/* Note:  It is better to scan the pieces to find the
	   tail link than to waste storage on double links. */
	for (pp = piecehead.links; pp->links != &piecehead;
	     pp = pp->links
	    )
	    ;
	pp->links = seghead.links;	/* last-piece link */
	seghead.links = piecehead.links;	/* add pieces */
    }
    Toss((void *)inp);		/* unused segment at EOF */

    return inp != NULL;
}


static int
Split(coords *p, segment *oldp, segment *listh) 		/* split segment in two */
    /* -> break point */
    /* -> segment to be split */
    /* -> list to attach tail to */
{
    segment *newp;	/* -> new list entry */

#ifdef CAD_BOUNDP_DEBUG
    fprintf(stderr, "  split (%g, %g) -> (%g, %g) at (%g, %g)\n",
	    (double)oldp->sxy.x, (double)oldp->sxy.y,
	    (double)oldp->exy.x, (double)oldp->exy.y,
	    (double)p->x, (double)p->y
	);
#endif
    if ((newp = (segment *)Alloc((unsigned)sizeof(segment))) == NULL)
	return 0;		/* out of heap space */
    newp->links = listh->links;
    newp->sxy = *p;
    newp->exy = oldp->exy;
    oldp->exy = *p; 		/* old entry, new endpoint */
    listh->links = newp;		/* attach to list head */

    return 1;
}


static int
Build(void) 				/* build linked lists */
{
    segment *listp; /* -> segment list entry */
    segment *deadp; /* -> list entry to be freed */

#ifdef CAD_BOUNDP_DEBUG
    fprintf(stderr, "\n\t\tBuild\n");
#endif
    /* When we are finished, `seghead' will become invalid. */
    for (listp = seghead.links; listp != &seghead;
	 deadp = listp, listp = listp->links, Toss((void *)deadp)
	) {
	point *startp, *endp; /* -> segment endpts */

	if ((startp = PutList(&listp->sxy)) == NULL
	    || (endp   = PutList(&listp->exy)) == NULL
	    || Enqueue(startp, endp) == NULL
	    || Enqueue(endp, startp) == NULL
	    )
	    return 0;	/* out of heap space */
    }

    return 1;
}


static int
Search(void)				/* output bounding polygon */
{
    double from;		/* backward edge direction */
    point *currentp;	/* -> current (start) point */
    point *previousp;	/* -> previous vertex point */

#ifdef CAD_BOUNDP_DEBUG
    fprintf(stderr, "\n\t\tSearch\n");
#endif
    if (headp == NULL)
	return 1;		/* trivial case */

    /* Locate the lowest point; this is the first polygon vertex. */
    {
	float miny;		/* smallest Y coordinate */
	point *listp; 	/* -> next point in list */

	currentp = listp = headp;
	miny = currentp->xy.y;
	while ((listp = listp->linkp) != NULL)
	    if (listp->xy.y < miny) {
		miny = listp->xy.y;
		currentp = listp;
	    }
    }
    previousp = NULL;		/* differs from currentp! */
    from = 270.0;

    /* Output point and look for next CCW point on periphery. */

    for (;;) {
	coords first = {0.0, 0.0}; /* first output if -v */
	double mindir = 0.0; /* smallest from->to angle */
	point *nextp = (point *)NULL; /* -> next perimeter point */
	queue *endq = NULL; /* -> endpoint queue entry */

#ifdef CAD_BOUNDP_DEBUG
	fprintf(stderr, "from %g", from);
#endif

	// If we don't have currentp, no way to proceed
	if (!currentp)
	    return 1;

	endq = currentp->firstq;
	if (endq == NULL) {
	    if (vflag && !initial)
		Output(&first);	/* closure */

	    return 1;	/* been here before => done */
	}

	Output(&currentp->xy);	/* found vertex */
	if (vflag && initial) {
	    initial = 0;
	    first = currentp->xy;	/* save for closure */
	}

	/* Find the rightmost forward edge endpoint. */

	mindir = 362.0;
	do {
	    double to;	/* forward edge dir */
	    double diff;	/* angle from->to */
	    point *endp;	/* -> endpoint */

	    endp = endq->endpoint;
	    if (endp == previousp	/* don't double back! */
		|| endp == currentp	/* don't stay here! */
		)
		continue;

	    /* Note: it would be possible to save some calls
	       to atan2 by being clever about quadrants.  */
	    if (NEAR_EQUAL(endp->xy.y, currentp->xy.y, SMALL_FASTF)
		&& NEAR_EQUAL(endp->xy.x, currentp->xy.x, SMALL_FASTF))
	    {
		to = 0.0;	/* not supposed to happen */
	    } else {
		to = atan2((double)(endp->xy.y - currentp->xy.y),
			   (double)(endp->xy.x - currentp->xy.x)
		    ) * DEG2RAD;
	    }
#ifdef CAD_BOUNDP_DEBUG
	    fprintf(stderr, "to %g", to);
#endif
	    diff = to - from;
	    /* Note: Exact 360 (0) case is not supposed to
	       happen, but this algorithm copes with it.  */
	    while (diff <= 0.0)
		diff += 360.0;

	    if (diff < mindir) {
#ifdef CAD_BOUNDP_DEBUG
		fprintf(stderr, "new min %g", diff);
#endif
		mindir = diff;
		nextp = endp;
	    }
	} while ((endq = endq->nextq) != NULL);

	if (mindir > 361.0)
	    fprintf(stderr, "degenerate input");

	currentp->firstq = NULL;	/* "visited" */
	previousp = currentp;
	currentp = nextp;

	from += mindir + 180.0; /* reverse of saved "to" */
	/* The following is needed only to improve accuracy: */
	while (from > 360.0)
	    from -= 360.0;	/* reduce to range [0, 360) */
    }
}


static point *
PutList(coords *coop) 			/* return -> point in list */
    /* -> coordinates */
{
    point *p;		/* -> list entry */

    p = LookUp(coop);		/* may already be there */
    if (p == NULL) {
	/* not yet in list */
	/* start new point group */
#ifdef CAD_BOUNDP_DEBUG
	fprintf(stderr, "new point group (%g, %g)",
		(double)coop->x, (double)coop->y
	    );
#endif
	p = NewPoint(coop);
    }

    return p;			/* -> point list entry */
}


static point *
LookUp(coords *coop)				/* find point group in list */
    /* -> coordinates */
{
    point *p;		/* -> list members */

    for (p = headp; p != NULL; p = p->linkp)
	if (Near(coop, &p->xy)) {
#ifdef CAD_BOUNDP_DEBUG
	    fprintf(stderr, "found (%g, %g) in list",
		    (double)coop->x, (double)coop->y
		);
#endif
	    return p;	/* found a match */
	}

    return NULL;			/* not yet in list */
}


static point *
NewPoint(coords *coop)			/* add point to list */
    /* -> coordinates */
{
    point *newp;		/* newly allocated point */

    newp = (point *)Alloc((unsigned)sizeof(point));
    if (newp == NULL)
	return NULL;

#ifdef CAD_BOUNDP_DEBUG
    fprintf(stderr, "add point (%g, %g)",
	    (double)coop->x, (double)coop->y
	);
#endif
    newp->linkp = headp;
    newp->firstq = NULL;		/* empty endpoint queue */
    newp->xy = *coop;		/* coordinates */
    return headp = newp;
}


static queue *
Enqueue(point *addp, point *startp) 		/* add to endpoint queue */
    /* -> point being queued */
    /* -> point owning queue */
{
    queue *newq;		/* new queue element */

    newq = (queue *)Alloc((unsigned)sizeof(queue));
    if (newq == NULL)
	return NULL;

#ifdef CAD_BOUNDP_DEBUG
    fprintf(stderr, "enqueue (%g, %g) on (%g, %g)",
	    (double)addp->xy.x, (double)addp->xy.y,
	    (double)startp->xy.x, (double)startp->xy.y
	);
#endif
    newq->nextq = startp->firstq;
    newq->endpoint = addp;
    return startp->firstq = newq;
}


static coords *
Intersect(segment *a, segment *b)			/* determine intersection */
    /* segments being tested */
{
    double det;	/* determinant, 0 if parallel */
    double xaeas, xbebs, yaeas, ybebs;
    /* coordinate differences */

    /* First perform range check, to eliminate most cases. Note that
       segments point left-to-right even after splitting. */

    if (a->sxy.x > 		/* a left */
	b->exy.x + tolerance	/* b right */
	|| a->exy.x < 		/* a right */
	b->sxy.x - tolerance	/* b left */
	|| FMIN(a->sxy.y, a->exy.y) >		/* a bottom */
	FMAX(b->sxy.y, b->exy.y) + tolerance	/* b top */
	|| FMAX(a->sxy.y, a->exy.y) <		/* a top */
	FMIN(b->sxy.y, b->exy.y) - tolerance	/* b bottom */
	) {
#ifdef CAD_BOUNDP_DEBUG
	fprintf(stderr, "  ranges don't intersect\n");
#endif
	return NULL;		/* can't intersect */
    }

    /* Passed quick check, now comes the hard part. */

    xaeas = (double)a->exy.x - (double)a->sxy.x;
    xbebs = (double)b->exy.x - (double)b->sxy.x;
    yaeas = (double)a->exy.y - (double)a->sxy.y;
    ybebs = (double)b->exy.y - (double)b->sxy.y;

    det = xbebs * yaeas - xaeas * ybebs;

    {
	double norm;			/* norm of coefficient matrix */
	double t;			/* test value for norm */

	norm = 0.0;
	if ((t = fabs(xaeas)) > norm)
	    norm = t;
	if ((t = fabs(xbebs)) > norm)
	    norm = t;
	if ((t = fabs(yaeas)) > norm)
	    norm = t;
	if ((t = fabs(ybebs)) > norm)
	    norm = t;

#define EPSILON 1.0e-06 		/* relative `det' size thresh */
	if (fabs(det) <= EPSILON * norm * norm) {
#ifdef CAD_BOUNDP_DEBUG
	    fprintf(stderr, "  parallel: det=%g, norm=%g\n", det, norm);
#endif
	    return NULL;		/* parallels don't intersect */
	}
#undef EPSILON
    }
    {
	/* `p' must be static; Intersect returns a void * to it! */
	static coords p;		/* point of intersection */
	double lambda, mu;	/* segment parameters */
	double onemmu; 	/* 1.0 - mu, for efficiency */
	double xbsas, ybsas;	/* more coord differences */

	xbsas = (double)b->sxy.x - (double)a->sxy.x;
	ybsas = (double)b->sxy.y - (double)a->sxy.y;

	mu = (xbebs * ybsas - xbsas * ybebs) / det;
	onemmu = 1.0 - mu;
	p.x = onemmu * a->sxy.x + mu * a->exy.x;
	p.y = onemmu * a->sxy.y + mu * a->exy.y;
	if ((onemmu < 0.0 || mu < 0.0) && !EndPoint(&p, a)) {
#ifdef CAD_BOUNDP_DEBUG
	    fprintf(stderr, "  intersect off (%g, %g)->(%g, %g): mu=%g\n",
		    (double)a->sxy.x, (double)a->sxy.y,
		    (double)a->exy.x, (double)a->exy.y,
		    mu
		);
#endif
	    return NULL;		/* not in segment *a */
	}

	lambda = (xaeas * ybsas - xbsas * yaeas) / det;
	if ((lambda > 1.0 || lambda < 0.0) && !EndPoint(&p, b)) {
#ifdef CAD_BOUNDP_DEBUG
	    fprintf(stderr, "intersect off (%g, %g)->(%g, %g): lambda=%g",
		    (double)b->sxy.x, (double)b->sxy.y,
		    (double)b->exy.x, (double)b->exy.y,
		    lambda
		);
#endif
	    return NULL;		/* not in segment *b */
	}

#ifdef CAD_BOUNDP_DEBUG
	fprintf(stderr, "  intersection is (%g, %g): mu=%g lambda=%g\n",
		(double)p.x, (double)p.y, mu, lambda
	    );
#endif
	return &p;
    }
}


static int
EndPoint(coords *p, segment *segp)			/* check for segment endpoint */
    /* -> point being tested */
    /* -> segment */
{
#ifdef CAD_BOUNDP_DEBUG
    if (Near(p, &segp->sxy) || Near(p, &segp->exy))
	fprintf(stderr, "  (%g, %g) is endpt of (%g, %g)->(%g, %g)\n",
		(double)p->x, (double)p->y,
		(double)segp->sxy.x, (double)segp->sxy.y,
		(double)segp->exy.x, (double)segp->exy.y
	    );
#endif
    return Near(p, &segp->sxy) || Near(p, &segp->exy);
}


static int
Near(coords *ap, coords *bp)				/* check if within tolerance */
    /* -> points being checked */
{
    double xsq, ysq;	/* dist between coords ^ 2 */

    /* Originally this was an abs value test; this is neater. */

    xsq = ap->x - bp->x;
    xsq *= xsq;
    ysq = ap->y - bp->y;
    ysq *= ysq;

#ifdef CAD_BOUNDP_DEBUG
    if (xsq + ysq <= tolsq)
	fprintf(stderr, "  (%g, %g) is near (%g, %g)\n",
		(double)ap->x, (double)ap->y,
		(double)bp->x, (double)bp->y
	    );
#endif
    return xsq + ysq <= tolsq;
}


static void *
Alloc(unsigned int size)				/* allocate storage from heap */
    /* # bytes required */
{
    void * ptr;	/* -> allocated storage */

    if ((ptr = malloc(size * sizeof(char))) == NULL)
	fprintf(stderr, "out of memory");

    return ptr;			/* (may be NULL) */
}


static void
Toss(void * ptr)				/* return storage to heap */
    /* -> allocated storage */
{
    if (ptr != NULL)
	free(ptr);
}


static int
Input(segment *inp)				/* input stroke record */
    /* -> input segment */
{
    char inbuf[82];	/* record buffer */

    while (bu_fgets(inbuf, (int)sizeof inbuf, stdin) != NULL) {
	/* scan input record */
	int cvt;	/* # fields converted */

#ifdef CAD_BOUNDP_DEBUG
	fprintf(stderr, "input: %s", inbuf);
#endif
	cvt = sscanf(inbuf, " %e %e %e %e",
		     &inp->sxy.x, &inp->sxy.y,
		     &inp->exy.x, &inp->exy.y
	    );

	if (cvt == 0)
	    continue;	/* skip color, comment, etc. */

	if (cvt == 4)
	    return 1;	/* successfully converted */

	fprintf(stderr, "\n  bad input: %s", inbuf);
	bu_exit(5, NULL);		/* return false insufficient */
    }

    return 0;			/* EOF */
}


static void
Output(coords *coop)				/* dump polygon vertex coords */
    /* -> coords to be output */
{
    static coords last;		/* previous *coop */

    if (vflag) {
	if (!initial)
	    printf("%g %g %g %g\n",
		   (double)last.x, (double)last.y,
		   (double)coop->x, (double)coop->y
		);

	last = *coop;		/* save for next start point */
    } else
	printf("%g %g\n",
	       (double)coop->x, (double)coop->y
	    );
#ifdef CAD_BOUNDP_DEBUG
    fprintf(stderr, "output: %g %g", (double)coop->x, (double)coop->y);
#endif
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
