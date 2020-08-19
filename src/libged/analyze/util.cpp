#include "common.h"

extern "C" {
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
}

#include <limits>

extern "C" {
#include "bu/malloc.h"
#include "bu/vls.h"
#include "../ged_private.h"
#include "./ged_analyze.h"
}

#define FBUFSIZ 100
#define NFIELDS 9

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



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

