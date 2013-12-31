/*                          A N A L Y Z E . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2013 United States Government as represented by
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
/** @file libged/analyze.c
 *
 * The analyze command.
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>
#include <assert.h>

#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "rtgeom.h"

#include "./ged_private.h"

/**
 * TODO: primitives that still need implementing
 * ehy
 * metaball
 * nmg
 * rhc
 */

/* Conversion factor for Gallons to cubic millimeters */
#define GALLONS_TO_MM3 3785411.784


/* ARB face printout array */
static const int prface[5][6] = {
    {123, 124, 234, 134, -111, -111},		/* ARB4 */
    {1234, 125, 235, 345, 145, -111},		/* ARB5 */
    {1234, 2365, 1564, 512, 634, -111},		/* ARB6 */
    {1234, 567, 145, 2376, 1265, 4375},		/* ARB7 */
    {1234, 5678, 1584, 2376, 1265, 4378},	/* ARB8 */
};


/* edge definition array */
static const int nedge[5][24] = {
    {0, 1, 1, 2, 2, 0, 0, 3, 3, 2, 1, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},   /* ARB4 */
    {0, 1, 1, 2, 2, 3, 0, 3, 0, 4, 1, 4, 2, 4, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1},       /* ARB5 */
    {0, 1, 1, 2, 2, 3, 0, 3, 0, 4, 1, 4, 2, 5, 3, 5, 4, 5, -1, -1, -1, -1, -1, -1},         /* ARB6 */
    {0, 1, 1, 2, 2, 3, 0, 3, 0, 4, 3, 4, 1, 5, 2, 6, 4, 5, 5, 6, 4, 6, -1, -1},             /* ARB7 */
    {0, 1, 1, 2, 2, 3, 0, 3, 0, 4, 4, 5, 1, 5, 5, 6, 6, 7, 4, 7, 3, 7, 2, 6},               /* ARB8 */
};


/* contains information used to analyze a polygonal face */
struct poly_face
{
    char label[5];
    size_t npts;
    point_t *pts;
    plane_t plane_eqn;
    fastf_t area;
};


#define POLY_FACE_INIT_ZERO { { 0, 0, 0, 0, 0 }, 0, NULL, HINIT_ZERO, 0.0 }

#define ADD_PT(face, pt) do { VMOVE((face).pts[(face).npts], (pt)); (face).npts++; } while(0)

/* structures and subroutines for analyze pretty printing */

#define FBUFSIZ 100
#define NFIELDS 9
#define NOT_A_PLANE -1
typedef struct row_field
{
    int nchars;
    char buf[FBUFSIZ];
} field_t;

typedef struct table_row
{
    int nfields;
    field_t fields[NFIELDS];
} row_t;

typedef struct table
{
    int nrows;
    row_t *rows;
} table_t;

void get_dashes(field_t *f, const int ndashes)
{
    int i;
    f->buf[0] = '\0';
    for (i = 0; i < ndashes; ++i) {
	bu_strlcat(f->buf, "-", FBUFSIZ);
    }
    f->nchars = ndashes;
}


void print_volume_table(struct ged *gedp
			, const fastf_t tot_vol
			, const fastf_t tot_area
			, const fastf_t tot_gallons
    )
{

/* table format

   +------------------------------------+
   | Volume       = 7999999999.99999905 |
   | Surface Area =   24000000.00000000 |
   | Gallons      =       2113.37641887 |
   +------------------------------------+

*/
    /* track actual table column widths */
    /* this table has 1 column (plus a name column) */
    int maxwidth[2] = {0, 0};
    field_t dashes;
    char* fnames[3] = {"Volume",
		       "Surface Area",
		       "Gallons"};
    int indent = 4; /* number spaces to indent the table */
    int table_width_chars;
    table_t table;
    int i, nd, field;

    table.nrows = 3;
    table.rows = (row_t *)bu_calloc(3, sizeof(row_t), "print_volume_table: rows");
    for (i = 0; i < table.nrows; ++i) {
	fastf_t val = 0.0;

	/* field 0 */
	field = 0;
	table.rows[i].fields[0].nchars = snprintf(table.rows[i].fields[field].buf, FBUFSIZ, "%s",
						  fnames[i]);
	if (maxwidth[field] < table.rows[i].fields[field].nchars)
	    maxwidth[field] = table.rows[i].fields[field].nchars;

	if (i == 0) {
	    val = tot_vol;
	} else if (i == 1) {
	    val = tot_area;
	} else if (i == 2) {
	    val = tot_gallons;
	}

	/* field 1 */
	field = 1;
	if (val < 0) {
	    table.rows[i].fields[1].nchars = snprintf(table.rows[i].fields[field].buf, FBUFSIZ, "COULD NOT DETERMINE");
	} else {
	    table.rows[i].fields[1].nchars = snprintf(table.rows[i].fields[field].buf, FBUFSIZ, "%10.8f", val);
	}
	if (maxwidth[field] < table.rows[i].fields[field].nchars)
	    maxwidth[field] = table.rows[i].fields[field].nchars;
    }

    /* get total table width */
    table_width_chars  = maxwidth[0] + maxwidth[1];
    table_width_chars += 2 + 2; /* 2 chars at each end of a row */
    table_width_chars += 3; /* ' = ' between the two fields of a row */

    /* newline following previous table */
    bu_vls_printf(gedp->ged_result_str, "\n");

    /* header row 1 */
    nd = table_width_chars - 4;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%-*.*s+-%-*.*s-+\n",
		  indent, indent, " ",
		  nd, nd, dashes.buf);

    /* the three data rows */
    for (i = 0; i < table.nrows; ++i) {
	bu_vls_printf(gedp->ged_result_str, "%-*.*s| %-*.*s = %*.*s |\n",
		      indent, indent, " ",
		      maxwidth[0], maxwidth[0], table.rows[i].fields[0].buf,
		      maxwidth[1], maxwidth[1], table.rows[i].fields[1].buf);
    }

    /* closing table row */
    bu_vls_printf(gedp->ged_result_str, "%-*.*s+-%-*.*s-+\n",
		  indent, indent, " ",
		  nd, nd, dashes.buf);
    bu_free((char *)table.rows, "print_volume_table: rows");
}


