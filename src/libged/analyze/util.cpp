/*                        U T I L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file util.cpp
 *
 * Brief description
 *
 */

#include "common.h"

extern "C" {
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
}

#include <cassert>
#include <limits>

extern "C" {
#include "bu/malloc.h"
#include "bu/vls.h"
#include "../ged_private.h"
#include "./ged_analyze.h"
}

void get_dashes(field_t *f, const int ndashes)
{
    int i;
    f->buf[0] = '\0';
    for (i = 0; i < ndashes; ++i) {
	bu_strlcat(f->buf, "-", FBUFSIZ);
    }
    f->nchars = ndashes;
}

void
print_edges_table(struct ged *gedp, table_t *table)
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


void
print_faces_table(struct ged *gedp, table_t *table)
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
    int nd;
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
    nd = c0;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%-*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+-");
    nd = h1a + h1b + h1c + 1 + h2a + h2b + h2c;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+-");
    nd = c2a + c2b + c2c;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+-");
    nd = f7a + f7b + f7c;
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
    nd = c0;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%-*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+-");
    nd = h1a + h1b + h1c + 1 + h2a + h2b + h2c;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+-");
    nd = c2a + c2b + c2c;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+-");
    nd = f7a + f7b + f7c;
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
	for (j = 0; j < table->rows[i].nfields - 1; ++j) {

	    // For Coverity - check to ensure we never overrun maxwidth
	    if (j == 8) {
		bu_log("util.cpp:%d - trying to print more than 8 columns (?)\n", __LINE__);
		break;
	    }
	    assert(table->rows[i].fields[j].buf);

	    bu_vls_printf(gedp->ged_result_str, " %*.*s", maxwidth[j], maxwidth[j], table->rows[i].fields[j].buf);

	    /* do we need a separator? */
	    if (j == 0 || j == 2 || j == 6 || j == 7)
		bu_vls_printf(gedp->ged_result_str, " |");

	}
	/* close the row */
	bu_vls_printf(gedp->ged_result_str, "\n");
    }

    /* close the table with the ender row */
    bu_vls_printf(gedp->ged_result_str, "+-");
    nd = c0;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%-*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+-");
    nd = h1a + h1b + h1c + 1 + h2a + h2b + h2c;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+-");
    nd = c2a + c2b + c2c;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+-");
    nd = f7a + f7b + f7c;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%*.*s",
		  nd, nd, dashes.buf);
    bu_vls_printf(gedp->ged_result_str, "-+\n");
}

void
print_volume_table(
	struct ged *gedp,
	const fastf_t tot_vol,
	const fastf_t tot_area,
	const fastf_t tot_gallons
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
    const char* fnames[3] = {"Volume", "Surface Area", "Gallons"};
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


/**
 * finds direction cosines and rotation, fallback angles of a unit vector
 * angles = pointer to 5 fastf_t's to store angles
 * unitv = pointer to the unit vector (previously computed)
 */
static void
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
	    angles[i] = acos(unitv[i]) * RAD2DEG;
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
	    angles[3] = RAD2DEG * acos(f);
    } else
	angles[3] = 0.0;

    if (unitv[Y] < 0)
	angles[3] = 360.0 - angles[3];

    angles[4] *= RAD2DEG;
}



/**
 * general analyze function for polygonal faces.
 * Currently used for:
 * - arb8
 * - arbn
 * - ars
 *
 * returns:
 * - area in face->area
 * - print_faces_table() information in row
 * - sorts vertices in face->pts into ccw order
 */
void
analyze_poly_face(struct ged *gedp, struct poly_face *face, row_t *row)
{
    fastf_t angles[5];

    findang(angles, face->plane_eqn);

    /* sort points */
    bg_3d_polygon_sort_ccw(face->npts, face->pts, face->plane_eqn);
    bg_3d_polygon_area(&face->area, face->npts, (const point_t *)face->pts);

    /* store face information for pretty printing */
    row->nfields = 8;
    row->fields[0].nchars = sprintf(row->fields[0].buf, "%4s", face->label);
    row->fields[1].nchars = sprintf(row->fields[1].buf, "%10.8f", angles[3]);
    row->fields[2].nchars = sprintf(row->fields[2].buf, "%10.8f", angles[4]);
    row->fields[3].nchars = sprintf(row->fields[3].buf, "%10.8f", face->plane_eqn[X]);
    row->fields[4].nchars = sprintf(row->fields[4].buf, "%10.8f", face->plane_eqn[Y]);
    row->fields[5].nchars = sprintf(row->fields[5].buf, "%10.8f", face->plane_eqn[Z]);
    row->fields[6].nchars = sprintf(row->fields[6].buf, "%10.8f",
				    face->plane_eqn[W]*gedp->dbip->dbi_base2local);
    row->fields[7].nchars = sprintf(row->fields[7].buf, "%10.8f",
				    face->area*gedp->dbip->dbi_base2local*gedp->dbip->dbi_base2local);
}

/**
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
 * - rhc
 */
void
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
		      centroid[X] * gedp->dbip->dbi_base2local,
		      centroid[Y] * gedp->dbip->dbi_base2local,
		      centroid[Z] * gedp->dbip->dbi_base2local);
    }

    print_volume_table(gedp,
		       vol
		       * gedp->dbip->dbi_base2local
		       * gedp->dbip->dbi_base2local
		       * gedp->dbip->dbi_base2local,
		       area
		       * gedp->dbip->dbi_base2local
		       * gedp->dbip->dbi_base2local,
		       vol/GALLONS_TO_MM3
	);
}




// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

