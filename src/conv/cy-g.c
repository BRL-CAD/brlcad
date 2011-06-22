/*                          C Y - G . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file conv/cy-g.c
 *
 *	This routine converts Cyberware Digitizer Data (laser scan data)
 *	to a single BRL-CAD ARS solid. The data must be in cylindrical scan
 *	format (Cyberware Echo file format).
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"


#define LINE_LEN 256
#define CY_DATA "CY_DATA"

#define INDEX(lt, lg) ((lg) * nlt + (lt))

#define VOIDVAL (0xffff8000 << rshift)
#define MAXVAL (0x00007fff << rshift)

static const char usage[] = "Usage:\n\tcy-g input_laser_scan_file.cy output_brlcad_file.g\n";

int
main(int argc, char **argv)
{
    FILE *infp = NULL;
    struct rt_wdb *outfp = NULL;

    short *data = NULL;

    char *cptr = NULL;
    char line[LINE_LEN] = {0};
    char name[LINE_LEN] = {0};
    char date[LINE_LEN] = {0};
    char space[LINE_LEN] = {0};
    char color[LINE_LEN] = {0};

    int x = 0;
    int y = 0;
    int lt = 0;
    int lg = 0;
    int nlg = 512;
    int nlt = 256;
    int lgshift = 0;
    int ltshift = 0;
    int rshift = 5;
    int rmax = 0;
    int rmin = 0;
    int lgincr = 0;
    int lgmin = 0;
    int lgmax = 0;
    int ltincr = 0;
    int ltmin = 0;
    int ltmax = 0;
    int first_non_zero = 30000;
    int last_non_zero = -1;
    int new_last = 0;
    int inside_out = 0;
    int theta_righthand = 0;
    int filled = 0;
    int smoothed = 0;

    fastf_t delta_z = 0.0;
    fastf_t delta_angle = 0.0;
    fastf_t angle = 0.0;
    fastf_t scale = 1000.0;
    fastf_t rprop = 100.0;

    fastf_t *sins = NULL;
    fastf_t *coss = NULL;
    fastf_t **curves = NULL;
    fastf_t *ptr = NULL;

    if (argc != 3) {
	bu_exit(1, "%s", usage);
    }

    if ((infp=fopen(argv[1], "rb")) == NULL) {
	bu_log("Cannot open input file (%s)\n", argv[1]);
	bu_exit(1, "%s", usage);
    }

    if ((outfp = wdb_fopen(argv[2])) == NULL) {
	bu_log("Cannot open output file (%s)\n", argv[2]);
	bu_exit(1, "%s", usage);
    }

    /* read first line, should identify */
    if (bu_fgets(line, LINE_LEN, infp) == NULL) {
	bu_exit(1, "Unexpected EOF encounterd while looking for file identifier\n");
    }
    if (line[strlen(line)-1] == '\n') {
	line[strlen(line)-1] = '\0';
    }
    if (strcasecmp(line, "Cyberware Digitizer Data") != 0) {
	bu_log("WARNING: Input file does not seem to be Cyberware Digitizer Data\n");
	bu_log("Trying to continue regardless...\n");
    }
    mk_id(outfp, line);
    bu_log("%s\n", line);

    /* initialize some values */
    snprintf(name, sizeof(name), CY_DATA);

    /* read ASCII header section.  the header is a list of KEY=VALUE
     * lines that ends when we encounter a final "DATA=\012" which
     * signals the start of the binary scan data.
     */
    while (1) {
	/* get a line from the header, strip the trailing newline */
	if (bu_fgets(line, LINE_LEN, infp) == NULL) {
	    bu_exit(1, "Unexpected EOF encountered while looking for data\n");
	}
	while (isspace(line[strlen(line)-1])) {
	    line[strlen(line)-1] = '\0';
	}

	/* start of the scan data marks the end of the header */
	if (strncasecmp("DATA=", line, 5) == 0) {
	    bu_log("Processing DATA\n");
	    break;
	}

	/* mandatory header items
	 *
	 * NLT, NLG, LGSHIFT, LTINCR, LGINCR, RSHIFT
	 */

	if (strncasecmp("NLG=", line, 4) == 0) {
	    /* number of longitude scan values */

	    cptr = line+4;
	    nlg = atoi(cptr);
	    db5_update_attribute("_GLOBAL", "NLG", cptr, outfp->dbip);
	    bu_log("NLG=%d\n", nlg);

	} else if (strncasecmp("NLT=", line, 4) == 0) {
	    /* number of latitude scan values */

	    cptr = line+4;
	    nlt = atoi(cptr);
	    db5_update_attribute("_GLOBAL", "NLT", cptr, outfp->dbip);
	    bu_log("NLT=%d\n", nlt);

	} else if (strncasecmp("LGSHIFT=", line, 8) == 0) {
	    /* rotate longitude values by (+/-) N positions */

	    cptr = line+8;
	    lgshift = atoi(cptr);
	    db5_update_attribute("_GLOBAL", "LGSHIFT", cptr, outfp->dbip);
	    bu_log("LGSHIFT=%d\n", lgshift);

	} else if (strncasecmp("LTINCR=", line, 7) == 0) {
	    /* latitude increment (in microns)
	     *
	     * The latitude increment is the separation between the
	     * 450 evenly spaced samples along the vertical axis. This
	     * separation is typically 1/450 of the 300 millimeter
	     * high field of view, or about 700 microns.
	     */

	    cptr = line+7;
	    ltincr = atoi(cptr);
	    db5_update_attribute("_GLOBAL", "LTINCR", cptr, outfp->dbip);
	    bu_log("LTINCR=%d\n", ltincr);

	} else if (strncasecmp("LGINCR=", line, 7) == 0) {
	    /* longitude increment (in microradians)
	     *
	     * For a cylindrical scan, the longitude increment is the
	     * angle between each of the profiles. With 512, this is
	     * 0.703125f or 12227 microradians.
	     */

	    cptr = line+7;
	    lgincr = atoi(cptr);
	    db5_update_attribute("_GLOBAL", "LGINCR", cptr, outfp->dbip);
	    bu_log("LGINCR=%d\n", lgincr);

	} else if (strncasecmp("RSHIFT=", line, 7) == 0) {
	    /* radius left shift (scale radius values by 2^RSHIFT) */

	    cptr = line+7;
	    rshift = atoi(cptr);
	    db5_update_attribute("_GLOBAL", "RSHIFT", cptr, outfp->dbip);
	    bu_log("RSHIFT=%d\n", rshift);

	} else

	    /* optional header items
	     *
	     * NAME, DATE, LTMIN, LTMAX, LGMIN, LGMAX, RMIN, RMAX, SCALE,
	     * RPROP, FILLED, SMOOTHED, SPACE, INSIDE_OUT, COLOR,
	     * THETA_RIGHTHAND
	     */

	    if (strncasecmp("NAME=", line, 5) == 0) {
		/* name of the scan subject */

		cptr = line+5;
		snprintf(name, sizeof(name), "%s", cptr);

		if (strlen(name) <= 0) {
		    bu_log("Encountered empty NAME, using \"%s\" as object name\n", CY_DATA);
		    snprintf(name, sizeof(name), "%s", CY_DATA);
		} else {
		    db5_update_attribute("_GLOBAL", "NAME", cptr, outfp->dbip);
		    bu_log("NAME=%s\n", name);
		}

	    } else if (strncasecmp("DATE=", line, 5) == 0) {
		/* date the scan was performed */

		cptr = line+5;
		snprintf(date, sizeof(date), "%s", cptr);
		if (strlen(date) <= 0) {
		    bu_log("Encountered empty DATE, ignoring\n");
		} else {
		    db5_update_attribute("_GLOBAL", "DATE", cptr, outfp->dbip);
		    bu_log("DATE=%s\n", date);
		}

	    } else if (strncasecmp("SPACE=", line, 6) == 0) {
		/* what kind of scan */

		cptr = line+6;
		snprintf(space, sizeof(date), "%s", cptr);
		db5_update_attribute("_GLOBAL", "SPACE", cptr, outfp->dbip);
		bu_log("SPACE=%s\n", space);

		if (strcasecmp(space, "CYLINDRICAL") != 0) {
		    /* don't support CARTESIAN or BILATERAL */
		    bu_log("Encountered SPACE=%s\n", space);
		    bu_exit(1, "%s only supports CYLINDRICAL scans\n", argv[0]);
		}

	    } else if (strncasecmp("COLOR=", line, 6) == 0) {
		/* unknown/unsupported */

		cptr = line+6;
		snprintf(color, sizeof(date), "%s", cptr);
		db5_update_attribute("_GLOBAL", "COLOR", cptr, outfp->dbip);
		bu_log("COLOR=%s\n", color);

		if (strcasecmp(color, "SGI") != 0) {
		    bu_log("Encountered unknown COLOR, ignoring\n");
		}

	    } else if (strncasecmp("LGMIN=", line, 6) == 0) {
		/* minimum longitude data window (inclusive) */

		cptr = line+6;
		lgmin = atoi(cptr);
		db5_update_attribute("_GLOBAL", "LGMIN", cptr, outfp->dbip);
		bu_log("LGMIN=%d\n", lgmin);

	    } else if (strncasecmp("LGMAX=", line, 6) == 0) {
		/* maximum longitude data window (inclusive) */

		cptr = line+6;
		lgmax = atoi(cptr);
		db5_update_attribute("_GLOBAL", "LGMAX", cptr, outfp->dbip);
		bu_log("LGMAX=%d\n", lgmax);

	    } else if (strncasecmp("LTMIN=", line, 6) == 0) {
		/* minimum latitude data window (inclusive) */

		cptr = line+6;
		ltmin = atoi(cptr);
		db5_update_attribute("_GLOBAL", "LTMIN", cptr, outfp->dbip);
		bu_log("LTMIN=%d\n", ltmin);

	    } else if (strncasecmp("LTMAX=", line, 6) == 0) {
		/* maximum latitude data window (inclusive) */

		cptr = line+6;
		ltmax = atoi(cptr);
		db5_update_attribute("_GLOBAL", "LTMAX", cptr, outfp->dbip);
		bu_log("LTMAX=%d\n", ltmax);

	    } else if (strncasecmp("RMIN=", line, 5) == 0) {
		/* minimum radius */

		cptr = line+5;
		rmin = atoi(cptr);
		db5_update_attribute("_GLOBAL", "RMIN", cptr, outfp->dbip);
		bu_log("RMIN=%d\n", rmin);

	    } else if (strncasecmp("RMAX=", line, 5) == 0) {
		/* maximum radius */

		cptr = line+5;
		rmax = atoi(cptr);
		db5_update_attribute("_GLOBAL", "RMAX", cptr, outfp->dbip);
		bu_log("RMAX=%d\n", rmax);

	    } else if (strncasecmp("LTSHIFT=", line, 8) == 0) {
		/* translate latitude values by (+/-) N positions,
		 * wrapping around at the latitude limit.
		 */

		cptr = line+8;
		ltshift = atoi(cptr);
		db5_update_attribute("_GLOBAL", "LTSHIFT", cptr, outfp->dbip);
		bu_log("LTSHIFT=%d\n", ltshift);

	    } else if (strncasecmp("SCALE=", line, 6) == 0) {
		/* scan value scaling factor */

		cptr = line+6;
		scale = atof(cptr);
		db5_update_attribute("_GLOBAL", "SCALE", cptr, outfp->dbip);
		bu_log("SCALE=%lf\n", scale);

	    } else if (strncasecmp("RPROP=", line, 6) == 0) {
		/* radius scaling factor */

		cptr = line+6;
		rprop = atof(cptr);
		db5_update_attribute("_GLOBAL", "RPROP", cptr, outfp->dbip);
		bu_log("RPROP=%lf\n", rprop);

	    } else if (strncasecmp("FILLED=", line, 7) == 0) {
		/* unknown/unsupported */

		cptr = line+6;
		filled = strncasecmp(cptr, "TRUE", 4) == 0 ? 1 : 0;
		db5_update_attribute("_GLOBAL", "FILLED", cptr, outfp->dbip);
		bu_log("FILLED=%d\n", filled);

	    } else if (strncasecmp("SMOOTHED=", line, 9) == 0) {
		/* unknown/unsupported */

		cptr = line+9;
		smoothed = strncasecmp(cptr, "TRUE", 4) == 0 ? 1 : 0;
		db5_update_attribute("_GLOBAL", "SMOOTHED", cptr, outfp->dbip);
		bu_log("SMOOTHED=%d\n", smoothed);

	    } else if (strncasecmp("INSIDE_OUT=", line, 11) == 0) {
		/* unsupported, presumably being scanned from the inside */

		cptr = line+11;
		inside_out = strncasecmp(cptr, "TRUE", 4) == 0 ? 1 : 0;
		db5_update_attribute("_GLOBAL", "INSIDE_OUT", cptr, outfp->dbip);
		bu_log("INSIDE_OUT=%d\n", inside_out);

	    } else if (strncasecmp("THETA_RIGHTHAND=", line, 16) == 0) {
		/* unsupported */

		cptr = line+16;
		theta_righthand = strncasecmp(cptr, "TRUE", 4) == 0 ? 1 : 0;
		db5_update_attribute("_GLOBAL", "THETA_RIGHTHAND", cptr, outfp->dbip);
		bu_log("INSIDE_OUT=%d\n", theta_righthand);

	    } else {
		bu_log("UNKNOWN HEADER LINE: %s (skipping)\n", line);
	    }
    }


    delta_z = (fastf_t)(ltincr)/scale;

    /* calculate angle between longitudinal measurements */
    delta_angle = bn_twopi/(fastf_t)nlg;

    /* allocate memory to hold vertices */
    curves = (fastf_t **)bu_malloc((nlt+2)*sizeof(fastf_t **), "ars curve pointers");
    for (y=0; y<nlt+2; y++) {
	curves[y] = (fastf_t *)bu_calloc((unsigned int)(nlg+1)*3, sizeof(fastf_t), "ars curve");
    }

    /* allocate memory for a table os sines and cosines */
    sins = (fastf_t *)bu_calloc((unsigned int)nlg+1, sizeof(fastf_t), "sines");
    coss = (fastf_t *)bu_calloc((unsigned int)nlg+1, sizeof(fastf_t), "cosines");

    /* fill in the sines and cosines table */
    for (x=0; x<nlg; x++) {
	angle = delta_angle * (fastf_t)x;
	sins[x] = sin(angle);
	coss[x] = cos(angle);
    }
    sins[nlg] = sins[0];
    coss[nlg] = coss[0];

    /* DATA contains NLT * NLG * 2 bytes data values */
    data = (short *)bu_malloc(nlt * nlg * sizeof(short), "data");
