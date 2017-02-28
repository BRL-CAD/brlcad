/*                  A S S E M B L Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2017 United States Government as represented by
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
/** @file assembly.cpp
 *
 */

#include "common.h"
#include "creo-brl.h"
#include <string.h>

/* this routine is a filter for the feature visit routine
 * selects only "component" items (should be only parts and assemblies)
 */
extern "C" ProError
assembly_filter(ProFeature *feat, ProAppData *data)
{
    ProFeattype type;
    ProFeatStatus feat_stat;
    if (ProFeatureTypeGet(feat, &type) != PRO_TK_NO_ERROR || type != PRO_FEAT_COMPONENT) return PRO_TK_CONTINUE;
    if (ProFeatureStatusGet(feat, &feat_stat) != PRO_TK_NO_ERROR || feat_stat != PRO_FEAT_ACTIVE) return PRO_TK_CONTINUE;
    return PRO_TK_NO_ERROR;
}

/* routine that is called by feature visit for each assembly member
 * the "app_data" is the head of the assembly info for this assembly
 */
extern "C" ProError
assembly_gather( ProFeature *feat, ProError status, ProAppData app_data )
{
    ProError loc_status;
    ProMdl model;
    ProMdlType type;
    wchar_t wname[10000];
    char name[10000];
    wchar_t *wname_saved;
	struct creo_conv_info *cinfo = (struct creo_conv_info *)app_data;

    /* Get feature name */
    if ((loc_status = ProAsmcompMdlNameGet(feat, &type, wname)) != PRO_TK_NO_ERROR ) return creo_log(cinfo, MSG_FAIL, loc_status, "Failure getting name");
    (void)ProWstringToString(name, wname);

    /* get the model for this member */
    if ((loc_status = ProAsmcompMdlGet(feat, &model)) != PRO_TK_NO_ERROR) return creo_log(cinfo, MSG_FAIL, loc_status, "%s: failure getting model", name);

    /* get its type (part or assembly are the only ones that should make it here) */
    if ((loc_status = ProMdlTypeGet(model, &type)) != PRO_TK_NO_ERROR) return creo_log(cinfo, MSG_FAIL, loc_status, "%s: failure getting type", name);

    /* log this member */
    switch ( type ) {
	case PRO_MDL_ASSEMBLY:
	    if (cinfo->assems->find(wname) == cinfo->assems->end()) {
		wname_saved = (wchar_t *)bu_calloc(wcslen(wname)+1, sizeof(wchar_t), "CREO name");
		wcsncpy(wname_saved, wname, wcslen(wname)+1);
		cinfo->assems->insert(wname_saved);
		return ProSolidFeatVisit(ProMdlToPart(model), assembly_gather, (ProFeatureFilterAction)assembly_filter, app_data);
	    }
	    break;
	case PRO_MDL_PART:
	    if (cinfo->parts->find(wname) == cinfo->parts->end()) {
		wname_saved = (wchar_t *)bu_calloc(wcslen(wname)+1, sizeof(wchar_t), "CREO name");
		wcsncpy(wname_saved, wname, wcslen(wname)+1);
		cinfo->parts->insert(wname_saved);
	    }
	    break;
    }

    return PRO_TK_NO_ERROR;
}

extern "C" static ProError
assembly_check_empty( ProFeature *feat, ProError status, ProAppData app_data )
{
    ProError lstatus;
    ProMdlType type;
    wchar_t wname[10000];
	struct creo_conv_info *cinfo = (struct creo_conv_info *)app_data;
    int *has_shape = (int *)app_data;
    if ((lstatus = ProAsmcompMdlNameGet(feat, &type, wname)) != PRO_TK_NO_ERROR ) return lstatus;
    if (cinfo->empty->find(wname) == cinfo->empty->end()) (*has_shape) = 1;
    return PRO_TK_NO_ERROR;
}

/* run this only *after* output_parts - need that information */
extern "C" void
find_empty_assemblies(struct creo_conv_info *cinfo)
{
    int steady_state = 0;
    if (cinfo->empty->size() == 0) return;
    while (!steady_state) {
	std::set<wchar_t *, WStrCmp>::iterator d_it;
	steady_state = 1;
	for (d_it = cinfo->assems->begin(); d_it != cinfo->assems->end(); d_it++) {
	    /* for each assem, verify at least one child is non-empty.  If all
	     * children are empty, add to empty set and unset steady_state. */
	    int has_shape = 0;
	    ProMdl model;
	    if (ProMdlnameInit(*d_it, PRO_MDLFILE_ASSEMBLY, &model) == PRO_TK_NO_ERROR ) {
		if (cinfo->empty->find(*d_it) == cinfo->empty->end()) {
		    ProSolidFeatVisit(ProMdlToPart(model), assembly_check_empty, (ProFeatureFilterAction)assembly_filter, (ProAppData)&has_shape);
		    if (!has_shape) {
			cinfo->empty->insert(*d_it);
			steady_state = 0;
		    }
		}
	    }
	}
    }
}