void print_edges_table(struct ged *gedp, table_t *table)
{

/* table header

   +--------------------+--------------------+--------------------+--------------------+
   | EDGE          LEN  | EDGE          LEN  | EDGE          LEN  | EDGE          LEN  |
   +--------------------+--------------------+--------------------+--------------------+

*/

    int i;
    int tcol, nd, nrow, nrows;
    int maxwidth[] = {0, 0, 0,
		      0, 0, 0,
		      0, 0};
    int indent = 2;
    field_t dashes;
    char EDGE[] = {"EDGE"};
    int elen    = strlen(EDGE);
    char LEN[]  = {"LENGTH"};
    int llen    = strlen(LEN);
    char buf[FBUFSIZ];

    /* put four edges per row making 8 columns */
    /* this table has 8 columns per row: 2 columns per edge; 4 edges per row */

    /* collect max table column widths */
    tcol = 0;
    for (i = 0; i < table->nrows; ++i) {
	/* field 0 */
	int field = 0;
	if (maxwidth[tcol] < table->rows[i].fields[field].nchars)
	    maxwidth[tcol] = table->rows[i].fields[field].nchars;
	if (maxwidth[tcol] < elen)
	    maxwidth[tcol] = elen;

	/* field 1 */
	field = 1;
	if (maxwidth[tcol+1] < table->rows[i].fields[field].nchars)
	    maxwidth[tcol+1] = table->rows[i].fields[field].nchars;
	if (maxwidth[tcol] < llen)
	    maxwidth[tcol] = llen;

	/* iterate on columns */
	tcol += 2;
	tcol = tcol > 6 ? 0 : tcol;
    }

    /* header row 1 */
    /* print dashes in 4 sets */
    nd = maxwidth[0] + maxwidth[1] + 3; /* 1 space between numbers and one at each end */
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%-*.*s+%-*.*s",
		  indent, indent, " ",
		  nd, nd, dashes.buf);
    nd = maxwidth[2] + maxwidth[3] + 3; /* 1 space between numbers and one at each end */
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "+%-*.*s",
		  nd, nd, dashes.buf);
    nd = maxwidth[4] + maxwidth[5] + 3; /* 1 space between numbers and one at each end */
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "+%-*.*s",
		  nd, nd, dashes.buf);
    nd = maxwidth[6] + maxwidth[7] + 3; /* 1 space between numbers and one at each end */
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "+%-*.*s+\n",
		  nd, nd, dashes.buf);

    /* header row 2 */
    /* print titles in 4 sets */

    /* bu_vls_printf can't handle this at the moment */
    bu_vls_printf(gedp->ged_result_str, "%-*.*s| %-*.*s %*.*s ",
		  indent, indent, " ",
		  maxwidth[0], maxwidth[0], EDGE,
		  maxwidth[1], maxwidth[1], LEN);
    bu_vls_printf(gedp->ged_result_str, "| %-*.*s %*.*s ",
		  maxwidth[2], maxwidth[2], EDGE,
		  maxwidth[3], maxwidth[3], LEN);
    bu_vls_printf(gedp->ged_result_str, "| %-*.*s %*.*s ",
		  maxwidth[4], maxwidth[4], EDGE,
		  maxwidth[5], maxwidth[5], LEN);
    bu_vls_printf(gedp->ged_result_str, "| %-*.*s %*.*s |\n",
		  maxwidth[6], maxwidth[6], EDGE,
		  maxwidth[7], maxwidth[7], LEN);

    /* header row 3 */
    /* print dashes in 4 sets */
    nd = maxwidth[0] + maxwidth[1] + 3; /* 1 space between numbers and one at each end */
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%-*.*s+%-*.*s",
		  indent, indent, " ",
		  nd, nd, dashes.buf);
    nd = maxwidth[2] + maxwidth[3] + 3; /* 1 space between numbers and one at each end */
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "+%-*.*s",
		  nd, nd, dashes.buf);
    nd = maxwidth[4] + maxwidth[5] + 3; /* 1 space between numbers and one at each end */
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "+%-*.*s",
		  nd, nd, dashes.buf);
    nd = maxwidth[6] + maxwidth[7] + 3; /* 1 space between numbers and one at each end */
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "+%-*.*s+\n",
		  nd, nd, dashes.buf);

    /* print the data lines */
    /* collect max table column widths */
    tcol = 0;
    nrow = 0;
    for (i = 0; i < table->nrows; ++i) {
	int field;

	if (tcol == 0) {
	    /* need to start a row */
	    snprintf(buf, FBUFSIZ, "%-*.*s|",
		     indent, indent, " ");
	    bu_vls_printf(gedp->ged_result_str, "%s", buf);
	}

	/* data in sets of two */
	/* field 0 */
	field = 0;
	/* FIXME: using snprintf because bu_vls_printf is broken for complex formats */
	snprintf(buf, FBUFSIZ, " %-*.*s",
		 maxwidth[tcol], maxwidth[tcol], table->rows[i].fields[field].buf);
	bu_vls_printf(gedp->ged_result_str, "%s", buf);

	/* field 1 */
	field = 1;
	/* FIXME: using snprintf because bu_vls_printf is broken for complex formats */
	snprintf(buf, FBUFSIZ, " %-*.*s |",
		 maxwidth[tcol+1], maxwidth[tcol+1], table->rows[i].fields[field].buf);
	bu_vls_printf(gedp->ged_result_str, "%s", buf);

	/* iterate on columns */
	tcol += 2;

	if (tcol > 6) {
	    /* time for a newline to end the row */
	    bu_vls_printf(gedp->ged_result_str, "\n");
	    tcol = 0;
	    ++nrow;
	}
    }

    /* we may have a row to finish */
    nrows = table->nrows % 4;
    if (nrows) {
	assert(tcol < 8);

	/* write blanks */
	while (tcol < 7) {

	    /* data in sets of two */
	    /* this is field 0 */
	    /* FIXME: using snprintf because bu_vls_printf is broken for complex formats */
	    snprintf(buf, FBUFSIZ, " %-*.*s",
		     maxwidth[tcol], maxwidth[tcol], " ");
	    bu_vls_printf(gedp->ged_result_str, "%s", buf);

	    /* this is field 1 */
	    /* FIXME: using snprintf because bu_vls_printf is broken for complex formats */
	    snprintf(buf, FBUFSIZ, " %-*.*s |",
		     maxwidth[tcol+1], maxwidth[tcol+1], " ");
	    bu_vls_printf(gedp->ged_result_str, "%s", buf);

	    /* iterate on columns */
	    tcol += 2;

	    if (tcol > 6) {
		/* time for a newline to end the row */
		bu_vls_printf(gedp->ged_result_str, "\n");
	    }
	}
    }

    /* close the table */
    /* print dashes in 4 sets */
    nd = maxwidth[0] + maxwidth[1] + 3; /* 1 space between numbers and one at each end */
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%-*.*s+%-*.*s",
		  indent, indent, " ",
		  nd, nd, dashes.buf);
    nd = maxwidth[2] + maxwidth[3] + 3; /* 1 space between numbers and one at each end */
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "+%-*.*s",
		  nd, nd, dashes.buf);
    nd = maxwidth[4] + maxwidth[5] + 3; /* 1 space between numbers and one at each end */
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "+%-*.*s",
		  nd, nd, dashes.buf);
    nd = maxwidth[6] + maxwidth[7] + 3; /* 1 space between numbers and one at each end */
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "+%-*.*s+\n",
		  nd, nd, dashes.buf);
}


