/*                  D P L O T _ R E A D E R . H
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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
/** @file dplot_reader.h
 *
 * Brief description
 *
 */

#ifndef DPLOT_PARSER
#define DPLOT_PARSER

#define PERPLEX_ON_ENTER struct dplot_data *data = (struct dplot_data *)yyextra;

#include "bu.h"
#include "dplot_parser.h"
#include "dplot_scanner.h"

typedef struct {
    int n;
} token_t;

struct isocsx {
    struct bu_list l;
    int events;
};

struct ssx {
    struct bu_list l;
    int brep1_surface;
    int brep2_surface;
    int final_curve_events;
    int face1_clipped_curves;
    int face2_clipped_curves;
    int intersecting_brep1_isocurves;
    int intersecting_isocurves;
    struct bu_list isocsx_list; /* struct isocsx list */
    int *isocsx_events;
};

struct split_face {
    int outerloop_curves;
    int innerloop_curves;
};

struct dplot_data {
    token_t token_data;
    int brep1_surface_count;
    int brep2_surface_count;
    int linked_curve_count;
    int ssx_count;
    int split_face_count;
    struct bu_list ssx_list; /* struct ssx list */
    struct ssx *ssx;
    struct bu_list isocsx_list; /* struct ssx list */
    struct split_face *face;
};

/* lemon prototypes */
void *ParseAlloc(void *(*mallocProc)(size_t));
void ParseFree(void *parser, void (*freeProc)(void *));
void Parse(void *yyp, int yymajor, token_t tokenData, struct dplot_data *data);
void ParseTrace(FILE *fp, char *s);

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