extern "C" static ProError
assembly_entry_matrix(ProMdl parent, ProFeature *entry)
{
}

extern "C" static ProError
assembly_write_entry(ProFeature *feat, ProError status, ProAppData app_data)
{
    ProError lstatus;
	ProMdlType type;
    struct bu_vls entry_name = BU_VLS_INIT_ZERO;
    wchar_t wname[10000];
    char name[10000];
	struct creo_conv_info *cinfo = (struct creo_conv_info *)app_data;

    if ((lstatus = ProAsmcompMdlNameGet(feat, &type, wname)) != PRO_TK_NO_ERROR ) return lstatus;
    (void)ProWstringToString(name, wname);

    /* TODO: BRL-CAD name foo based on name - put result in entry_name */

    //(void)mk_addmember(bu_vls_addr(&prim_name), &(wcomb.l), NULL, 'u');
}

extern "C" static ProError
assembly_write(ProMdl model, ProError status, ProAppData app_data)
{
    struct wmember wcomb;
    struct bu_vls comb_name = BU_VLS_INIT_ZERO;
    wchar_t wname[10000];
    char name[10000];
	struct creo_conv_info *cinfo = (struct creo_conv_info *)app_data;

    /* Initial comb setup */
    ProMdlMdlnameGet(model, wname);
    (void)ProWstringToString(name, wname);

    /* TODO: BRL-CAD name foo based on name - put result in comb_name */

    BU_LIST_INIT(&wcomb.l);

    ProSolidFeatVisit( ProMdlToPart(model), assembly_write_entry, (ProFeatureFilterAction)assembly_filter, app_data);

    mk_lcomb(cinfo->wdbp, bu_vls_addr(&comb_name), &wcomb, 0, NULL, NULL, NULL, 0);

    return PRO_TK_NO_ERROR;
}



/* routine that is called by feature visit for each assembly member
 * the "app_data" is the head of the assembly info for this assembly
 */
