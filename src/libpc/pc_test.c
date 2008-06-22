/*                   	P C _ T E S T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2012 United States Government as represented by
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
/** @file pc_test.c
 *
 * Simple test file for checking various aspects of libpc through the
 * initial development phase
 *
 * Immediate jobs: Include cleanup
 * Short term: Clean up commandline argument parsing
 *
 * @author	Dawn Thomas
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
    int i,ret;
    struct rt_wdb *fp;
    struct directory *dp;
    struct rt_db_internal ip;
    struct rt_ell_internal *eip;
    struct pc_param_set ps;
    struct pc_pc_set pcs;

    ps.len=4;
    ps.p = (genptr_t)bu_malloc(ps.len * sizeof(struct pc_parameter), "pc_parameter");
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

    /*rt_init_resource(&rt_uniresource,0,NULL);*/
    if(argc!=2) {
	bu_exit(1,"Too few arguments, Please provide output filename\n");
    }

    if((fp = wdb_fopen(argv[1])) == NULL) {
	perror(argv[2]);
	return 1;
    }
    mk_id(fp,"Parametrics test");
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

    if ((dp = db_lookup(fp->dbip,solnam,LOOKUP_QUIET)) == DIR_NULL)
	return 2;
    /*rt_db_get_internal(&intern, dp, fp->dbip, NULL, &rt_uniresource);*/

    ps.p[0].name= "radius";
    ps.p[1].name= "center-x";
    ps.p[2].name= "center-y";
    ps.p[3].name= "center-z";
    for(i=0; i<4; i++) {
	ps.p[i].value=2.33 + i;
	ps.p[i].min=-1.0 + i;
	ps.p[i].max=1.0 + i;
	ps.p[i].step=0.0001+ i;
	ps.p[i].parametrized=1;
    }
    pc_write_param_set(ps, dp, fp->dbip);
    /*	pc_generate_parameters(&ps, dp, fp->dbip);*/

    pc_mk_constraint(fp,"Constraint",0);
    if ((dp = db_lookup(fp->dbip,"Constraint",LOOKUP_QUIET)) == DIR_NULL)
	return 3;
    /*pc_write_parameter_set(ps, dp, fp->dbip);*/
    wdb_import(fp, &ip,solnam, (matp_t)NULL);
    ip.idb_meth->ft_params(&pcs,&ip);
    fprintf(stdout, "%s = ( %f , %f , %f ) %s = (%f,%f,%f) %s = (%f,%f,%f) %s = (%f,%f,%f)\n",\
	    pcs.ps[0].pname, pcs.ps[0].pointp[0],pcs.ps[0].pointp[1], \
	    pcs.ps[0].pointp[2], pcs.ps[1].pname, pcs.ps[1].vectorp[0], \
	    pcs.ps[1].vectorp[1], pcs.ps[1].vectorp[2], pcs.ps[2].pname, \
	    pcs.ps[2].vectorp[0], pcs.ps[2].vectorp[1], pcs.ps[2].vectorp[2], \
	    pcs.ps[3].pname, pcs.ps[3].vectorp[0], pcs.ps[3].vectorp[1], \
	    pcs.ps[3].vectorp[2]);
    /* Todo: Free pcs parametric set */

    wdb_close(fp);

    return ret;
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
