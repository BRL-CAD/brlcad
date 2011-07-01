/*                   	P C _ T E S T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file util/pc_test.c
 *
 * Simple test file for checking various aspects of libpc through the
 * initial development phase
 *
 * Immediate jobs: Include cleanup
 * Short term: Clean up commandline argument parsing
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bio.h"

#include "db.h"
#include "raytrace.h"
#include "wdb.h"
#include "rtgeom.h"
#include "pc.h"

int
main(int argc, char **argv)
{
    struct rt_wdb *fp;
    struct directory *dp;
    struct rt_db_internal ip;
    struct pc_pc_set pcs;

    point_t cent;
    fastf_t rad;
    char solnam[9];
    /* Set up solid name */
    solnam[0] = 's';
    solnam[1] = '.';
    solnam[2] = 's';
    solnam[3] = 'p';
    solnam[4] = 'h';
    solnam[5] = ' ';
    solnam[6] = 'P';
    solnam[7] = '1';
    solnam[8] = '\0';

    /*rt_init_resource(&rt_uniresource, 0, NULL);*/
    if (argc!=2) {
	bu_exit(1, "Too few arguments, Please provide output filename\n");
    }

    if ((fp = wdb_fopen(argv[1])) == NULL) {
	perror(argv[2]);
	return 1;
    }
    mk_id(fp, "Parametrics test");
    cent[0] = 3.4;
    cent[1] = 4.5;
    cent[2] = 5.3;
    rad = 53.2;
    mk_sph(fp, solnam, cent, rad);

    solnam[0] = 's';
    solnam[1] = '.';
    solnam[2] = 's';
    solnam[3] = 'p';
    solnam[4] = 'h';
    solnam[5] = ' ';
    solnam[6] = 'P';
    solnam[7] = '2';
    solnam[8] = '\0';

    cent[0] = 23.4;
    cent[1] = 34.5;
    cent[2] = 45.3;
    rad = 153.2;
    mk_sph(fp, solnam, cent, rad);

    if ((dp = db_lookup(fp->dbip, solnam, LOOKUP_QUIET)) == RT_DIR_NULL)
	return 2;
    /*rt_db_get_internal(&intern, dp, fp->dbip, NULL, &rt_uniresource);*/


    mk_constraint(fp, "Constraint", 0);
    if ((dp = db_lookup(fp->dbip, "Constraint", LOOKUP_QUIET)) == RT_DIR_NULL)
	return 3;
    wdb_import(fp, &ip, solnam, (matp_t)NULL);
    ip.idb_meth->ft_params(&pcs, &ip);
    
    /* Todo: Free pcs parametric set */

    wdb_close(fp);

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