extern "C" static ProError
assembly_comp( ProFeature *feat, ProError status, ProAppData app_data )
{
	ProError lstatus;
    ProIdTable id_table;
    ProMdl model;
    ProAsmcomppath comp_path;
    ProMatrix xform;
    ProMdlType type;
    wchar_t name[10000];
    ProCharLine astr;
    ProFileName msgfil;
	struct creo_conv_info *cinfo = (struct creo_conv_info *)app_data;
    struct asm_member *member, *prev=NULL;
    int i, j;

    ProStringToWstring(msgfil, CREO_BRL_MSG_FILE);
#if 0
    lstatus = ProAsmcompMdlNameGet( feat, &type, name );
    if ( lstatus != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "ProAsmcompMdlNameGet() failed\n" );
	return lstatus;
    }
    (void)ProWstringToString( cinfo->curr_part_name, name );
    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
	fprintf(cinfo->logger, "Processing assembly member %s\n", cinfo->curr_part_name );
    }

    /* the next two Pro/Toolkit calls are the only way I could find to get
     * the transformation matrix to apply to this member.
     * this call is creating a path from the assembly to this particular member
     * (assembly/member)
     */
	if(feat){
    id_table[0] = feat->id;
    status = ProAsmcomppathInit( (ProSolid)curr_assem->model, id_table, 1, &comp_path );
    if ( status != PRO_TK_NO_ERROR ) {
	snprintf( astr, sizeof(astr), "Failed to get path from %s to %s (aborting)", cinfo->curr_asm_name,
		cinfo->curr_part_name );
	(void)ProMessageDisplay(msgfil, "USER_ERROR", astr );
	ProMessageClear();
	fprintf( stderr, "%s\n", astr );
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	return status;
    }
	}

    /* this call accumulates the xform matrix along the path created above */
    status = ProAsmcomppathTrfGet( &comp_path, PRO_B_TRUE, xform );
    if ( status != PRO_TK_NO_ERROR ) {
		if(feat){
	snprintf( astr, sizeof(astr), "Failed to get transformation matrix %s/%s, error = %d, id = %d",
		cinfo->curr_asm_name, cinfo->curr_part_name, status, feat->id );
	(void)ProMessageDisplay(msgfil, "USER_ERROR", astr );
	}
	ProMessageClear();
	fprintf( stderr, "%s\n", astr );
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	return PRO_TK_NO_ERROR;
    }

    /* add this member to our assembly info */
    prev = NULL;
    member = curr_assem->members;
    if ( member ) {
	while ( member->next ) {
	    prev = member;
	    member = member->next;
	}
	BU_ALLOC(member->next, struct asm_member);
	prev = member;
	member = member->next;
    } else {
	BU_ALLOC(curr_assem->members, struct asm_member);
	member = curr_assem->members;
    }

    if ( !member ) {
	(void)ProMessageDisplay(msgfil, "USER_ERROR",
		"memory allocation for member failed" );
	ProMessageClear();
	fprintf( stderr, "memory allocation for member failed\n" );
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	return PRO_TK_GENERAL_ERROR;
    }
    member->next = NULL;

    /* capture its name */
    (void)ProWstringToString( member->name, name );

    /* copy xform matrix */
    for ( i=0; i<4; i++ ) {
	for ( j=0; j<4; j++ ) {
	    member->xform[i][j] = xform[i][j];
	}
    }

    /* get the model for this member */
    status = ProAsmcompMdlGet( feat, &model );
    if ( status != PRO_TK_NO_ERROR ) {
	snprintf( astr, sizeof(astr), "Failed to get model for component %s",
		cinfo->curr_part_name );
	(void)ProMessageDisplay(msgfil, "USER_ERROR", astr );
	ProMessageClear();
	fprintf( stderr, "%s\n", astr );
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	return status;
    }

    /* get its type (part or assembly are the only ones that should make it here) */
    status = ProMdlTypeGet( model, &type );
    if ( status != PRO_TK_NO_ERROR ) {
	snprintf( astr, sizeof(astr), "Failed to get type for component %s",
		cinfo->curr_part_name );
	(void)ProMessageDisplay(msgfil, "USER_ERROR", astr );
	ProMessageClear();
	fprintf( stderr, "%s\n", astr );
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	return status;
    }

    /* remember the type */
    member->type = type;

    /* output this member */
    switch ( type ) {
	case PRO_MDL_ASSEMBLY:
	    output_assembly(cinfo, model );
	    break;
	case PRO_MDL_PART:
	    if ( output_part(cinfo, model ) == 2 ) {
		/* part had no solid parts, eliminate from the assembly */
		if ( prev ) {
		    prev->next = NULL;
		} else {
		    curr_assem->members = NULL;
		}
		bu_free( (char *)member, "asm member" );
	    }
	    break;
    }
#endif
    return PRO_TK_NO_ERROR;
}




/* routine to check if xform is an identity */
extern "C" int
is_non_identity( ProMatrix xform )
{
    int i, j;

    for ( i=0; i<4; i++ ) {
	for ( j=0; j<4; j++ ) {
	    if ( i == j ) {
		if ( xform[i][j] != 1.0 )
		    return 1;
	    } else {
		if ( xform[i][j] != 0.0 )
		    return 1;
	    }
	}
    }

    return 0;
}

/* routine to output an assembly as a BRL-CAD combination
 * The combination will have the Pro/E name with a ".c" suffix.
 * Cannot just use the Pro/E name, because assembly can have the same name as a part.
 */
extern "C" void
output_assembly(struct creo_conv_info *cinfo, ProMdl model)
{
    ProName asm_name;
    ProMassProperty mass_prop;
    ProError status;
    ProBoolean is_exploded;
    ProCharLine astr;
    struct asm_member *member;
    int member_count=0;
    int i, j, k;
    int ret_status=0;

    if ( ProMdlNameGet( model, asm_name ) != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get model name for an assembly\n" );
	return;
    }

    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
	fprintf(cinfo->logger, "Processing assembly %s:\n", ProWstringToString( astr, asm_name ) );
    }

    /* let the user know we are doing something */

    status = ProUILabelTextSet( "creo_brl", "curr_proc", asm_name );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to update dialog label for currently processed assembly\n" );
	return;
    }