void print_faces_table(struct ged *gedp, table_t *table)
{

/* table header

   +------+-----------------------------+--------------------------------------------------+-----------------+
   | FACE |      ROT           FB       |                  PLANE EQUATION                  |   SURFACE AREA  |
   +------+-----------------------------+--------------------------------------------------+-----------------+

*/

    /* track actual table column widths */
    /* this table has 8 columns */
    int maxwidth[8] = {0, 0, 0,
		       0, 0, 0,
		       0, 0};
    int i, j;
    int c0, h1a, h1b, h1c;
    int h2a, h2b, h2c;
    int c2, c2a, c2b, c2c;
    int f7, f7a, f7b, f7c;
    int nd, tnd;
    field_t dashes;
    char ROT[] = {"ROT"};
    char FB[]  = {"FB"};
    char PA[]  = {"PLANE EQUATION"};
    char SA[]  = {"SURFACE AREA"};

    /* get max fields widths */
    for (i = 0; i < table->nrows; ++i) {
	for (j = 0; j < table->rows[i].nfields; ++j) {
	    if (table->rows[i].fields[j].nchars > maxwidth[j])
		maxwidth[j] = table->rows[i].fields[j].nchars;
	}
    }

    /* blank line following previous table */
    bu_vls_printf(gedp->ged_result_str, "\n");

    /* get max width of header columns (not counting single space on either side) */
    c0 = maxwidth[0] > 4 ? maxwidth[0] : 4;

    /* print "ROT" in center of field 1 space */
    h1b = strlen(ROT);
    h1a = (maxwidth[1] - h1b)/2;
    h1c = (maxwidth[1] - h1b - h1a);

    /* print "FB" in center of field 2 space */
    h2b = strlen(FB);
    h2a = (maxwidth[2] - h2b)/2;
    h2c = (maxwidth[2] - h2b - h2a);

    /* get width of subcolumns of header column 2 */
    /* print "PLANE EQUATION" in center of columns 2 space */
    c2 = maxwidth[3] + maxwidth[4] + maxwidth[5] + maxwidth[6] + 3; /* 3 spaces between fields */
    c2b = strlen(PA);
    c2a = (c2 - c2b)/2;
    c2c = (c2 - c2b - c2a);

    /* print "SURFACE AREA" in center of field 7 space */
    f7b = strlen(SA);
    f7  = maxwidth[7] > f7b ? maxwidth[7] : f7b;
    f7a = (f7 - f7b)/2;
    f7c = (f7 - f7b - f7a);

    /* print the pieces */

    /* header row 1 */
    bu_vls_printf(gedp->ged_result_str, "+-");
    nd = c0; tnd = nd;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%-*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+-");
    nd = h1a + h1b + h1c + 1 + h2a + h2b + h2c; tnd += nd + 3;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+-");
    nd = c2a + c2b + c2c; tnd += nd + 3;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+-");
    nd = f7a + f7b + f7c; tnd += nd + 3;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+\n");

    /* header row 2 */
    bu_vls_printf(gedp->ged_result_str, "| ");

    bu_vls_printf(gedp->ged_result_str, "%-*.*s", c0, c0, "FACE");

    bu_vls_printf(gedp->ged_result_str, " | ");


    bu_vls_printf(gedp->ged_result_str, "%*.*s", h1a, h1a, " ");

    bu_vls_printf(gedp->ged_result_str, "%*.*s", h1b, h1b, ROT);

    bu_vls_printf(gedp->ged_result_str, "%*.*s", h1c+h2a, h1c+h2a, " ");

    bu_vls_printf(gedp->ged_result_str, "%*.*s", h2b, h2b, FB);

    bu_vls_printf(gedp->ged_result_str, "%*.*s ", h2c, h2c, " ");


    bu_vls_printf(gedp->ged_result_str, " | ");


    bu_vls_printf(gedp->ged_result_str, "%*.*s", c2a, c2a, " ");

    bu_vls_printf(gedp->ged_result_str, "%*.*s", c2b, c2b, PA);

    bu_vls_printf(gedp->ged_result_str, "%*.*s", c2c, c2c, " ");


    bu_vls_printf(gedp->ged_result_str, " | ");

    bu_vls_printf(gedp->ged_result_str, "%*.*s", f7a, f7a, " ");

    bu_vls_printf(gedp->ged_result_str, "%*.*s", f7b, f7b, SA);

    bu_vls_printf(gedp->ged_result_str, "%*.*s", f7c, f7c, " ");

    bu_vls_printf(gedp->ged_result_str, " |\n");

    /* header row 3 */
    bu_vls_printf(gedp->ged_result_str, "+-");
    nd = c0; tnd = nd;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%-*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+-");
    nd = h1a + h1b + h1c + 1 + h2a + h2b + h2c; tnd += nd + 3;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+-");
    nd = c2a + c2b + c2c; tnd += nd + 3;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+-");
    nd = f7a + f7b + f7c; tnd += nd + 3;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+\n");

    /* output table data rows */
    for (i = 0; i < table->nrows; ++i) {
	/* may not have a row with data */
	if (table->rows[i].nfields == 0)
	    continue;
	if (table->rows[i].nfields == NOT_A_PLANE)
	    bu_vls_printf(gedp->ged_result_str, "***NOT A PLANE ***");

	bu_vls_printf(gedp->ged_result_str, "|");
	for (j = 0; j < table->rows[i].nfields; ++j) {
	    assert(table->rows[i].fields[j].buf);
	    bu_vls_printf(gedp->ged_result_str, " %*.*s",
			  maxwidth[j], maxwidth[j],
			  table->rows[i].fields[j].buf);
	    /* do we need a separator? */
	    if (j == 0 || j == 2 || j == 6 || j == 7)
		bu_vls_printf(gedp->ged_result_str, " |");
	}
	/* close the row */
	bu_vls_printf(gedp->ged_result_str, "\n");
    }

    /* close the table with the ender row */
    bu_vls_printf(gedp->ged_result_str, "+-");
    nd = c0; tnd = nd;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%-*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+-");
    nd = h1a + h1b + h1c + 1 + h2a + h2b + h2c; tnd += nd + 3;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+-");
    nd = c2a + c2b + c2c; tnd += nd + 3;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+-");
    nd = f7a + f7b + f7c; tnd += nd + 3;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+\n");
}


