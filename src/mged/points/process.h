/*                       P R O C E S S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @file points/process.h
 *
 * Data structures for the comma-separated value point file parser.
 *
 */

/* private header */

#ifndef MGED_POINTS_PROCESS_H
#define MGED_POINTS_PROCESS_H

#include "common.h"

#include "vmath.h"

#include "./count.h"

typedef struct point_line {
    const char *type;	/* type name (e.g. "POINTS") */
    int code;		/* points_parse.h numeric code */
    int index;		/* acquisition index */
    int count;		/* how many values are set (private use)*/
    point_t val;	/* xyz values */
} point_line_t;

#undef YYSTYPE
#define YYSTYPE point_line_t

#ifndef YY_STACK_USED
#  define YY_STACK_USED 0
#endif
#ifndef YY_STACK_UNUSED
#  define YY_STACK_UNUSED 0
#endif
#ifndef YY_ALWAYS_INTERACTIVE
#  define YY_ALWAYS_INTERACTIVE 0
#endif
#ifndef YY_NEVER_INTERACTIVE
#  define YY_NEVER_INTERACTIVE 0
#endif
#ifndef YY_MAIN
#  define YY_MAIN 0
#endif

/* must come after point_line_t typedef and YYSTYPE define */
#include "./points_parse.h"


#ifndef X
#  define X 0
#endif
#ifndef Y
#  define Y 1
#endif
#ifndef Z
#  define Z 2
#endif

#define INITIALIZE_POINT_LINE_T(x) {\
    (x).type = (const char *)NULL;\
    (x).code = 0;\
    (x).index = 0;\
    (x).count = 0;\
    (x).val[X] = (x).val[Y] = (x).val[Z] = (fastf_t)0.0;\
}

#define COPY_POINT_LINE_T(x, y) {\
    (x).type = (y).type;\
    (x).code = (y).code;\
    (x).index = (y).index;\
    (x).count = (y).count;\
    (x).val[X] = (y).val[X];\
    (x).val[Y] = (y).val[Y];\
    (x).val[Z] = (y).val[Z];\
}


/** process a single point value, adding it to the point */
void process_value(point_line_t *plt, double value);

/** process a single point type, adding it to the point */
void process_type(point_line_t *plt, const char *type, int code);

/** process a single point, passing it along to process_multi_group when collection types change */
void process_point(point_line_t *plt);

/**
 * remove nullified points, condensing the point array by collapsing
 * the invalid points.
 */
int condense_points(point_line_t **plta, int count);

/**
 * remove points marked as "bogus".  this is conventionally done by
 * specifying 5 or more identical points (within a given tolerance) in
 * a given set of points.  if 5 or more points are followed by 5 or
 * more different points, then two previous points are removed and so
 * on.
 */
int delete_points(point_line_t **plta, int count, double tolerance);

/** process a group a points with possible multiples within some tolerance distance */
void process_multi_group(point_line_t **plta, int count, double tolerance);

/** process a single group of points, returns true if processed */
int process_group(point_line_t **plta, int count);

/* primitive point set types */

int create_plate(point_line_t **plta, int count);
int create_arb(point_line_t **plta, int count);
int create_points(point_line_t **plta, int count);
int create_cylinder(point_line_t **plta, int count);
int create_pipe(point_line_t **plta, int count);


#endif  /* MGED_POINTS_PROCESS_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
