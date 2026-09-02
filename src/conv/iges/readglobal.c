/*                    R E A D G L O B A L . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2026 United States Government as represented by
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

#include "./iges_struct.h"
#include "./iges_extern.h"

/* Conversion Factors (to mm)	*/
fastf_t cnv[] = {
/* default, inch, mm, ?, feet,  miles, meters, kilometers, mils, microns,
   cm, microinches */
    1.0,    25.4, 1.0, 1.0, 304.8, 1609344.0, 1000.0, 1000000.0, 0.0254, 0.001,
    10.0, 0.0000254	};

/* IGES Version */
#define NO_OF_VERSIONS 10
#define COMMA ','

const char *iges_version[NO_OF_VERSIONS] = {
    " ",
    "1.0",
    "ANSI Y14.26M - 1981",
    "2.0",
    "3.0",
    "ANSI Y14.26M - 1987",
    "4.0",
    "ASME Y14.26M - 1989",
    "5.0",
    "5.1" };

/*
 * Parse the IGES Global Section from the shared global "card" buffer.
 * First determines the field and record delimiters, then walks the
 * remaining global parameter fields, dispatching each to the appropriate
 * Read* routine and recording the scale, units, conversion factor, etc.
 * Auto-advances to the next record when a field crosses a record boundary.
 * The switch "case" values below are the IGES Global Section field numbers.
 */
void
Readglobal(int file_count)
{

    int field = 2, i;
    fastf_t a;
    char *name;


    /* Get End-of-field delimiter */
    if (card[counter] != COMMA) {
	counter--;
	while (card[++counter] == ' ');
	if (card[counter] != '1' || card[counter+1] != 'H') {
	    bu_log("Error in new delimiter\n");
	    bu_log("%s\n", card);
	    for (i = 0; i < counter - 1; i++)
		bu_log("%c", ' ');
	    bu_exit(1, "^\n");
	}
	counter++;
	eofd = card[++counter];
	while (card[++counter] != eofd);
    } else
	eofd = COMMA;


    /* Get End-of-record delimiter */
    if (card[++counter] != eofd) {
	counter--;
	while (card[++counter] == ' ');
	if (card[counter] != '1' || card[counter+1] != 'H')
	    bu_exit(1, "Error in new record delimiter\n");
	counter++;
	eord = card[++counter];
	while (card[++counter] != eofd);
    } else
	eord = ';';


    /* Read all the fields in the Global Section */
    counter++;
    while (field < 23) {
	if (card[counter-1] == eord) {
	    Readrec(++currec);
	    break;
	}

	/* each "case" is the IGES Global Section field number being read */
	switch (++field) {
	    case 3:		Readname(&name, "Product ID: ");	/* field 3: product identification (sending system) */
		if (!file_count) {
		    if (name != NULL) {
			mk_id(fdout, name);
			bu_free(name, "Readglobal: name");
		    } else
			mk_id(fdout, "Un-named Product");
		}
		break;
	    case 4:		Readstrg("File Name: ");	/* field 4: file name */
		break;
	    case 5:		Readstrg("System ID: ");	/* field 5: native system ID */
		break;
	    case 6:		Readstrg("Version: ");	/* field 6: preprocessor version */
		break;
	    case 7:		Readint(&i, "Integer Bits: ");	/* field 7: number of bits per integer */
		break;
	    case 8:		Readint(&i, "Max Power of ten(single precision): ");	/* field 8: single-precision max power of ten */
		break;
	    case 9:		Readint(&i, "Max significant digits (single precision): ");	/* field 9: single-precision significant digits */
		break;
	    case 10:	Readint(&i, "Max Power of ten(double precision): ");	/* field 10: double-precision max power of ten */
		break;
	    case 11:	Readint(&i, "Max significant digits (single precision): ");	/* field 11: double-precision significant digits */
		break;
	    case 12:	Readstrg("Product ID: ");	/* field 12: product identification (receiving system) */
		break;
	    case 13:	Readflt(&scale, "Scale: ");	/* field 13: model space scale */
		if (ZERO(scale))
		    scale = 1.0;
		inv_scale = 1.0/scale;
		break;
	    case 14:	Readint(&units, "Units: ");	/* field 14: units flag */
		if (units < 0 || units == 0 || units == 3 || units > 11) {
		    bu_log("Unrecognized units, assuming 'mm'\n");
		    conv_factor = 1.0;
		} else
		    conv_factor = cnv[units];
		/* make "conv_factor" account for both units and
		   scale factor */
		conv_factor *= inv_scale;
		break;
	    case 15:	Readstrg("Units: ");	/* field 15: units name */
		break;
	    case 16:	Readint(&i, "Line Weight Gradations: ");	/* field 16: max number of line weight gradations */
		break;
	    case 17:	Readflt(&a, "Line Width: ");	/* field 17: width of max line weight */
		break;
	    case 18:	Readtime("Exchange File Created On: ");	/* field 18: file generation date/time */
		break;
	    case 19:	Readflt(&a, "Resolution: ");	/* field 19: minimum resolution (granularity) */
		break;
	    case 20:	Readflt(&a, "Maximum value: ");	/* field 20: approximate maximum coordinate value */
		break;
	    case 21:	Readstrg("Author: ");	/* field 21: author name */
		break;
	    case 22:	Readstrg("Organization: ");	/* field 22: author's organization */
		break;
	    case 23:	Readint(&i, "");	/* field 23: IGES version (specification) flag */
		if (i < 1 || i >= NO_OF_VERSIONS)
		    bu_log("Unrecognized IGES version\n");
		else
		    bu_log("IGES version: %s\n", iges_version[i]);
		break;
	    case 24:	Readint(&i, "");	/* field 24: drafting standard flag */
		break;
	    case 25:	Readtime("Model Last Modified: ");	/* field 25: model creation/modification date/time */
		break;
	}
    }
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