/**
 * A N A L Y Z E _ G E N E R A L
 *
 * general analyze function for primitives that can be analyzed using volume
 * and surface area functions from the rt_functab.
 * Currently used for:
 * - ell
 * - tor
 * - tgc
 * - rpc
 * - eto
 * - epa
 * - part
 */
HIDDEN void
analyze_general(struct ged *gedp, const struct rt_db_internal *ip)
{
    fastf_t vol, area;
    point_t centroid;

    vol = area = -1.0;

    if (OBJ[ip->idb_minor_type].ft_volume) {
	OBJ[ip->idb_minor_type].ft_volume(&vol, ip);
    }
    if (OBJ[ip->idb_minor_type].ft_surf_area) {
	OBJ[ip->idb_minor_type].ft_surf_area(&area, ip);
    }

    if (OBJ[ip->idb_minor_type].ft_centroid) {
        OBJ[ip->idb_minor_type].ft_centroid(&centroid, ip);
	bu_vls_printf(gedp->ged_result_str, "\n    Centroid: (%g, %g, %g)\n",
		      centroid[X] * gedp->ged_wdbp->dbip->dbi_base2local,
		      centroid[Y] * gedp->ged_wdbp->dbip->dbi_base2local,
		      centroid[Z] * gedp->ged_wdbp->dbip->dbi_base2local);
    }

    print_volume_table(gedp,
		       vol
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local,
		       area
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local,
		       vol/GALLONS_TO_MM3
	);
}


/**
 * F I N D A N G
 *
 * finds direction cosines and rotation, fallback angles of a unit vector
 * angles = pointer to 5 fastf_t's to store angles
 * unitv = pointer to the unit vector (previously computed)
 */
HIDDEN void
findang(fastf_t *angles, fastf_t *unitv)
{
    int i;
    fastf_t f;

    /* convert direction cosines into axis angles */
    for (i = X; i <= Z; i++) {
	if (unitv[i] <= -1.0)
	    angles[i] = -90.0;
	else if (unitv[i] >= 1.0)
	    angles[i] = 90.0;
	else
	    angles[i] = acos(unitv[i]) * bn_radtodeg;
    }

    /* fallback angle */
    if (unitv[Z] <= -1.0)
	unitv[Z] = -1.0;
    else if (unitv[Z] >= 1.0)
	unitv[Z] = 1.0;
    angles[4] = asin(unitv[Z]);

    /* rotation angle */
    /* For the tolerance below, on an SGI 4D/70, cos(asin(1.0)) != 0.0
     * with an epsilon of +/- 1.0e-17, so the tolerance below was
     * substituted for the original +/- 1.0e-20.
     */
    if ((f = cos(angles[4])) > 1.0e-16 || f < -1.0e-16) {
	f = unitv[X]/f;
	if (f <= -1.0)
	    angles[3] = 180.0;
	else if (f >= 1.0)
	    angles[3] = 0.0;
	else
	    angles[3] = bn_radtodeg * acos(f);
    } else
	angles[3] = 0.0;

    if (unitv[Y] < 0)
	angles[3] = 360.0 - angles[3];

    angles[4] *= bn_radtodeg;
}


/* qsort helper function, used to sort points into
 * counter-clockwise order */
HIDDEN int
ccw(const void *x, const void *y, void *cmp)
{
    vect_t tmp;
    VCROSS(tmp, ((fastf_t *)x), ((fastf_t *)y));
    return VDOT(*((point_t *)cmp), tmp);
}


/**
 * A N A L Y Z E _ P O L Y _ F A C E
 *
 * general analyze function for polygonal faces.
 * Currently used for:
 * - arb8
 * - arbn
 * - bot
 * - ars
 *
 * returns:
 * - area in face->area
 * - print_faces_table() information in row
 * - sorts vertices in face->pts into ccw order
 */
