/*                         B R E P . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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
/** @file brep.c
 *
 * The brep command.
 *
 */

#include "common.h"

#include "bio.h"

#include "./ged_private.h"

extern int brep_surface_plot(struct ged *gedp, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp,int index);

int
ged_brep(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    struct bn_vlblock*vbp;
    FILE *fp;
    double char_size;
    char *solid_name;
    char *command;
    static const char *usage = "brepsolidname [command]";
    register struct directory *ndp;
    struct rt_db_internal	intern;
    struct rt_brep_internal* bi;
    struct brep_specific* bs;
    struct soltab *stp;
    int real_flag;
    
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);
    
    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    
    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	bu_vls_printf(&gedp->ged_result_str, "commands:\n");
	bu_vls_printf(&gedp->ged_result_str, "\tinfo - return count information for specific BREP\n");
	bu_vls_printf(&gedp->ged_result_str, "\tinfo S [index] - return information for specific BREP 'surface'\n");
	bu_vls_printf(&gedp->ged_result_str, "\tinfo F [index] - return information for specific BREP 'face'\n");
	bu_vls_printf(&gedp->ged_result_str, "\tplot - plot entire BREP\n");
	bu_vls_printf(&gedp->ged_result_str, "\tplot S [index] - plot specific BREP 'surface'\n");
	bu_vls_printf(&gedp->ged_result_str, "\tplot F [index] - plot specific BREP 'face'\n");
	return GED_HELP;
    }
    
    if (argc < 2 || argc > 6) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }
    
    solid_name = (char *)argv[1];
    if ( (ndp = db_lookup( gedp->ged_wdbp->dbip,  solid_name, LOOKUP_NOISY )) == DIR_NULL ) {
	bu_vls_printf(&gedp->ged_result_str, "Error: %s is not a solid or does not exist in database", solid_name);
	return GED_ERROR;
    } else {
        real_flag = (ndp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
    }
    
    if (!real_flag) {
        /* solid doesnt exists - don't kill */
        bu_vls_printf(&gedp->ged_result_str, "Error: %s is not a real solid", solid_name);
        return GED_OK;
    }
    
    GED_DB_GET_INTERNAL(gedp, &intern, ndp, bn_mat_identity, &rt_uniresource, GED_ERROR);
    
    
    RT_CK_DB_INTERNAL(&intern);
    bi = (struct rt_brep_internal*)intern.idb_ptr;
    
    if (!RT_BREP_TEST_MAGIC(bi)) {
        /* solid doesnt exists - don't kill */
        bu_vls_printf(&gedp->ged_result_str, "Error: %s is not a BREP solid.", solid_name);
        return GED_OK;
    }
    
    BU_GETSTRUCT(stp, soltab);
    
    if ((bs = (struct brep_specific*)stp->st_specific) == NULL) {
	bs = (struct brep_specific*)bu_malloc(sizeof(struct brep_specific), "brep_specific");
	bs->brep = bi->brep;
	bi->brep = NULL;
	stp->st_specific = (genptr_t)bs;
    }
    
    if (argc == 2)
	command = "info";
    else
	command = (char *)argv[2];
    
    if ( strcmp(command,"info") == 0) {
	if ( strcmp(command,"?") == 0) {
	    bu_vls_printf( &gedp->ged_result_str, "%s", usage);
	    bu_vls_printf(&gedp->ged_result_str, "\tinfo - return count information for specific BREP\n");
	    bu_vls_printf(&gedp->ged_result_str, "\tinfo S [index] - return information for specific BREP 'surface'\n");
	    bu_vls_printf(&gedp->ged_result_str, "\tinfo F [index] - return information for specific BREP 'face'\n");
	} else if (argc == 3) {
	    brep_info(bs, &gedp->ged_result_str);
	} else if (argc == 5) {
	    char *part = argv[3];
	    char *strindex = argv[4];
	    if (strcmp(strindex,"?") == 0) {
		//printout indedx options
		bu_vls_printf(&gedp->ged_result_str, "\tinfo S [index] - return information for specific BREP 'surface'\n");
		bu_vls_printf(&gedp->ged_result_str, "\tinfo F [index] - return information for specific BREP 'face'\n");
	    } else {
		int index = atoi(strindex);
		if (strcmp(part,"S") == 0) {
		    bu_vls_printf( &gedp->ged_result_str, "%s - ", solid_name);
		    brep_surface_info(bs, &gedp->ged_result_str,index);
		} else if (strcmp(part,"F") == 0) {
		    bu_vls_printf( &gedp->ged_result_str, "%s - ", solid_name);
		    brep_face_info(bs, &gedp->ged_result_str,index);
		}
	    }
	}
    } else if ( strcmp(command,"plot") == 0) {
	if ( strcmp(command,"?") == 0) {
	    bu_vls_printf( &gedp->ged_result_str, "%s", usage);
	    bu_vls_printf(&gedp->ged_result_str, "\tplot - plot entire BREP");
	    bu_vls_printf(&gedp->ged_result_str, "\tplot S [index] - plot specific BREP 'surface'\n");
	    bu_vls_printf(&gedp->ged_result_str, "\tplot F [index] - plot specific BREP 'face'\n");
	    brep_info(bs, &gedp->ged_result_str);
	} else if (argc == 3) {
	    bu_vls_printf( &gedp->ged_result_str, "%s", usage);
	    brep_info(bs, &gedp->ged_result_str);
	} else if (argc == 5) {
	    char *part = argv[3];
	    char *strindex = argv[4];
	    if (strcmp(strindex,"?") == 0) {
		//printout indedx options
		bu_vls_printf(&gedp->ged_result_str, "\tplot S [index] - plot specific BREP 'surface'\n");
		bu_vls_printf(&gedp->ged_result_str, "\tplot F [index] - plot specific BREP 'face'\n");
		brep_info(bs, &gedp->ged_result_str);
	    } else {
		int index = atoi(strindex);
		if (strcmp(part,"S") == 0) {
		    bu_vls_printf(&gedp->ged_result_str, "%s plot:", solid_name);
		    
		    vbp = rt_vlblock_init();
		    brep_surface_plot(gedp,bs,bi,vbp,index);
		    ged_cvt_vlblock_to_solids(gedp, vbp, "_SURF_", 0);
		    rt_vlblock_free(vbp);
		    vbp = (struct bn_vlblock *)NULL;
		} else if (strcmp(part,"F") == 0) {
		    bu_vls_printf(&gedp->ged_result_str, "%s plot:", solid_name);
		    
		    vbp = rt_vlblock_init();
		    brep_facetrim_plot(gedp,bs,bi,vbp,index);
		    ged_cvt_vlblock_to_solids(gedp, vbp, "_FACE_", 0);
		    rt_vlblock_free(vbp);
		    vbp = (struct bn_vlblock *)NULL;
		}
	    }
	}
    }
    
    
    rt_db_free_internal(&intern, &rt_uniresource);
    
    return GED_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
