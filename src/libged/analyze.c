/*                          A N A L Y Z E . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2012 United States Government as represented by
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


/* division of an arb8 into 6 arb4s */
static const int farb4[6][4] = {
    {0, 1, 2, 4},
    {4, 5, 6, 1},
    {1, 2, 6, 4},
    {0, 2, 3, 4},
    {4, 6, 7, 2},
    {2, 3, 7, 4},
};


/* edge definition array */
static const int nedge[5][24] = {
    {0, 1, 1, 2, 2, 0, 0, 3, 3, 2, 1, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},	/* ARB4 */
    {0, 1, 1, 2, 2, 3, 0, 3, 0, 4, 1, 4, 2, 4, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1},		/* ARB5 */
    {0, 1, 1, 2, 2, 3, 0, 3, 0, 4, 1, 4, 2, 5, 3, 5, 4, 5, -1, -1, -1, -1, -1, -1},		/* ARB6 */
    {0, 1, 1, 2, 2, 3, 0, 3, 0, 4, 3, 4, 1, 5, 2, 6, 4, 5, 5, 6, 4, 6, -1, -1},			/* ARB7 */
    {0, 1, 1, 2, 2, 3, 0, 3, 0, 4, 4, 5, 1, 5, 5, 6, 6, 7, 4, 7, 3, 7, 2, 6},			/* ARB8 */
};

/* structures and subroutines for analyze pretty printing */

#define FBUFSIZ 100
#define NFIELDS   9
#define NROWS    12
#define NOT_A_PLANE -1
typedef struct row_field
{
  int  nchars;
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
  row_t rows[NROWS];
} table_t;