HIDDEN void
analyze_poly_face(struct ged *gedp, struct poly_face *face, row_t *row)
{
    fastf_t angles[5];

    findang(angles, face->plane_eqn);

    /* sort points */
    bu_sort(face->pts, face->npts, sizeof(point_t), ccw, &face->plane_eqn);
    bn_polygon_area(&face->area, face->npts, (const point_t *)face->pts);

    /* store face information for pretty printing */
    row->nfields = 8;
    row->fields[0].nchars = sprintf(row->fields[0].buf, "%4s", face->label);
    row->fields[1].nchars = sprintf(row->fields[1].buf, "%10.8f", angles[3]);
    row->fields[2].nchars = sprintf(row->fields[2].buf, "%10.8f", angles[4]);
    row->fields[3].nchars = sprintf(row->fields[3].buf, "%10.8f", face->plane_eqn[X]);
    row->fields[4].nchars = sprintf(row->fields[4].buf, "%10.8f", face->plane_eqn[Y]);
    row->fields[5].nchars = sprintf(row->fields[5].buf, "%10.8f", face->plane_eqn[Z]);
    row->fields[6].nchars = sprintf(row->fields[6].buf, "%10.8f",
				    face->plane_eqn[W]*gedp->ged_wdbp->dbip->dbi_base2local);
    row->fields[7].nchars = sprintf(row->fields[7].buf, "%10.8f",
				    face->area*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local);
}


/**
 * A N A L Y Z E _ E D G E
 */
HIDDEN void
analyze_edge(struct ged *gedp, const int edge, const struct rt_arb_internal *arb,
	     const int type, row_t *row)
{
    int a = nedge[type][edge*2];
    int b = nedge[type][edge*2+1];

    if (b == -1) {
	row->nfields = 0;
	return;
    }

    row->nfields = 2;
    row->fields[0].nchars = sprintf(row->fields[0].buf, "%d%d", a + 1, b + 1);
    row->fields[1].nchars = sprintf(row->fields[1].buf, "%10.8f",
				    DIST_PT_PT(arb->pt[a], arb->pt[b])*gedp->ged_wdbp->dbip->dbi_base2local);
}


/**
 * A N A L Y Z E _ A R B 8
 */
HIDDEN void
analyze_arb8(struct ged *gedp, const struct rt_db_internal *ip)
{
    int i, type;
    int cgtype;     /* COMGEOM arb type: # of vertices */
    table_t table;  /* holds table data from child functions */
    fastf_t tot_vol = 0.0, tot_area = 0.0;
    point_t center_pt = VINIT_ZERO;
    struct poly_face face = POLY_FACE_INIT_ZERO;
    struct rt_arb_internal earb;
    struct rt_arb_internal *arb = (struct rt_arb_internal *)ip->idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    /* find the specific arb type, in GIFT order. */
    if ((cgtype = rt_arb_std_type(ip, &gedp->ged_wdbp->wdb_tol)) == 0) {
	bu_vls_printf(gedp->ged_result_str, "analyze_arb: bad ARB\n");
	return;
    }

    type = cgtype - 4;

    /* to get formatting correct, we need to collect the actual string
     * lengths for each field BEFORE we start printing a table (fields
     * are allowed to overflow the stated printf field width) */

    /* TABLE 1 =========================================== */
    /* analyze each face, use center point of arb for reference */
    rt_arb_centroid(&center_pt, ip);

    /* allocate pts array, maximum 4 verts per arb8 face */
    face.pts = (point_t *)bu_calloc(4, sizeof(point_t), "analyze_arb8: pts");
    /* allocate table rows, 12 rows needed for arb8 edges */
    table.rows = (row_t *)bu_calloc(12, sizeof(row_t), "analyze_arb8: rows");

    table.nrows = 0;
    for (face.npts = 0, i = 0; i < 6; face.npts = 0, i++) {
	int a, b, c, d; /* 4 indices to face vertices */

	a = rt_arb_faces[type][i*4+0];
	b = rt_arb_faces[type][i*4+1];
	c = rt_arb_faces[type][i*4+2];
	d = rt_arb_faces[type][i*4+3];

	if (a == -1) {
	    table.rows[i].nfields = 0;
	    continue;
	}

	/* find plane eqn for this face */
	if (bn_mk_plane_3pts(face.plane_eqn, arb->pt[a], arb->pt[b], arb->pt[c], &gedp->ged_wdbp->wdb_tol) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "| %d%d%d%d |         ***NOT A PLANE***                                          |\n",
			  a+1, b+1, c+1, d+1);
	    /* this row has 1 special fields */
	    table.rows[i].nfields = NOT_A_PLANE;
	    continue;
	}

	ADD_PT(face, arb->pt[a]);
	ADD_PT(face, arb->pt[b]);
	ADD_PT(face, arb->pt[c]);
	ADD_PT(face, arb->pt[d]);

	/* The plane equations returned by bn_mk_plane_3pts above do
	 * not necessarily point outward. Use the reference center
	 * point for the arb and reverse direction for any errant planes.
	 * This corrects the output rotation, fallback angles so that
	 * they always give the outward pointing normal vector. */
	if (DIST_PT_PLANE(center_pt, face.plane_eqn) > 0.0) {
	    HREVERSE(face.plane_eqn, face.plane_eqn);
	}

	snprintf(face.label, sizeof(face.label), "%d", prface[type][i]);

	analyze_poly_face(gedp, &face, &(table.rows[i]));
	tot_area += face.area;
	table.nrows++;
    }

    /* and print it */
    print_faces_table(gedp, &table);

    /* TABLE 2 =========================================== */
    /* analyze each edge */

    /* blank line following previous table */
    bu_vls_printf(gedp->ged_result_str, "\n");

    /* set up the records for arb4's and arb6's */
    earb = *arb; /* struct copy */
    if (cgtype == 4) {
	VMOVE(earb.pt[3], earb.pt[4]);
    } else if (cgtype == 6) {
	VMOVE(earb.pt[5], earb.pt[6]);
    }

    table.nrows = 0;
    for (i = 0; i < 12; i++) {
	analyze_edge(gedp, i, &earb, type, &(table.rows[i]));
	if (nedge[type][i*2] == -1) {
	    break;
	}
	table.nrows += 1;
    }

    print_edges_table(gedp, &table);

    /* TABLE 3 =========================================== */
    /* find the volume - break arb8 into 6 arb4s */

    if (OBJ[ID_ARB8].ft_volume)
	OBJ[ID_ARB8].ft_volume(&tot_vol, ip);

    print_volume_table(gedp,
		       tot_vol
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local,
		       tot_area
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local,
		       tot_vol/GALLONS_TO_MM3
	);

    bu_free((char *)face.pts, "analyze_arb8: pts");
    bu_free((char *)table.rows, "analyze_arb8: rows");
}