#if 0
    /* everything starts out in "curr_part_name", copy name to "curr_asm_name" */
    bu_strlcpy( cinfo->curr_asm_name, cinfo->curr_part_name, sizeof(cinfo->curr_asm_name) );

    /* start filling in assembly info */
    bu_strlcpy( curr_assem.name, cinfo->curr_part_name, sizeof(curr_assem.name) );
    curr_assem.members = NULL;
    curr_assem.model = model;

    /* make sure this assembly is not "exploded"!!!
     * some careless designers leave assemblies in exploded mode
     */

    status = ProAssemblyIsExploded(*(ProAssembly *)model, &is_exploded );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get explode status of %s\n", curr_assem.name );
	if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
	    fprintf(cinfo->logger, "Failed to get explode status of %s\n", curr_assem.name );
	}
    }

    if ( is_exploded ) {
	/* unexplode this assembly !!!! */
	status = ProAssemblyUnexplode(*(ProAssembly *)model );
	if ( status != PRO_TK_NO_ERROR ) {
	    fprintf( stderr, "Failed to un-explode assembly %s\n", curr_assem.name );
	    fprintf( stderr, "\tcomponents will be incorrectly positioned\n" );
	    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
		fprintf(cinfo->logger, "Failed to un-explode assembly %s\n", curr_assem.name );
		fprintf(cinfo->logger, "\tcomponents will be incorrectly positioned\n" );
	    }
	}
    }


    /* use feature visit to get info about assembly members.
     * also calls output functions for members (parts or assemblies)
     */
    struct app_data adata;
    adata.c_assem = &curr_assem;
    adata.cinfo = cinfo;
    status = ProSolidFeatVisit( ProMdlToPart(model), assembly_comp, (ProFeatureFilterAction)assembly_filter, (ProAppData)&adata);

    /* output the accumulated assembly info */
    fprintf(cinfo->outfp, "put {%s.c} comb region no tree ", get_brlcad_name(cinfo, curr_assem.name ) );

    /* count number of members */
    member = curr_assem.members;
    while ( member ) {
	member_count++;
	member = member->next;
    }

    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
	fprintf(cinfo->logger, "Output %d members of assembly\n", member_count );
    }

    /* output the "tree" */
    for ( i=1; i<member_count; i++ ) {
	fprintf(cinfo->outfp, "{u ");
    }

    member = curr_assem.members;
    i = 0;
    while ( member ) {
	/* output the member name */
	if ( member->type == PRO_MDL_ASSEMBLY ) {
	    fprintf(cinfo->outfp, "{l {%s.c}", get_brlcad_name(cinfo, member->name ) );
	} else {
	    fprintf(cinfo->outfp, "{l {%s}", get_brlcad_name(cinfo, member->name ) );
	}

	/* if there is an xform matrix, put it here */
	if ( is_non_identity( member->xform ) ) {
	    fprintf(cinfo->outfp, " {" );
	    for ( j=0; j<4; j++ ) {
		for ( k=0; k<4; k++ ) {
		    if ( k == 3 && j < 3 ) {
			fprintf(cinfo->outfp, " %.12e",
				member->xform[k][j] * cinfo->creo_to_brl_conv );
		    } else {
			fprintf(cinfo->outfp, " %.12e",
				member->xform[k][j] );
		    }
		}
	    }
	    fprintf(cinfo->outfp, "}" );
	}
	if ( i ) {
	    fprintf(cinfo->outfp, "}} " );
	} else {
	    fprintf(cinfo->outfp, "} " );
	}
	member = member->next;
	i++;
    }
    fprintf(cinfo->outfp, "\n" );

    fprintf(cinfo->outfp, "attr set {%s.c} %s %s\n",
	    get_brlcad_name(cinfo, curr_assem.name ),
	    CREO_NAME_ATTR,
	    curr_assem.name );

    /* calculate mass properties */
    if (cinfo->logger_type == LOGGER_TYPE_ALL ) {
	fprintf(cinfo->logger, "Getting mass properties for this assembly\n" );
    }

    status = ProSolidMassPropertyGet( ProMdlToSolid( model ), NULL, &mass_prop );
    if ( status == PRO_TK_NO_ERROR ) {
	if ( mass_prop.density > 0.0 ) {
	    fprintf(cinfo->outfp, "attr set {%s.c} density %g\n",
		    get_brlcad_name(cinfo, curr_assem.name ),
		    mass_prop.density );
	}
	if ( mass_prop.mass > 0.0 ) {
	    fprintf(cinfo->outfp, "attr set {%s.c} mass %g\n",
		    get_brlcad_name(cinfo, curr_assem.name ),
		    mass_prop.mass );
	}
	if ( mass_prop.volume > 0.0 ) {
	    fprintf(cinfo->outfp, "attr set {%s.c} volume %g\n",
		    get_brlcad_name(cinfo, curr_assem.name ),
		    mass_prop.volume );
	}
    }

    /* add this assembly to the list of already output objects */
    add_to_done_asm( cinfo, asm_name );

    /* free the memory associated with this assembly */
    free_assem(cinfo, &curr_assem );
#endif
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