/*
  bu_log("VOIDVAL is %d\n", VOIDVAL);
  bu_log("MAXVAL is %d\n", MAXVAL);
*/

    /* read int the actual data values */
    for (lg=0; lg<nlg; lg++) {
	for (lt=0; lt<nlt; lt++) {
	    short r;

	    if (fread(&r, sizeof(short), 1, infp) != 1)
		bu_exit(1, "Unexpected end-of-file encountered in [%s]\n", argv[1]);

	    data[INDEX(lt, lg)] = r;
	}
    }

    /* read the actual data */
    for (x=0; x<nlg; x++) {
	fastf_t z=0.0;

	for (y=0; y<nlt; y++) {
	    short r;
	    long radius;
	    fastf_t rad;

	    ptr = &curves[y+1][x*3];

	    r = data[INDEX(y,x)];
	    if ((r << rshift) == (short)VOIDVAL) {
		/* skip void values */
		continue;
	    } else if (r < 0) {
/*		bu_log("FOUND NEGATIVE VALUE at %d,%d\n", x, y);*/
		rad = 0.0;
	    } else {
		if (y < first_non_zero)
		    first_non_zero = y;
		radius = (long)(r) << rshift;
		rad = (fastf_t)radius/rprop;
		if (y > last_non_zero)
		    last_non_zero = y;
	    }
	    *ptr = rad * coss[x];
	    *(ptr+1) = rad * sins[x];
	    *(ptr+2) = z;
/*			bu_log("%d %d: %g (%d) (%g %g %g)\n", x, y, rad, r, V3ARGS(ptr)); */

	    /* duplicate the first point at the end of the curve */
	    if (x == 0) {
		ptr = &curves[y+1][nlg*3];
		*ptr = rad * coss[x];
		*(ptr+1) = rad * sins[x];
		*(ptr+2) = z;
	    }
	    z += delta_z;
	}
    }

    /* finished with input file */
    fclose(infp);

    /* eliminate single vertex spikes on each curve */
    for (y=first_non_zero; y<=last_non_zero; y++) {
	int is_zero=1;

	for (x=0; x<nlg; x++) {
	    fastf_t *next, *prev;

	    ptr = &curves[y][x*3];
	    if (x == 0)
		prev = &curves[y][nlg*3];
	    else
		prev = ptr - 3;
	    next = ptr + 3;

	    if (!ZERO(ptr[0]) || !ZERO(ptr[1])) {
		if (ZERO(prev[0]) &&
		    ZERO(prev[1]) &&
		    ZERO(next[0]) &&
		    ZERO(next[1]))
		{
		    ptr[0] = 0.0;
		    ptr[1] = 0.0;
		} else {
		    is_zero = 0;
		}
	    }
	}
	if (is_zero && first_non_zero == y)
	    first_non_zero = y + 1;
	else
	    new_last = y;
    }

    last_non_zero = new_last;

    /* write out ARS solid
     * First curve is all zeros (first_non_zero - 1)
     * Last curve is all zeros (last_non_zero + 1)
     * Number of curves is (last_non_zero - first_non_zero + 2)
     */
    mk_ars(outfp, name, last_non_zero - first_non_zero + 2, nlg, &curves[first_non_zero-1]);

    bu_free(data, "data");
    wdb_close(outfp);
    return 0;
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