/**
 * A N A L Y Z E _ A R B N
 */
HIDDEN void
analyze_arbn(struct ged *gedp, const struct rt_db_internal *ip)
{
    size_t i, j, k, l;
    fastf_t tot_vol = 0.0, tot_area = 0.0;
    table_t table;
    struct poly_face *faces;
    struct bu_vls tmpstr = BU_VLS_INIT_ZERO;
    struct rt_arbn_internal *aip = (struct rt_arbn_internal *)ip->idb_ptr;

    /* allocate array of face structs */
    faces = (struct poly_face *)bu_calloc(aip->neqn, sizeof(struct poly_face), "analyze_arbn: faces");
    for (i = 0; i < aip->neqn; i++) {
	HMOVE(faces[i].plane_eqn, aip->eqn[i]);
	VUNITIZE(faces[i].plane_eqn);
	/* allocate array of pt structs, max number of verts per faces = (# of faces) - 1 */
	faces[i].pts = (point_t *)bu_calloc(aip->neqn - 1, sizeof(point_t), "analyze_arbn: pts");
    }
    /* allocate table rows, 1 row per plane eqn */
    table.rows = (row_t *)bu_calloc(aip->neqn, sizeof(row_t), "analyze_arbn: rows");
    table.nrows = aip->neqn;

    /* find all vertices */
    for (i = 0; i < aip->neqn - 2; i++) {
	for (j = i + 1; j < aip->neqn - 1; j++) {
	    for (k = j + 1; k < aip->neqn; k++) {
		point_t pt;
		int keep_point = 1;
		if (bn_mkpoint_3planes(pt, aip->eqn[i], aip->eqn[j], aip->eqn[k]) < 0) {
		    continue;
		}
		/* discard pt if it is outside the arbn */
		for (l = 0; l < aip->neqn; l++) {
		    if (l == i || l == j || l == k) {
			continue;
		    }
		    if (DIST_PT_PLANE(pt, aip->eqn[l]) > gedp->ged_wdbp->wdb_tol.dist) {
			keep_point = 0;
			break;
		    }
		}
		/* found a good point, add it to each of the intersecting faces */
		if (keep_point) {
		    ADD_PT(faces[i], pt);
		    ADD_PT(faces[j], pt);
		    ADD_PT(faces[k], pt);
		}
	    }
	}
    }

    for (i = 0; i < aip->neqn; i++) {
	vect_t tmp;
	bu_vls_sprintf(&tmpstr, "%4zu", i);
	snprintf(faces[i].label, sizeof(faces[i].label), "%s", bu_vls_addr(&tmpstr));

	/* calculate surface area */
	analyze_poly_face(gedp, &faces[i], &table.rows[i]);
	tot_area += faces[i].area;

	/* calculate volume */
	VSCALE(tmp, faces[i].plane_eqn, faces[i].area);
	tot_vol += VDOT(faces[i].pts[0], tmp);
    }
    tot_vol /= 3.0;

    print_faces_table(gedp, &table);
    print_volume_table(gedp,
		       tot_vol
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local,
		       tot_area
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local,
		       tot_vol/GALLONS_TO_MM3
	);

    for (i = 0; i < aip->neqn; i++) {
	bu_free((char *)faces[i].pts, "analyze_arbn: pts");
    }
    bu_free((char *)faces, "analyze_arbn: faces");
    bu_free((char *)table.rows, "analyze_arbn: rows");
    bu_vls_free(&tmpstr);
}


#define BOT_PT(idx) (&bot->vertices[(idx) * ELEMENTS_PER_POINT])

/**
 * A N A L Y Z E _ B O T
 */