void get_dashes(field_t *f, const int ndashes)
{
  int i;
  f->buf[0] = '\0';
  for (i = 0; i < ndashes; ++i) {
    strcat(f->buf, "-");
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
    for (i = 0; i < table.nrows; ++i) {
        double val;

        /* field 0 */
        field = 0;
        table.rows[i].fields[0].nchars = sprintf(table.rows[i].fields[field].buf, "%s",
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
        table.rows[i].fields[1].nchars = sprintf(table.rows[i].fields[field].buf, "%10.8f",
                                                 val);
        if (maxwidth[field] < table.rows[i].fields[field].nchars)
            maxwidth[field] = table.rows[i].fields[field].nchars;
    }

    /* get total table width */
    table_width_chars  = maxwidth[0] + maxwidth[1];
    table_width_chars += 2 + 2; /* 2 chars at each end of a row */
    table_width_chars += 3; /* ' = ' between the two fields of a row */

    /* header row 1 */
    nd = table_width_chars - 4;
    get_dashes(&dashes, nd);
    bu_vls_printf(gedp->ged_result_str, "%-*.*s+-%-*.*s-+\n",
                  indent, indent, " ",
                  nd, nd, dashes.buf);

    /* the three data rows */
    for (i = 0; i < table.nrows; ++i) {
#if 1
        char tbuf[FBUFSIZ];
        sprintf(tbuf, "%-*.*s| %-*.*s = %*.*s |\n",
                indent, indent, " ",
                maxwidth[0], maxwidth[0], table.rows[i].fields[0].buf,
                maxwidth[1], maxwidth[1], table.rows[i].fields[1].buf);
        bu_vls_printf(gedp->ged_result_str, "%s", tbuf);
#else
        /* bu_vls_printf can't handle this at the moment */
        bu_vls_printf(gedp->ged_result_str, "%-*.*s| %-*.*s = %*.*s |\n",
                      indent, indent, " ",
                      maxwidth[0], maxwidth[0], table.rows[i].fields[0].buf,
                      maxwidth[1], maxwidth[1], table.rows[i].fields[1].buf);
#endif
    }

    /* closing table row */
    bu_vls_printf(gedp->ged_result_str, "%-*.*s+-%-*.*s-+\n",
                  indent, indent, " ",
                  nd, nd, dashes.buf);

}

void print_edges_table(struct ged *gedp, table_t *table)
{

/* table header

   +--------------------+--------------------+--------------------+--------------------+
   | EDGE          LEN  | EDGE          LEN  | EDGE          LEN  | EDGE          LEN  |
   +--------------------+--------------------+--------------------+--------------------+

*/

    int i; //j, k
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
    /* this table has 8 columns per row: 2 columns per edge; 4 edges per row  */

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

#if 1
    /* using sprintf because bu_vls_printf is broken for complex formats */
    sprintf(buf, "%-*.*s| %-*.*s %*.*s ",
            indent, indent, " ",
            maxwidth[0], maxwidth[0], EDGE,
            maxwidth[1], maxwidth[1], LEN);
    bu_vls_printf(gedp->ged_result_str, "%s", buf);

    sprintf(buf, "| %-*.*s %*.*s ",
            maxwidth[2], maxwidth[2], EDGE,
            maxwidth[3], maxwidth[3], LEN);
    bu_vls_printf(gedp->ged_result_str, "%s", buf);

    sprintf(buf, "| %-*.*s %*.*s ",
            maxwidth[4], maxwidth[4], EDGE,
            maxwidth[5], maxwidth[5], LEN);
    bu_vls_printf(gedp->ged_result_str, "%s", buf);

    sprintf(buf, "| %-*.*s %*.*s |\n",
            maxwidth[6], maxwidth[6], EDGE,
            maxwidth[7], maxwidth[7], LEN);
    bu_vls_printf(gedp->ged_result_str, "%s", buf);

#elif 0
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
#endif

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

        if (tcol == 0) {
          /* need to start a row */
          sprintf(buf, "%-*.*s|",
                  indent, indent, " ");
          bu_vls_printf(gedp->ged_result_str, "%s", buf);
        }

        /* data in sets of two */
        /* field 0 */
        int field = 0;
        /* using sprintf because bu_vls_printf is broken for complex formats */
        sprintf(buf, " %-*.*s",
                maxwidth[tcol], maxwidth[tcol], table->rows[i].fields[field].buf);
        bu_vls_printf(gedp->ged_result_str, "%s", buf);

        /* field 1 */
        field = 1;
        /* using sprintf because bu_vls_printf is broken for complex formats */
        sprintf(buf, " %-*.*s |",
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
            /* field 0 */
            int field = 0;
            /* using sprintf because bu_vls_printf is broken for complex formats */
            sprintf(buf, " %-*.*s",
                    maxwidth[tcol], maxwidth[tcol], " ");
            bu_vls_printf(gedp->ged_result_str, "%s", buf);

            /* field 1 */
            field = 1;
            /* using sprintf because bu_vls_printf is broken for complex formats */
            sprintf(buf, " %-*.*s |",
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

    /* print "FB"  in center of field 2 space  */
    h2b = strlen(FB);
    h2a = (maxwidth[2] - h2b)/2;
    h2c = (maxwidth[2] - h2b - h2a);

    /* get width of subcolumns of header column 2 */
    /* print "PLANE EQUATION" in center of columns 2 space  */
    c2 = maxwidth[3] + maxwidth[4] + maxwidth[5] + maxwidth[6] + 3; /* 3 spaces between fields */
    c2b = strlen(PA);
    c2a = (c2 - c2b)/2;
    c2c = (c2 - c2b - c2a);

    /* print "SURFACE AREA" in center of field 7 space  */
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

/*
 * F I N D A N G
 *
 * finds direction cosines and rotation, fallback angles of a unit vector
 * angles = pointer to 5 fastf_t's to store angles
 * unitv = pointer to the unit vector (previously computed)
 */
static void
findang(fastf_t *angles, fastf_t *unitv)
{
    fastf_t f;

    /* convert direction cosines into axis angles */
    if (unitv[X] <= -1.0) angles[X] = -90.0;
    else if (unitv[X] >= 1.0) angles[X] = 90.0;
    else angles[X] = acos(unitv[X]) * bn_radtodeg;

    if (unitv[Y] <= -1.0) angles[Y] = -90.0;
    else if (unitv[Y] >= 1.0) angles[Y] = 90.0;
    else angles[Y] = acos(unitv[Y]) * bn_radtodeg;

    if (unitv[Z] <= -1.0) angles[Z] = -90.0;
    else if (unitv[Z] >= 1.0) angles[Z] = 90.0;
    else angles[Z] = acos(unitv[Z]) * bn_radtodeg;

    /* fallback angle */
    if (unitv[Z] <= -1.0) unitv[Z] = -1.0;
    else if (unitv[Z] >= 1.0) unitv[Z] = 1.0;
    angles[4] = asin(unitv[Z]);

    /* rotation angle */
    /* For the tolerance below, on an SGI 4D/70, cos(asin(1.0)) != 0.0
     * with an epsilon of +/- 1.0e-17, so the tolerance below was
     * substituted for the original +/- 1.0e-20.
     */
    if ((f = cos(angles[4])) > 1.0e-16 || f < -1.0e-16) {
	f = unitv[X]/f;
	if (f <= -1.0)
	    angles[3] = 180;
	else if (f >= 1.0)
	    angles[3] = 0;
	else
	    angles[3] = bn_radtodeg * acos(f);
    }  else
           angles[3] = 0.0;

    if (unitv[Y] < 0)
	angles[3] = 360.0 - angles[3];

    angles[4] *= bn_radtodeg;
}


/* Analyzes an arb face
 */
static double
analyze_face(struct ged *gedp, int face, fastf_t *center_pt,
             const struct rt_arb_internal *arb, int type,
             const struct bn_tol *tol, row_t *row)


/* reference center point */


{
    int i, j, k;
    int a, b, c, d;	/* 4 points of face to look at */
    fastf_t angles[5];	/* direction cosines, rot, fb */
    fastf_t temp;
    fastf_t area[2], len[6];
    vect_t v_temp;
    plane_t plane;
    double face_area = 0;

    a = rt_arb_faces[type][face*4+0];
    b = rt_arb_faces[type][face*4+1];
    c = rt_arb_faces[type][face*4+2];
    d = rt_arb_faces[type][face*4+3];

    if (a == -1) {
        row->nfields = 0;
	return 0;
    }

    /* find plane eqn for this face */
    if (bn_mk_plane_3pts(plane, arb->pt[a], arb->pt[b],
			 arb->pt[c], tol) < 0) {
#if 1
	bu_vls_printf(gedp->ged_result_str, "| %d%d%d%d |         ***NOT A PLANE***                                          |\n",
		      a+1, b+1, c+1, d+1);
#endif
        /* this row has 1 special fields */
        row->nfields = NOT_A_PLANE;
	return 0;
    }

    /* The plane equations returned by planeqn above do not
     * necessarily point outward. Use the reference center
     * point for the arb and reverse direction for
     * any errant planes. This corrects the output rotation,
     * fallback angles so that they always give the outward
     * pointing normal vector.
     */
    if ((plane[W] - VDOT(center_pt, &plane[0])) < 0.0) {
	for (i = 0; i < 4; i++)
	    plane[i] *= -1.0;
    }

    /* plane[] contains normalized eqn of plane of this face
     * find the dir cos, rot, fb angles
     */
    findang(angles, &plane[0]);

    /* find the surface area of this face */
    for (i = 0; i < 3; i++) {
	j = rt_arb_faces[type][face*4+i];
	k = rt_arb_faces[type][face*4+i+1];
	VSUB2(v_temp, arb->pt[k], arb->pt[j]);
	len[i] = MAGNITUDE(v_temp);
    }
    len[4] = len[2];
    j = rt_arb_faces[type][face*4+0];
    for (i = 2; i < 4; i++) {
	k = rt_arb_faces[type][face*4+i];
	VSUB2(v_temp, arb->pt[k], arb->pt[j]);
	len[((i*2)-1)] = MAGNITUDE(v_temp);
    }
    len[2] = len[3];

    for (i = 0; i < 2; i++) {
	j = i * 3;
	temp = .5 * (len[j] + len[j+1] + len[j+2]);
	area[i] = sqrt(temp * (temp - len[j]) * (temp - len[j+1]) * (temp - len[j+2]));
	face_area += area[i];
    }

    /* don't printf, just sprintf! */
    /* these rows have 8 fields */
    row->nfields = 8;
    row->fields[0].nchars = sprintf(row->fields[0].buf, "%4d", prface[type][face]);
    row->fields[1].nchars = sprintf(row->fields[1].buf, "%10.8f", angles[3]);
    row->fields[2].nchars = sprintf(row->fields[2].buf, "%10.8f", angles[4]);
    row->fields[3].nchars = sprintf(row->fields[3].buf, "%10.8f", plane[X]);
    row->fields[4].nchars = sprintf(row->fields[4].buf, "%10.8f", plane[Y]);
    row->fields[5].nchars = sprintf(row->fields[5].buf, "%10.8f", plane[Z]);
    row->fields[6].nchars = sprintf(row->fields[6].buf, "%10.8f",
                                    plane[W]*gedp->ged_wdbp->dbip->dbi_base2local);
    row->fields[7].nchars = sprintf(row->fields[7].buf, "%10.8f",
                                    (area[0]+area[1])*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local );

    return face_area;
}


/* Analyzes arb edges - finds lengths */
static void
analyze_edge(struct ged *gedp, const int edge, const struct rt_arb_internal *arb,
             const int type, row_t *row)
{
    static vect_t v_temp;
    int a = nedge[type][edge*2];
    int b = nedge[type][edge*2+1];

    if (b == -1) {
	/* fill out the line */
        if ((a = edge % 4) == 0) {
            row->nfields = 0;
	    return;
        }
	if (a == 1) {
            row->nfields = 0;
	    return;
	}
	if (a == 2) {
            row->nfields = 0;
	    return;
	}
        row->nfields = 0;
	return;
    }

    VSUB2(v_temp, arb->pt[b], arb->pt[a]);

    row->nfields = 2;
    row->fields[0].nchars = sprintf(row->fields[0].buf, "%d%d", a + 1, b + 1);
    row->fields[1].nchars = sprintf(row->fields[1].buf, "%10.8f",
                                    MAGNITUDE(v_temp)*gedp->ged_wdbp->dbip->dbi_base2local);
}


/* Finds volume of an arb4 defined by farb4[loc][] 	*/
static double
analyze_find_vol(int loc, struct rt_arb_internal *arb, struct bn_tol *tol)
{
    int a, b, c, d;
    fastf_t vol, height, len[3], temp, areabase;
    vect_t v_temp;
    plane_t plane;

    /* a, b, c = base of the arb4 */
    a = farb4[loc][0];
    b = farb4[loc][1];
    c = farb4[loc][2];

    /* d = "top" point of arb4 */
    d = farb4[loc][3];

    if (bn_mk_plane_3pts(plane, arb->pt[a], arb->pt[b],
			 arb->pt[c], tol) < 0)
	return 0.0;

    /* have a good arb4 - find its volume */
    height = fabs(plane[W] - VDOT(&plane[0], arb->pt[d]));
    VSUB2(v_temp, arb->pt[b], arb->pt[a]);
    len[0] = MAGNITUDE(v_temp);
    VSUB2(v_temp, arb->pt[c], arb->pt[a]);
    len[1] = MAGNITUDE(v_temp);
    VSUB2(v_temp, arb->pt[c], arb->pt[b]);
    len[2] = MAGNITUDE(v_temp);
    temp = 0.5 * (len[0] + len[1] + len[2]);
    areabase = sqrt(temp * (temp-len[0]) * (temp-len[1]) * (temp-len[2]));
    vol = areabase * height / 3.0;
    return vol;
}

/*
 * A R B _ A N A L
 */
static void
analyze_arb(struct ged *gedp, const struct rt_db_internal *ip)
{
    struct rt_arb_internal *arb = (struct rt_arb_internal *)ip->idb_ptr;
    int i;
    point_t center_pt;
    double tot_vol;
    double tot_area;
    int cgtype;		/* COMGEOM arb type: # of vertices */
    int type;

    /* variables for pretty printing */
    table_t table;      /* holds table data from child functions */

    /* find the specific arb type, in GIFT order. */
    if ((cgtype = rt_arb_std_type(ip, &gedp->ged_wdbp->wdb_tol)) == 0) {
	bu_vls_printf(gedp->ged_result_str, "analyze_arb: bad ARB\n");
	return;
    }

    tot_area = tot_vol = 0.0;

    type = cgtype - 4;

    /* to get formatting correct, we need to collect the actual string
       lengths for each field BEFORE we start printing a table (fields
       are allowed to overflow the stated printf field width) */

    /* TABLE 1 =========================================== */
    /* analyze each face, use center point of arb for reference */
    rt_arb_centroid(center_pt, arb, cgtype);

    /* collect table data */
    table.nrows = 0;
    for (i = 0; i < 6; i++) {
      tot_area += analyze_face(gedp, i, center_pt, arb, type, &gedp->ged_wdbp->wdb_tol,
                               &(table.rows[i]));
      table.nrows += 1;
    }

    /* and print it */
    print_faces_table(gedp, &table);

    /* TABLE 2 =========================================== */
    /* analyze each edge */

    /* blank line following previous table */
    bu_vls_printf(gedp->ged_result_str, "\n");

    /* set up the records for arb4's and arb6's */
    /* also collect table data */
    table.nrows = 0;
    
    {
	struct rt_arb_internal earb = *arb;	/* struct copy */

        if (cgtype == 4) {
	    VMOVE(earb.pt[3], earb.pt[4]);
	} else if (cgtype == 6) {
	    VMOVE(earb.pt[5], earb.pt[6]);
	}

	for (i = 0; i < 12; i++) {
            analyze_edge(gedp, i, &earb, type, &(table.rows[i]));
	    if (nedge[type][i*2] == -1)
		break;
            table.nrows += 1;
	}
    }

    print_edges_table(gedp, &table);

    /* TABLE 3 =========================================== */
    /* find the volume - break arb8 into 6 arb4s */

    /* blank line following previous table */
    bu_vls_printf(gedp->ged_result_str, "\n");

    for (i = 0; i < 6; i++)
	tot_vol += analyze_find_vol(i, arb, &gedp->ged_wdbp->wdb_tol);

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

}


/* analyze a torus */
static void
analyze_tor(struct ged *gedp, const struct rt_db_internal *ip)
{
    struct rt_tor_internal *tor = (struct rt_tor_internal *)ip->idb_ptr;
    fastf_t r1, r2, vol, sur_area;

    RT_TOR_CK_MAGIC(tor);

    r1 = tor->r_a;
    r2 = tor->r_h;

    vol = 2.0 * M_PI * M_PI * r1 * r2 * r2;
    sur_area = 4.0 * M_PI * M_PI * r1 * r2;

    bu_vls_printf(gedp->ged_result_str, "\n");

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

#if 0
    //OR Vol = %.8f (%.8f gal) Surface Area = %.8f\n",

    bu_vls_printf(gedp->ged_result_str, "TOR Vol = %.8f (%.8f gal) Surface Area = %.8f\n",
		  vol*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  vol/GALLONS_TO_MM3,
		  sur_area*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local);
#endif

    return;
}


#define PROLATE 1
#define OBLATE 2

/* analyze an ell */
static void
analyze_ell(struct ged *gedp, const struct rt_db_internal *ip)
{
    struct rt_ell_internal *ell = (struct rt_ell_internal *)ip->idb_ptr;
    fastf_t ma, mb, mc;
#ifdef major		/* Some systems have these defined as macros!!! */
#undef major
#endif
#ifdef minor
#undef minor
#endif
    fastf_t ecc, major, minor;
    fastf_t vol, sur_area;
    int type;

    RT_ELL_CK_MAGIC(ell);

    ma = MAGNITUDE(ell->a);
    mb = MAGNITUDE(ell->b);
    mc = MAGNITUDE(ell->c);

    type = 0;

    vol = 4.0 * M_PI * ma * mb * mc / 3.0;
    bu_vls_printf(gedp->ged_result_str, "ELL Volume = %.8f (%.8f gal)",
		  vol*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  vol/GALLONS_TO_MM3);

    if (fabs(ma-mb) < .00001 && fabs(mb-mc) < .00001) {
	/* have a sphere */
	sur_area = 4.0 * M_PI * ma * ma;
	bu_vls_printf(gedp->ged_result_str, "   Surface Area = %.8f\n",
		      sur_area*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local);
	return;
    }

    if (fabs(ma-mb) < .00001) {
	/* A == B */
	if (mc > ma) {
	    /* oblate spheroid */
	    type = OBLATE;
	    major = mc;
	    minor = ma;
	} else {
	    /* prolate spheroid */
	    type = PROLATE;
	    major = ma;
	    minor = mc;
	}
    } else
	if (fabs(ma-mc) < .00001) {
	    /* A == C */
	    if (mb > ma) {
		/* oblate spheroid */
		type = OBLATE;
		major = mb;
		minor = ma;
	    } else {
		/* prolate spheroid */
		type = PROLATE;
		major = ma;
		minor = mb;
	    }
	} else
	    if (fabs(mb-mc) < .00001) {
		/* B == C */
		if (ma > mb) {
		    /* oblate spheroid */
		    type = OBLATE;
		    major = ma;
		    minor = mb;
		} else {
		    /* prolate spheroid */
		    type = PROLATE;
		    major = mb;
		    minor = ma;
		}
	    } else {
		bu_vls_printf(gedp->ged_result_str, "   Cannot find surface area\n");
		return;
	    }

    ecc = sqrt(major*major - minor*minor) / major;
    if (type == PROLATE) {
	sur_area = 2.0 * M_PI * minor * minor +
	    (2.0 * M_PI * (major*minor/ecc) * asin(ecc));
    } else { /* type == OBLATE */
	sur_area = 2.0 * M_PI * major * major +
	    (M_PI * (minor*minor/ecc) * log((1.0+ecc)/(1.0-ecc)));
    }

    bu_vls_printf(gedp->ged_result_str, "   Surface Area = %.8f\n",
		  sur_area*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local);
}


/* analyze an superell */
static void
analyze_superell(struct ged *gedp, const struct rt_db_internal *ip)
{
    struct rt_superell_internal *superell = (struct rt_superell_internal *)ip->idb_ptr;
    fastf_t ma, mb, mc;
#ifdef major		/* Some systems have these defined as macros!!! */
#undef major
#endif
#ifdef minor
#undef minor
#endif
    fastf_t ecc, major, minor;
    fastf_t vol, sur_area;
    int type;

    RT_SUPERELL_CK_MAGIC(superell);

    ma = MAGNITUDE(superell->a);
    mb = MAGNITUDE(superell->b);
    mc = MAGNITUDE(superell->c);

    type = 0;

    vol = 4.0 * M_PI * ma * mb * mc / 3.0;
    bu_vls_printf(gedp->ged_result_str, "SUPERELL Volume = %.8f (%.8f gal)",
		  vol*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  vol/GALLONS_TO_MM3);

    if (fabs(ma-mb) < .00001 && fabs(mb-mc) < .00001) {
	/* have a sphere */
	sur_area = 4.0 * M_PI * ma * ma;
	bu_vls_printf(gedp->ged_result_str, "   Surface Area = %.8f\n",
		      sur_area*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local);
	return;
    }
    if (fabs(ma-mb) < .00001) {
	/* A == B */
	if (mc > ma) {
	    /* oblate spheroid */
	    type = OBLATE;
	    major = mc;
	    minor = ma;
	} else {
	    /* prolate spheroid */
	    type = PROLATE;
	    major = ma;
	    minor = mc;
	}
    } else
	if (fabs(ma-mc) < .00001) {
	    /* A == C */
	    if (mb > ma) {
		/* oblate spheroid */
		type = OBLATE;
		major = mb;
		minor = ma;
	    } else {
		/* prolate spheroid */
		type = PROLATE;
		major = ma;
		minor = mb;
	    }
	} else
	    if (fabs(mb-mc) < .00001) {
		/* B == C */
		if (ma > mb) {
		    /* oblate spheroid */
		    type = OBLATE;
		    major = ma;
		    minor = mb;
		} else {
		    /* prolate spheroid */
		    type = PROLATE;
		    major = mb;
		    minor = ma;
		}
	    } else {
		bu_vls_printf(gedp->ged_result_str, "   Cannot find surface area\n");
		return;
	    }
    ecc = sqrt(major*major - minor*minor) / major;
    if (type == PROLATE) {
	sur_area = 2.0 * M_PI * minor * minor +
	    (2.0 * M_PI * (major*minor/ecc) * asin(ecc));
    } else { /* type == OBLATE */
	sur_area = 2.0 * M_PI * major * major +
	    (M_PI * (minor*minor/ecc) * log((1.0+ecc)/(1.0-ecc)));
    }

    bu_vls_printf(gedp->ged_result_str, "   Surface Area = %.8f\n",
		  sur_area*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local);
}


#define GED_ANAL_RCC 1
#define GED_ANAL_TRC 2
#define GED_ANAL_REC 3

/* analyze tgc */
static void
analyze_tgc(struct ged *gedp, const struct rt_db_internal *ip)
{
    struct rt_tgc_internal *tgc = (struct rt_tgc_internal *)ip->idb_ptr;
    fastf_t maxb, ma, mb, mc, md, mh;
    fastf_t area_base, area_top, area_side, vol;
    vect_t axb;
    int cgtype = 0;

    RT_TGC_CK_MAGIC(tgc);

    VCROSS(axb, tgc->a, tgc->b);
    maxb = MAGNITUDE(axb);
    ma = MAGNITUDE(tgc->a);
    mb = MAGNITUDE(tgc->b);
    mc = MAGNITUDE(tgc->c);
    md = MAGNITUDE(tgc->d);
    mh = MAGNITUDE(tgc->h);

    /* check for right cylinder */
    if (fabs(fabs(VDOT(tgc->h, axb)) - (mh*maxb)) < .00001) {
	/* have a right cylinder */
	if (fabs(ma-mb) < .00001) {
	    /* have a circular base */
	    if (fabs(mc-md) < .00001) {
		/* have a circular top */
		if (fabs(ma-mc) < .00001)
		    cgtype = GED_ANAL_RCC;
		else
		    cgtype = GED_ANAL_TRC;
	    }
	} else {
	    /* have an elliptical base */
	    if (fabs(ma-mc) < .00001 && fabs(mb-md) < .00001)
		cgtype = GED_ANAL_REC;
	}
    }

    switch (cgtype) {

	case GED_ANAL_RCC:
	    area_base = M_PI * ma * ma;
	    area_top = area_base;
	    area_side = 2.0 * M_PI * ma * mh;
	    vol = M_PI * ma * ma * mh;
	    bu_vls_printf(gedp->ged_result_str, "RCC ");
	    break;

	case GED_ANAL_TRC:
	    area_base = M_PI * ma * ma;
	    area_top = M_PI * mc * mc;
	    area_side = M_PI * (ma+mc) * sqrt((ma-mc)*(ma-mc)+(mh*mh));
	    vol = M_PI * mh * (ma*ma + mc*mc + ma*mc) / 3.0;
	    bu_vls_printf(gedp->ged_result_str, "TRC ");
	    break;

	case GED_ANAL_REC:
	    area_base = M_PI * ma * mb;
	    area_top = M_PI * mc * md;
	    /* approximate */
	    area_side = 2.0 * M_PI * mh * sqrt(0.5 * (ma*ma + mb*mb));
	    vol = M_PI * ma * mb * mh;
	    bu_vls_printf(gedp->ged_result_str, "REC ");
	    break;

	default:
	    bu_vls_printf(gedp->ged_result_str, "TGC Cannot find areas and volume\n");
	    return;
    }

    /* print the results */
    bu_vls_printf(gedp->ged_result_str, "Surface Areas:  base(AxB)=%.8f  top(CxD)=%.8f  side=%.8f\n",
		  area_base*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  area_top*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  area_side*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local);
    bu_vls_printf(gedp->ged_result_str, "Total Surface Area=%.8f    Volume=%.8f (%.8f gal)\n",
		  (area_base+area_top+area_side)*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  vol*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local, vol/GALLONS_TO_MM3);
    /* Print units? */
    return;

}


/* analyze ars */
static void
analyze_ars(struct ged *gedp, const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    bu_vls_printf(gedp->ged_result_str, "ARS analyze not implemented\n");
}


/* XXX analyze spline needed
 * static void
 * analyze_spline(gedp->ged_result_str, ip)
 * struct bu_vls *vp;
 * const struct rt_db_internal *ip;
 * {
 * bu_vls_printf(gedp->ged_result_str, "SPLINE analyze not implemented\n");
 * }
 */

/* analyze particle */
static void
analyze_part(struct ged *gedp, const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    bu_vls_printf(gedp->ged_result_str, "PARTICLE analyze not implemented\n");
}


#define arcsinh(x) (log((x) + sqrt((x)*(x) + 1.)))

/* analyze rpc */
static void
analyze_rpc(struct ged *gedp, const struct rt_db_internal *ip)
{
    fastf_t area_parab, area_body, b, h, r, vol_parab;
    struct rt_rpc_internal *rpc = (struct rt_rpc_internal *)ip->idb_ptr;

    RT_RPC_CK_MAGIC(rpc);

    b = MAGNITUDE(rpc->rpc_B);
    h = MAGNITUDE(rpc->rpc_H);
    r = rpc->rpc_r;

    /* area of one parabolic side */
    area_parab = 4./3 * b*r;

    /* volume of rpc */
    vol_parab = area_parab*h;

    /* surface area of parabolic body */
    area_body = .5*sqrt(r*r + 4.*b*b) + .25*r*r/b*arcsinh(2.*b/r);
    area_body *= 2.;

    bu_vls_printf(gedp->ged_result_str, "Surface Areas:  front(BxR)=%.8f  top(RxH)=%.8f  body=%.8f\n",
		  area_parab*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  2*r*h*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  area_body*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local);
    bu_vls_printf(gedp->ged_result_str, "Total Surface Area=%.8f    Volume=%.8f (%.8f gal)\n",
		  (2*area_parab+2*r*h+area_body)*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  vol_parab*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  vol_parab/GALLONS_TO_MM3);
}


/* analyze rhc */
static void
analyze_rhc(struct ged *gedp, const struct rt_db_internal *ip)
{
    fastf_t area_hyperb, area_body, b, c, h, r, vol_hyperb,	work1;
    struct rt_rhc_internal *rhc = (struct rt_rhc_internal *)ip->idb_ptr;

    RT_RHC_CK_MAGIC(rhc);

    b = MAGNITUDE(rhc->rhc_B);
    h = MAGNITUDE(rhc->rhc_H);
    r = rhc->rhc_r;
    c = rhc->rhc_c;

    /* area of one hyperbolic side (from macsyma) WRONG!!!! */
    work1 = sqrt(b*(b + 2.*c));
    area_hyperb = -2.*r*work1*(.5*(b+c) + c*c*log(c/(work1 + b + c)));

    /* volume of rhc */
    vol_hyperb = area_hyperb*h;

    /* surface area of hyperbolic body */
    area_body=0.;
#if 0
    k = (b+c)*(b+c) - c*c;
#define X_eval(y) sqrt(1. + (4.*k)/(r*r*k*k*(y)*(y) + r*r*c*c))
#define L_eval(y) .5*k*(y)*X_eval(y) \
	+ r*k*(r*r*c*c + 4.*k - r*r*c*c/k)*arcsinh((y)*sqrt(k)/c)
    area_body = 2.*(L_eval(r) - L_eval(0.));
#endif

    bu_vls_printf(gedp->ged_result_str, "Surface Areas:  front(BxR)=%.8f  top(RxH)=%.8f  body=%.8f\n",
		  area_hyperb*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  2*r*h*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  area_body*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local);
    bu_vls_printf(gedp->ged_result_str, "Total Surface Area=%.8f    Volume=%.8f (%.8f gal)\n",
		  (2*area_hyperb+2*r*h+2*area_body)*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  vol_hyperb*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local*gedp->ged_wdbp->dbip->dbi_base2local,
		  vol_hyperb/GALLONS_TO_MM3);
}


/*
 * Analyze command - prints loads of info about a solid
 * Format:	analyze [name]
 * if 'name' is missing use solid being edited
 */

/* Analyze solid in internal form */
static void
analyze_do(struct ged *gedp, const struct rt_db_internal *ip)
{
    /* XXX Could give solid name, and current units, here */

    switch (ip->idb_type) {

	case ID_ARS:
	    analyze_ars(gedp, ip);
	    break;

	case ID_ARB8:
	    analyze_arb(gedp, ip);
	    break;

	case ID_TGC:
	    analyze_tgc(gedp, ip);
	    break;

	case ID_ELL:
	    analyze_ell(gedp, ip);
	    break;

	case ID_TOR:
	    analyze_tor(gedp, ip);
	    break;

	case ID_RPC:
	    analyze_rpc(gedp, ip);
	    break;

	case ID_RHC:
	    analyze_rhc(gedp, ip);
	    break;

	case ID_PARTICLE:
	    analyze_part(gedp, ip);
	    break;

	case ID_SUPERELL:
	    analyze_superell(gedp, ip);
	    break;

	default:
	    bu_vls_printf(gedp->ged_result_str, "analyze: unable to process %s solid\n",
			  rt_functab[ip->idb_type].ft_name);
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
