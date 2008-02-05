/*                          C Y - G . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2008 United States Government as represented by
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
/** @file cy-g.c
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

#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"


#define LINE_LEN 256
#define CY_DATA "CY_DATA"

static const char usage[] = "Usage:\n\tcy-g input_laser_scan_file.cy output_brlcad_file.g\n";

int
main(int argc, char **argv)
{
    FILE *infp = NULL;
    struct rt_wdb *outfp = NULL;

    char *cptr = NULL;
    char line[LINE_LEN] = {0};
    char name[LINE_LEN] = {0};
    char date[LINE_LEN] = {0};
    char space[LINE_LEN] = {0};
    char color[LINE_LEN] = {0};

    int x = 0;
    int y = 0;
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
    if (strcmp(line, "Cyberware Digitizer Data") != 0) {
	bu_log("WARNING: Input file does not seem to be Cyberware Digitizer Data\n");
	bu_log("Trying to continue regardless...\n");
    }
    mk_id(outfp, line);
    bu_log("%s\n", line);

    /* initialize some values */
    snprintf(name, sizeof(name), CY_DATA);

    /* read ASCII header section */
    while (1) {
	/* get a line from the header, strip the newline */
	if (bu_fgets(line, LINE_LEN, infp) == NULL) {
	    bu_exit(1, "Unexpected EOF encountered while looking for data\n");
	}
	while (isspace(line[strlen(line)-1])) {
	    line[strlen(line)-1] = '\0';
	}

	if (strncmp("DATA", line, 4) == 0) {
	    /* start of the scan data */

	    bu_log("Processing DATA\n");
	    break;

	} else if (strncmp("NAME", line, 4) == 0) {
	    /* name of the scan */

	    cptr = strchr(line, '=');
	    if (!cptr) {
		bu_exit(1, "Error parsing NAME line: %s\n", line);
	    }

	    snprintf(name, sizeof(name), "%s", ++cptr);

	    if (strlen(name) <= 0) {
		bu_log("Encountered empty NAME, using \"%s\" as object name\n", CY_DATA);
		snprintf(name, sizeof(name), "%s", CY_DATA);
	    } else {
		db5_update_attribute("_GLOBAL", "NAME", cptr, outfp->dbip);
		bu_log("NAME=%s\n", name);
	    }

	} else if (strncmp("DATE", line, 4) == 0) {
	    /* date the scan was performed */

	    cptr = strchr(line, '=');
	    if (!cptr) {
		bu_exit(1, "Error parsing DATE line: %s\n", line);
	    }

	    snprintf(date, sizeof(date), "%s", ++cptr);
	    if (strlen(date) <= 0) {
		bu_log("Encountered empty DATE, ignoring\n");
	    } else {
		db5_update_attribute("_GLOBAL", "DATE", cptr, outfp->dbip);
		bu_log("DATE=%s\n", date);
	    }

	} else if (strncmp("SPACE", line, 5) == 0) {
	    /* what kind of scan */

	    cptr = strchr(line, '=');
	    if (!cptr) {
		bu_exit(1, "Error parsing SPACE line: %s\n", line);
	    }

	    snprintf(space, sizeof(date), "%s", ++cptr);
	    db5_update_attribute("_GLOBAL", "SPACE", cptr, outfp->dbip);
	    bu_log("SPACE=%s\n", space);

	    if (strcmp(space, "CYLINDRICAL") != 0) {
		bu_log("Encountered SPACE=%s\n", space);
		bu_exit(1, "%s only supports CYLINDRICAL scans\n", argv[0]);
	    }

	} else if (strncmp("COLOR", line, 4) == 0) {
	    /* whether texture mode is RGB, Luminance, or None */

	    cptr = strchr(line, '=');
	    if (!cptr) {
		bu_exit(1, "Error parsing COLOR line: %s\n", line);
	    }

	    snprintf(color, sizeof(date), "%s", ++cptr);
	    db5_update_attribute("_GLOBAL", "COLOR", cptr, outfp->dbip);
	    bu_log("COLOR=%s (IGNORED)\n", color);

	    if (strcmp(color, "SGI") != 0) {
		bu_log("Encountered unknown COLOR, ignoring\n");
	    }

	} else if (strncmp("NLG", line, 3) == 0) {
	    /* number of longitude values */

	    cptr = strchr(line, '=');
	    if (!cptr) {
		bu_exit(1, "Error parsing NLG line: %s\n", line);
	    }

	    nlg = atoi(++cptr);
	    db5_update_attribute("_GLOBAL", "NLG", cptr, outfp->dbip);
	    bu_log("NLG=%d\n", nlg);

	} else if (strncmp("NLT", line, 3) == 0) {
	    /* number of latitude values */

	    cptr = strchr(line, '=');
	    if (!cptr) {
		bu_exit(1, "Error parsing NLT line: %s\n", line);
	    }

	    nlt = atoi(++cptr);
	    db5_update_attribute("_GLOBAL", "NLT", cptr, outfp->dbip);
	    bu_log("NLT=%d\n", nlt);

	} else if (strncmp("LGINCR", line, 6) == 0) {
	    /* longitude increment (in microradians) 
	     *
	     * For a cylindrical scan, the longitude increment is the
	     * angle between each of the profiles. With 512, this is
	     * 0.703125f or 12227 microradians.
	     */

	    cptr = strchr(line, '=');
	    if (!cptr) {
		bu_exit(1, "Error parsing LGINCR line: %s\n", line);
	    }

	    lgincr = atoi(++cptr);
	    db5_update_attribute("_GLOBAL", "LGINCR", cptr, outfp->dbip);
	    bu_log("LGINCR=%d (ignored)\n", lgincr);

	} else if (strncmp("LGMIN", line, 5) == 0) {
	    /* minimum longitude */

	    cptr = strchr(line, '=');
	    if (!cptr) {
		bu_exit(1, "Error parsing LGMIN line: %s\n", line);
	    }

	    lgmin = atoi(++cptr);
	    db5_update_attribute("_GLOBAL", "LGMIN", cptr, outfp->dbip);
	    bu_log("LGMIN=%d (ignored)\n", lgmin);

	} else if (strncmp("LGMAX", line, 5) == 0) {
	    /* maximum longitude */

	    cptr = strchr(line, '=');
	    if (!cptr) {
		bu_exit(1, "Error parsing LGMAX line: %s\n", line);
	    }

	    lgmax = atoi(++cptr);
	    db5_update_attribute("_GLOBAL", "LGMAX", cptr, outfp->dbip);
	    bu_log("LGMAX=%d (ignored)\n", lgmax);

	} else if (strncmp("LTINCR", line, 6) == 0) {
	    /* latitude increment (in microns)
	     *
	     * The latitude increment is the separation between the
	     * 450 evenly spaced samples along the vertical axis. This
	     * separation is typically 1/450 of the 300 millimeter
	     * high field of view, or about 700 microns.
	     */

	    cptr = strchr(line, '=');
	    if (!cptr) {
		bu_exit(1, "Error parsing LTINCR line: %s\n", line);
	    }

	    ltincr = atoi(++cptr);
	    db5_update_attribute("_GLOBAL", "LTINCR", cptr, outfp->dbip);
	    bu_log("LTINCR=%d\n", ltincr);

	} else if (strncmp("LTMIN", line, 5) == 0) {
	    /* minimum latitude */

	    cptr = strchr(line, '=');
	    if (!cptr) {
		bu_exit(1, "Error parsing LTMIN line: %s\n", line);
	    }

	    ltmin = atoi(++cptr);
	    db5_update_attribute("_GLOBAL", "LTMIN", cptr, outfp->dbip);
	    bu_log("LTMIN=%d (ignored)\n", ltmin);

	} else if (strncmp("LTMAX", line, 5) == 0) {
	    /* maximum latitude */

	    cptr = strchr(line, '=');
	    if (!cptr) {
		bu_exit(1, "Error parsing LTMAX line: %s\n", line);
	    }

	    ltmax = atoi(++cptr);
	    db5_update_attribute("_GLOBAL", "LTMAX", cptr, outfp->dbip);
	    bu_log("LTMAX=%d (ignored)\n", ltmax);

	} else if (strncmp("RMIN", line, 4) == 0) {
	    /* minimum radius */

	    cptr = strchr(line, '=');
	    if (!cptr) {
		bu_exit(1, "Error parsing RMIN line: %s\n", line);
	    }

	    rmin = atoi(++cptr);
	    db5_update_attribute("_GLOBAL", "RMIN", cptr, outfp->dbip);
	    bu_log("RMIN=%d (ignored)\n", rmin);

	} else if (strncmp("RMAX", line, 4) == 0) {
	    /* maximum radius */

	    cptr = strchr(line, '=');
	    if (!cptr) {
		bu_exit(1, "Error parsing RMAX line: %s\n", line);
	    }

	    rmax = atoi(++cptr);
	    db5_update_attribute("_GLOBAL", "RMAX", cptr, outfp->dbip);
	    bu_log("RMAX=%d (ignored)\n", rmax);

	} else if (strncmp("RSHIFT", line, 6) == 0) {
	    /* radius left shift (scale radius values by 2^RSHIFT) */

	    cptr = strchr(line, '=');
	    if (!cptr) {
		bu_exit(1, "Error parsing RSHIFT line: %s\n", line);
	    }

	    rshift = atoi(++cptr);
	    db5_update_attribute("_GLOBAL", "RSHIFT", cptr, outfp->dbip);
	    bu_log("RSHIFT=%d\n", rshift);

	} else if (strncmp("LGSHIFT", line, 6) == 0) {
	    /* rotate longitude values by (+/-) N positions */

	    cptr = strchr(line, '=');
	    if (!cptr) {
		bu_exit(1, "Error parsing LGSHIFT line: %s\n", line);
	    }

	    lgshift = atoi(++cptr);
	    db5_update_attribute("_GLOBAL", "LGSHIFT", cptr, outfp->dbip);
	    bu_log("LGSHIFT=%d (IGNORED)\n", lgshift);

	} else if (strncmp("LTSHIFT", line, 6) == 0) {
	    /* translate latitude values by (+/-) N positions,
	     * wrapping around at the latitude limit.
	     */

	    cptr = strchr(line, '=');
	    if (!cptr) {
		bu_exit(1, "Error parsing LTSHIFT line: %s\n", line);
	    }

	    ltshift = atoi(++cptr);
	    db5_update_attribute("_GLOBAL", "LTSHIFT", cptr, outfp->dbip);
	    bu_log("LTSHIFT=%d (IGNORED)\n", ltshift);

	} else if (strncmp("SCALE", line, 5) == 0) {
	    /* scan value scaling factor */

	    cptr = strchr(line, '=');
	    if (!cptr) {
		bu_exit(1, "Error parsing SCALE line: %s\n", line);
	    }

	    scale = atof(++cptr);
	    db5_update_attribute("_GLOBAL", "SCALE", cptr, outfp->dbip);
	    bu_log("SCALE=%lf\n", scale);

	} else if (strncmp("RPROP", line, 5) == 0) {
	    /* radius scaling factor */

	    cptr = strchr(line, '=');
	    if (!cptr) {
		bu_exit(1, "Error parsing RPROP line: %s\n", line);
	    }

	    rprop = atof(++cptr);
	    db5_update_attribute("_GLOBAL", "RPROP", cptr, outfp->dbip);
	    bu_log("RPROP=%lf\n", rprop);

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
	curves[y] = (fastf_t *)bu_calloc((size_t)(nlg+1)*3, sizeof(fastf_t), "ars curve");
    }

    /* allocate memory for a table os sines and cosines */
    sins = (fastf_t *)bu_calloc((size_t)nlg+1, sizeof(fastf_t), "sines");
    coss = (fastf_t *)bu_calloc((size_t)nlg+1, sizeof(fastf_t), "cosines");

    /* fill in the sines and cosines table */
    for (x=0; x<nlg; x++) {
	angle = delta_angle * (fastf_t)x;
	sins[x] = sin(angle);
	coss[x] = cos(angle);
    }
    sins[nlg] = sins[0];
    coss[nlg] = coss[0];

    /* read the actual data */
    for (x=0; x<nlg; x++) {
	fastf_t z=0.0;

	for (y=0; y<nlt; y++) {
	    short r;
	    long radius;
	    fastf_t rad;

	    ptr = &curves[y+1][x*3];

	    if (fread(&r, 2, 1, infp) != 1)
		bu_exit(1, "Unexpected end-of-file encountered in [%s]\n", argv[1]);
	    if (r < 0) {
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

	    if (!NEAR_ZERO(ptr[0], SMALL_FASTF) || !NEAR_ZERO(ptr[1], SMALL_FASTF)) {
		if (NEAR_ZERO(prev[0], SMALL_FASTF) &&
		    NEAR_ZERO(prev[1], SMALL_FASTF) &&
		    NEAR_ZERO(next[0], SMALL_FASTF) &&
		    NEAR_ZERO(next[1], SMALL_FASTF))
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