HIDDEN void
analyze_bot(struct ged *gedp, const struct rt_db_internal *ip)
{
    size_t i;
    fastf_t tot_area = 0.0, tot_vol = 0.0;
    table_t table;
    struct poly_face face = POLY_FACE_INIT_ZERO;
    struct rt_bot_internal *bot = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    /* allocate pts array, 3 vertices per bot face */
    face.pts = (point_t *)bu_calloc(3, sizeof(point_t), "analyze_bot: pts");
    /* allocate table rows, 1 row per bot face */
    table.rows = (row_t *)bu_calloc(bot->num_faces, sizeof(row_t), "analyze_bot: rows");
    table.nrows = bot->num_faces;

    for (face.npts = 0, i = 0; i < bot->num_faces; face.npts = 0, i++) {
	int a, b, c;
	vect_t tmp;

	/* find indices of the 3 vertices that make up this face */
	a = bot->faces[i * ELEMENTS_PER_POINT + 0];
	b = bot->faces[i * ELEMENTS_PER_POINT + 1];
	c = bot->faces[i * ELEMENTS_PER_POINT + 2];

	/* find normal, needed to calculate volume later */
	if (bot->bot_flags == RT_BOT_HAS_SURFACE_NORMALS && bot->normals) {
	    /* bot->normals array already exists, use those instead */
	    VMOVE(face.plane_eqn, &bot->normals[i * ELEMENTS_PER_VECT]);
	} else if (UNLIKELY(bn_mk_plane_3pts(face.plane_eqn, BOT_PT(a), BOT_PT(b), BOT_PT(c), &gedp->ged_wdbp->wdb_tol) < 0)) {
	    bu_vls_printf(gedp->ged_result_str,
			  "analyze_bot: bad BOT, points (%.3f, %.3f, %.3f), (%.3f, %.3f, %.3f), (%.3f, %.3f, %.3f) do not form a plane\n",
			  V3ARGS(BOT_PT(a)), V3ARGS(BOT_PT(b)), V3ARGS(BOT_PT(c)));
	    continue;
	}

	ADD_PT(face, BOT_PT(a));
	ADD_PT(face, BOT_PT(b));
	ADD_PT(face, BOT_PT(c));

	snprintf(face.label, sizeof(face.label), "%d%d%d", a, b, c);

	/* surface area */
	analyze_poly_face(gedp, &face, &table.rows[i]);
	tot_area += face.area;

	/* volume */
	VSCALE(tmp, face.plane_eqn, face.area);
	tot_vol += fabs(VDOT(face.pts[0], tmp));
    }
    tot_vol /= 3.0;

    print_faces_table(gedp, &table);
    print_volume_table(gedp,
		       tot_vol
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local,
		       tot_area
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local,
		       tot_vol/GALLONS_TO_MM3
	);

    bu_free((char *)face.pts, "analyze_bot: pts");
    bu_free((char *)table.rows, "analyze_bot: rows");
}


#define ARS_PT(ii, jj) (&arip->curves[i+(ii)][(j+(jj))*ELEMENTS_PER_VECT])

/**
 * A N A L Y Z E _ A R S
 */
HIDDEN void
analyze_ars(struct ged *gedp, const struct rt_db_internal *ip)
{
    size_t i, j, k;
    size_t nfaces = 0;
    fastf_t tot_area = 0.0, tot_vol = 0.0;
    table_t table;
    plane_t old_plane = HINIT_ZERO;
    struct bu_vls tmpstr = BU_VLS_INIT_ZERO;
    struct poly_face face = POLY_FACE_INIT_ZERO;
    struct rt_ars_internal *arip = (struct rt_ars_internal *)ip->idb_ptr;
    RT_ARS_CK_MAGIC(arip);

    /* allocate pts array, max 3 pts per triangular face */
    face.pts = (point_t *)bu_calloc(3, sizeof(point_t), "analyze_ars: pts");
    /* allocate table rows, probably overestimating the number of rows needed */
    table.rows = (row_t *)bu_calloc((arip->ncurves - 1) * 2 * arip->pts_per_curve, sizeof(row_t), "analyze_ars: rows");

    k = arip->pts_per_curve - 2;
    for (i = 0; i < arip->ncurves - 1; i++) {
	int double_ended = k != 1 && VEQUAL(&arip->curves[i][ELEMENTS_PER_VECT], &arip->curves[i][k * ELEMENTS_PER_VECT]);

	for (j = 0; j < arip->pts_per_curve; j++) {
	    vect_t tmp;

	    if (double_ended && i != 0 && (j == 0 || j == k || j == arip->pts_per_curve - 1)) continue;

	    /* first triangular face, make sure its not a duplicate */
	    if (bn_mk_plane_3pts(face.plane_eqn, ARS_PT(0, 0), ARS_PT(1, 1), ARS_PT(0, 1), &gedp->ged_wdbp->wdb_tol) == 0
		&& !HEQUAL(old_plane, face.plane_eqn)) {
		HMOVE(old_plane, face.plane_eqn);
		ADD_PT(face, ARS_PT(0, 1));
		ADD_PT(face, ARS_PT(0, 0));
		ADD_PT(face, ARS_PT(1, 1));

		bu_vls_sprintf(&tmpstr, "%zu%zu", i, j);
		snprintf(face.label, sizeof(face.label), "%s", bu_vls_addr(&tmpstr));

		/* surface area */
		analyze_poly_face(gedp, &face, &(table.rows[nfaces]));
		tot_area += face.area;

		/* volume */
		VSCALE(tmp, face.plane_eqn, face.area);
		tot_vol += fabs(VDOT(face.pts[0], tmp));

		face.npts = 0;
		nfaces++;
	    }

	    /* second triangular face, make sure its not a duplicate */
	    if (bn_mk_plane_3pts(face.plane_eqn, ARS_PT(1, 0), ARS_PT(1, 1), ARS_PT(0, 0), &gedp->ged_wdbp->wdb_tol) == 0
		&& !HEQUAL(old_plane, face.plane_eqn)) {
		HMOVE(old_plane, face.plane_eqn);
		ADD_PT(face, ARS_PT(1, 0));
		ADD_PT(face, ARS_PT(0, 0));
		ADD_PT(face, ARS_PT(1, 1));

		bu_vls_sprintf(&tmpstr, "%zu%zu", i, j);
		snprintf(face.label, sizeof(face.label), "%s", bu_vls_addr(&tmpstr));

		analyze_poly_face(gedp, &face, &table.rows[nfaces]);
		tot_area += face.area;

		VSCALE(tmp, face.plane_eqn, face.area);
		tot_vol += fabs(VDOT(face.pts[0], tmp));

		face.npts = 0;
		nfaces++;
	    }
	}
    }
    tot_vol /= 3.0;
    table.nrows = nfaces;

    print_faces_table(gedp, &table);
    print_volume_table(gedp,
		       tot_vol
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local,
		       tot_area
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local,
		       tot_vol/GALLONS_TO_MM3
	);

    bu_free((char *)face.pts, "analyze_ars: pts");
    bu_free((char *)table.rows, "analyze_ars: rows");
    bu_vls_free(&tmpstr);
}


#define PROLATE 1
#define OBLATE 2

/**
 * A N A L Y Z E _ S U P E R E L L
 */
HIDDEN void
analyze_superell(struct ged *gedp, const struct rt_db_internal *ip)
{
    struct rt_superell_internal *superell = (struct rt_superell_internal *)ip->idb_ptr;
    fastf_t ma, mb, mc;
    fastf_t ecc, major_mag, minor_mag;
    fastf_t vol, sur_area;
    int type;

    RT_SUPERELL_CK_MAGIC(superell);

    ma = MAGNITUDE(superell->a);
    mb = MAGNITUDE(superell->b);
    mc = MAGNITUDE(superell->c);

    type = 0;

    vol = 4.0 * M_PI * ma * mb * mc / 3.0;

    if (fabs(ma-mb) < .00001 && fabs(mb-mc) < .00001) {
	/* have a sphere */
	sur_area = 4.0 * M_PI * ma * ma;
	goto print_results;
    }
    if (fabs(ma-mb) < .00001) {
	/* A == B */
	if (mc > ma) {
	    /* oblate spheroid */
	    type = OBLATE;
	    major_mag = mc;
	    minor_mag = ma;
	} else {
	    /* prolate spheroid */
	    type = PROLATE;
	    major_mag = ma;
	    minor_mag = mc;
	}
    } else
	if (fabs(ma-mc) < .00001) {
	    /* A == C */
	    if (mb > ma) {
		/* oblate spheroid */
		type = OBLATE;
		major_mag = mb;
		minor_mag = ma;
	    } else {
		/* prolate spheroid */
		type = PROLATE;
		major_mag = ma;
		minor_mag = mb;
	    }
	} else
	    if (fabs(mb-mc) < .00001) {
		/* B == C */
		if (ma > mb) {
		    /* oblate spheroid */
		    type = OBLATE;
		    major_mag = ma;
		    minor_mag = mb;
		} else {
		    /* prolate spheroid */
		    type = PROLATE;
		    major_mag = mb;
		    minor_mag = ma;
		}
	    } else {
		bu_vls_printf(gedp->ged_result_str, "   Cannot find surface area\n");
		return;
	    }
    ecc = sqrt(major_mag*major_mag - minor_mag*minor_mag) / major_mag;
    if (type == PROLATE) {
	sur_area = 2.0 * M_PI * minor_mag * minor_mag +
	    (2.0 * M_PI * (major_mag*minor_mag/ecc) * asin(ecc));
    } else {
	/* type == OBLATE */
	sur_area = 2.0 * M_PI * major_mag * major_mag +
	    (M_PI * (minor_mag*minor_mag/ecc) * log((1.0+ecc)/(1.0-ecc)));
    }

print_results:
    print_volume_table(gedp,
		       vol
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local,
		       sur_area
		       * gedp->ged_wdbp->dbip->dbi_base2local
		       * gedp->ged_wdbp->dbip->dbi_base2local,
		       vol/GALLONS_TO_MM3
	);
}


/**
 * A N A L Y Z E _ S K E T C H
 */
HIDDEN void
analyze_sketch(struct ged *gedp, const struct rt_db_internal *ip)
{
    fastf_t area = -1;

    if (OBJ[ID_SKETCH].ft_surf_area)
	OBJ[ID_SKETCH].ft_surf_area(&area, ip);

    if (area > 0.0) {
	bu_vls_printf(gedp->ged_result_str, "\nTotal Area: %10.8f",
		      area
		      * gedp->ged_wdbp->dbip->dbi_local2base
		      * gedp->ged_wdbp->dbip->dbi_local2base
	    );
    }
}


/**
 * Analyze command - prints loads of info about a solid
 * Format:	analyze [name]
 * if 'name' is missing use solid being edited
 */

/* Analyze solid in internal form */
HIDDEN void
analyze_do(struct ged *gedp, const struct rt_db_internal *ip)
{
    /* XXX Could give solid name, and current units, here */

    switch (ip->idb_type) {

	case ID_ARB8:
	    analyze_arb8(gedp, ip);
	    break;

	case ID_BOT:
	    analyze_bot(gedp, ip);
	    break;

	case ID_ARBN:
	    analyze_arbn(gedp, ip);
	    break;

	case ID_ARS:
	    analyze_ars(gedp, ip);
	    break;

	case ID_TGC:
	    analyze_general(gedp, ip);
	    break;

	case ID_ELL:
	    analyze_general(gedp, ip);
	    break;

	case ID_TOR:
	    analyze_general(gedp, ip);
	    break;

	case ID_RPC:
	    analyze_general(gedp, ip);
	    break;

	case ID_ETO:
	    analyze_general(gedp, ip);
	    break;

	case ID_EPA:
	    analyze_general(gedp, ip);
	    break;

	case ID_PARTICLE:
	    analyze_general(gedp, ip);
	    break;

	case ID_SUPERELL:
	    analyze_superell(gedp, ip);
	    break;

	case ID_SKETCH:
	    analyze_sketch(gedp, ip);
	    break;

	case ID_HYP:
	    analyze_general(gedp, ip);
	    break;

	case ID_PIPE:
	    analyze_general(gedp, ip);
	    break;

	case ID_VOL:
	    analyze_general(gedp, ip);
	    break;

	default:
	    bu_vls_printf(gedp->ged_result_str, "\nanalyze: unable to process %s solid\n",
			  OBJ[ip->idb_type].ft_name);
	    break;
    }
}


int
ged_analyze(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *ndp;
    int i;
    struct rt_db_internal intern;
    static const char *usage = "object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /* use the names that were input */
    for (i = 1; i < argc; i++) {
	if ((ndp = db_lookup(gedp->ged_wdbp->dbip,  argv[i], LOOKUP_NOISY)) == RT_DIR_NULL)
	    continue;

	GED_DB_GET_INTERNAL(gedp, &intern, ndp, bn_mat_identity, &rt_uniresource, GED_ERROR);

	_ged_do_list(gedp, ndp, 1);
	analyze_do(gedp, &intern);
	rt_db_free_internal(&intern);
    }

    return GED_OK;
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
