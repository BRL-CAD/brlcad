/*                    M A I N . C P P
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
/** @file main.cpp
 *
 */

#include "common.h"
#include "creo-brl.h"

extern "C" void
creo_conv_info_init(struct creo_conv_info *cinfo)
{
    int i;

    /* Region ID */
    cinfo->reg_id = 1000;

    /* File settings */
    cinfo->logger = (FILE *)NULL;
    cinfo->logger_type=LOGGER_TYPE_NONE;
    cinfo->curr_msg_type = MSG_DEBUG;
    cinfo->print_to_console=1;

    /* units - model */
    cinfo->creo_to_brl_conv = 25.4; /* inches to mm */
    cinfo->local_tol=0.0;
    cinfo->local_tol_sq=0.0;

    /* facetization settings */
    cinfo->max_error=1.5;
    cinfo->min_error=1.5;
    cinfo->tol_dist=0.0005;
    cinfo->max_angle_cntrl=0.5;
    cinfo->min_angle_cntrl=0.5;
    cinfo->max_to_min_steps = 1;
    cinfo->error_increment=0.0;
    cinfo->angle_increment=0.0;

    /* csg settings */
    cinfo->min_hole_diameter=0.0;
    cinfo->min_chamfer_dim=0.0;
    cinfo->min_round_radius=0.0;

    cinfo->parts = new std::set<wchar_t *, WStrCmp>;
    cinfo->assems = new std::set<wchar_t *, WStrCmp>;
    cinfo->assem_child_cnts = new std::map<wchar_t *, int, WStrCmp>;
    cinfo->empty = new std::set<wchar_t *, WStrCmp>;
    cinfo->name_map = new std::map<wchar_t *, struct bu_vls *, WStrCmp>;
    cinfo->brlcad_names = new std::set<struct bu_vls *, StrCmp>;

}

extern "C" void
creo_conv_info_free(struct creo_conv_info *cinfo)
{

    std::set<wchar_t *, WStrCmp>::iterator d_it;
    for (d_it = cinfo->parts->begin(); d_it != cinfo->parts->end(); d_it++) {
	bu_free(*d_it, "free wchar str copy");
    }
    for (d_it = cinfo->assems->begin(); d_it != cinfo->assems->end(); d_it++) {
	bu_free(*d_it, "free wchar str copy");
    }

    std::set<struct bu_vls *, StrCmp>::iterator s_it;
    for (s_it = brlcad_names.begin(); s_it != brlcad_names.end(); s_it++) {
	struct bu_vls *v = *s_it;
	bu_vls_free(v);
	BU_PUT(v, struct bu_vls);
    }

    delete cinfo->parts;
    delete cinfo->assems;
    delete cinfo->assem_child_cnts;
    delete cinfo->empty; /* entries in empty were freed in parts and assems */
    delete cinfo->brlcad_names;
    delete cinfo->name_map; /* entries in name_map were freed in brlcad_names */

    if (cinfo->logger) fclose(cinfo->logger);
    wdb_close(cinfo->wdbp);

    /* Finally, clear the container */
    BU_PUT(cinfo, struct creo_conv_info);
}

extern "C" void
output_parts(struct creo_conv_info *cinfo)
{
    wchar_t wname[10000];
    char name[10000];
    std::set<wchar_t *, WStrCmp>::iterator d_it;
    for (d_it = cinfo->parts->begin(); d_it != cinfo->parts->end(); d_it++) {
	ProMdl m = ProMdlnameInit(*d_it, PRO_PART);
	if (ProMdlNameGet(m, wname) != PRO_TK_NO_ERROR) return;
	(void)ProWstringToString(name, wname);
	bu_log("Processing part %s\n", name);
	//int solid_part = output_part(cinfo, m);
	//if (!solid_part) cinfo->empty->insert(*d_it);
    }
}

extern "C" static ProError
assembly_check_empty( ProFeature *feat, ProError status, ProAppData app_data )
{
    ProError status;
    ProMdlType type;
    char wname[10000];
    int *has_shape = (int *)app_data;
    if (status = ProAsmcompMdlNameGet(feat, &type, wname) != PRO_TK_NO_ERROR ) return status;
    if (cinfo->empty->find(wname) == cinfo->empty->end()) (*has_shape) = 1;
    return PRO_TK_NO_ERROR;
}

/* run this only *after* output_parts - need that information */
extern "c" void
find_empty_assemblies(struct creo_conv_info *cinfo)
{
    int steady_state = 0;
    if (cinfo->empty->size() == 0) return;
    while (!steady_state) {
	std::set<wchar_t *, WStrCmp>::iterator d_it;
	steady_state = 1;
	for (d_it = cinfo->assems->begin(); d_it != cinfo->parts->end(); d_it++) {
	    /* TODO - for each assem, verify at least one child is non-empty.  If all
	     * children are empty, add to empty set and unset steady_state. */
	    int has_shape = 0;
	    ProSolidFeatVisit(ProMdlToPart(model), assembly_check_empty, (ProFeatureFilterAction)assembly_filter, (ProAppData)&has_shape);
	}
    }
}

extern "c" void
output_assems(struct creo_conv_info *cinfo)
{
    wchar_t wname[10000];
    char name[10000];
    std::set<wchar_t *, WStrCmp>::iterator d_it;
    for (d_it = cinfo->assems->begin(); d_it != cinfo->assems->end(); d_it++) {
	ProMdl parent = ProMdlnameInit(*d_it, PRO_ASSEM);
	if (ProMdlNameGet(parent, wname) != PRO_TK_NO_ERROR) return;
	(void)ProWstringToString(name, wname);
	bu_log("Processing assembly %s\n", name);
	//output_assembly(cinfo, parent);
    }
}

/* routine to output the top level object that is currently displayed in Pro/E */
extern "c" void
output_top_level_object(struct creo_conv_info *cinfo, promdl model, promdltype type )
{
    wchar_t wname[10000];
    char name[10000];
    wchar *wname_saved;

    /* get object name */
    if (ProMdlNameGet( model, wname ) != PRO_TK_NO_ERROR ) return;
    (void)ProWstringToString(name, wname);

    /* save name */
    wname_saved = (wchar *)bu_calloc(wcslen(wname)+1, sizeof(wchar), "CREO name");
    wcsncpy(wname_saved, wname, wsclen(wname)+1);

    /* output the object */
    if ( type == PRO_MDL_PART ) {
	/* tessellate part and output triangles */
	cinfo->parts->insert(wname_saved);
	output_parts(cinfo);
    } else if ( type == PRO_MDL_ASSEMBLY ) {
	/* visit all members of assembly */
	cinfo->assems->insert(wname_saved);
	ProSolidFeatVisit(ProMdlToPart(model), assembly_gather, (ProFeatureFilterAction)assembly_filter, (ProAppData)&adata);
	output_parts(cinfo);
	find_empty_assemblies(cinfo);
	output_assems(cinfo);
    } else {
	bu_log("Object %s is neither PART nor ASSEMBLY, skipping", name);
    }

    /* TODO - Make a final toplevel comb with the file name to hold the orientation matrix */
    /* xform to rotate the model into standard BRL-CAD orientation */
    /*0 0 1 0 1 0 0 0 0 1 0 0 0 0 0 1*/

    bu_free(wname_saved, "saved name");
}



extern "C" void
doit( char *dialog, char *compnent, ProAppData appdata )
{
    ProError status;
    ProMdl model;
    ProMdlType type;
    ProLine tmp_line;
    ProCharLine astr;
    ProFileName msgfil;
    int n_selected_names;
    char **selected_names;
    int ret_status=0;

    /* This replaces the global variables used in the original Pro/E converter */
    struct creo_conv_info *cinfo = new creo_conv_info;
    creo_conv_info_init(cinfo);

    ProStringToWstring( tmp_line, "Not processing" );
    status = ProUILabelTextSet( "creo_brl", "curr_proc", tmp_line );

    /********************/
    /*  Set up logging  */
    /********************/
    {
	char log_file[128];
	char logger_type_str[128];
	wchar_t *tmp_str;
	/* get the name of the log file */
	status = ProUIInputpanelValueGet( "creo_brl", "log_file", &tmp_str );
	if ( status != PRO_TK_NO_ERROR ) {
	    fprintf(stderr, "Failed to get log file name\n");
	    creo_conv_info_free(cinfo);
	    ProUIDialogDestroy("creo_brl");
	    return;
	}
	ProWstringToString(log_file, tmp_str);
	ProWstringFree(tmp_str);

	/* Set logger type */
	status = ProUIRadiogroupSelectednamesGet( "creo_brl", "log_file_type_rg", &n_selected_names, &selected_names );
	if (status != PRO_TK_NO_ERROR) {
	    fprintf( stderr, "Failed to get log file type\n" );
	    creo_conv_info_free(cinfo);
	    ProUIDialogDestroy("creo_brl");
	    return;
	}
	sprintf(logger_type_str,"%s", selected_names[0]);
	ProStringarrayFree(selected_names, n_selected_names);
	if (BU_STR_EQUAL("Failure", logger_type_str))
	    cinfo->logger_type = LOGGER_TYPE_FAILURE;
	else if (BU_STR_EQUAL("Success", logger_type_str))
	    cinfo->logger_type = LOGGER_TYPE_SUCCESS;
	else if (BU_STR_EQUAL("Failure/Success", logger_type_str))
	    cinfo->logger_type = LOGGER_TYPE_FAILURE_OR_SUCCESS;
	else
	    cinfo->logger_type = LOGGER_TYPE_ALL;

	/* open log file, if a name was provided */
	if (strlen(log_file) > 0) {
	    if ((cinfo->logger=fopen(log_file, "wb")) == NULL) {
		(void)ProMessageDisplay(msgfil, "USER_ERROR", "Cannot open log file");
		ProMessageClear();
		fprintf(stderr, "Cannot open log file\n");
		perror("\t");
		creo_conv_info_free(cinfo);
		ProUIDialogDestroy("creo_brl");
		return;
	    }
	} else {
	    cinfo->logger = (FILE *)NULL;
	}
    }

    /**************************/
    /* Set up the output file */
    /**************************/
    {
	/* Get string from dialog */
	char output_file[128];
	wchar_t *w_output_file;
	status = ProUIInputpanelValueGet("creo_brl", "output_file", &w_output_file);
	if ( status != PRO_TK_NO_ERROR ) {
	    creo_log(cinfo, MSG_FAIL, status, "Failed to get output file name\n");
	    creo_conv_info_free(cinfo);
	    ProUIDialogDestroy( "creo_brl" );
	    return;
	}
	ProWstringToString(output_file, w_output_file);
	ProWstringFree(w_output_file);

	/* Safety check - don't overwrite a pre-existing file */
	if (bu_file_exists(output_file, NULL)) {
	    /* TODO - wrap this up into a creo msg dialog function of some kind... */
	    struct bu_vls error_msg = BU_VLS_INIT_ZERO;
	    wchar_t w_error_msg[512];
	    bu_vls_printf( &error_msg, "Cannot create file %s - file already exists.\n", output_file );
	    ProStringToWstring( w_error_msg, bu_vls_addr( &error_msg ) );
	    status = ProUIDialogCreate( "creo_brl_gen_error", "creo_brl_gen_error" );
	    (void)ProUIPushbuttonActivateActionSet( "creo_brl_gen_error", "ok_button", kill_gen_error_dialog, NULL );
	    status = ProUITextareaValueSet( "creo_brl_gen_error", "error_message", w_error_msg );
	    status = ProUIDialogActivate( "creo_brl_gen_error", &dialog_return );
	    ProUIDialogDestroy( "creo_brl" );
	    return;
	}

	/* open output file */
	if ( (cinfo->dbip = db_create(output_file, 5) ) == DBI_NULL ) {
	    creo_log(cinfo, MSG_FAIL, status, "Cannot open output file.\n");
	    creo_conv_info_free(cinfo);
	    ProUIDialogDestroy( "creo_brl" );
	    return;
	}
	cinfo->wdbp = wdb_dbopen(cinfo->dbip, RT_WDB_TYPE_DB_DISK);
    }


    /* get starting ident */
    if ((status = ProUIInputpanelValueGet("creo_brl", "starting_ident", &tmp_str)) != PRO_TK_NO_ERROR) {
	creo_log(cinfo, MSG_FAIL, status, "Failed to get starting ident.\n");
	creo_conv_info_free(cinfo);
	ProUIDialogDestroy( "creo_brl" );
	return;
    } else {
	cinfo->reg_id = wstr_to_long(cinfo, &tmp_str);
	V_MAX(cinfo->reg_id, 1);
	ProWstringFree(tmp_str);
    }


    /* get max error */
    if ((status = ProUIInputpanelValueGet("creo_brl", "max_error", &tmp_str)) != PRO_TK_NO_ERROR) {
	creo_log(cinfo, MSG_FAIL, status, "Failed to get max tessellation error.\n");
	creo_conv_info_free(cinfo);
	ProUIDialogDestroy( "creo_brl" );
	return;
    } else {
	cinfo->max_error = wstr_to_double(&tmp_str);
	ProWstringFree(tmp_str);
    }


    /* get min error */
    if ((status = ProUIInputpanelValueGet("creo_brl", "min_error", &tmp_str)) != PRO_TK_NO_ERROR) {
	creo_log(cinfo, MSG_FAIL, status, "Failed to get min tessellation error.\n");
	creo_conv_info_free(cinfo);
	ProUIDialogDestroy( "creo_brl" );
	return;
    } else {
	cinfo->min_error = wstr_to_double(&tmp_str);
	ProWstringFree(tmp_str);
	V_MAX(cinfo->max_error, cinfo->min_error);
    }


    /* get the max angle control */
    if ((status = ProUIInputpanelValueGet("creo_brl", "max_angle_ctrl", &tmp_str)) != PRO_TK_NO_ERROR) {
	creo_log(cinfo, MSG_FAIL, status, "Failed to get max angle control.\n");
	creo_conv_info_free(cinfo);
	ProUIDialogDestroy( "creo_brl" );
	return;
    } else {
	cinfo->max_angle_cntrl = wstr_to_double(&tmp_str);
	ProWstringFree(tmp_str);
    }


    /* get the min angle control */
    if ((status = ProUIInputpanelValueGet("creo_brl", "min_angle_ctrl", &tmp_str)) != PRO_TK_NO_ERROR) {
	creo_log(cinfo, MSG_FAIL, status, "Failed to get min angle control.\n");
	creo_conv_info_free(cinfo);
	ProUIDialogDestroy( "creo_brl" );
	return;
    } else {
	cinfo->min_angle_cntrl = wstr_to_double(&tmp_str);
	ProWstringFree(&tmp_str);
	V_MAX(cinfo->max_angle_cntrl, cinfo->min_angle_cntrl);
    }

    /* get the max to min steps */
    if ((status = ProUIInputpanelValueGet("creo_brl", "isteps", &tmp_str)) != PRO_TK_NO_ERROR) {
	creo_log(cinfo, MSG_FAIL, status, "Failed to get max to min steps.\n");
	creo_conv_info_free(cinfo);
	ProUIDialogDestroy( "creo_brl" );
	return;
    } else {
	cinfo->max_to_min_steps = wstr_to_long(&tmp_str);
	ProWstringFree(tmp_str);
	if (cinfo->max_to_min_steps <= 0) {
	    cinfo->max_to_min_steps = 0;
	} else {
	    cinfo->error_increment = (ZERO((cinfo->max_error - cinfo->min_error))) ? 0 : cinfo->error_increment = (cinfo->max_error - cinfo->min_error) / (double)cinfo->max_to_min_steps;
	    cinfo->angle_increment = (ZERO((cinfo->max_angle_cntrl - cinfo->min_angle_cntrl))) ? 0 : (cinfo->max_angle_cntrl - cinfo->min_angle_cntrl) / (double)cinfo->max_to_min_steps;
	    if (ZERO(cinfo->error_increment) && ZERO(cinfo->angle_increment)) cinfo->max_to_min_steps = 0;
	}
    }

    /* check if user wants to do any CSG */
    if ((status = ProUICheckbuttonGetState("creo_brl", "facets_only", &cinfo->do_facets_only)) != PRO_TK_NO_ERROR) {
	creo_log(cinfo, MSG_FAIL, status, "Failed to get checkbutton setting (facetize only).\n");
	creo_conv_info_free(cinfo);
	ProUIDialogDestroy( "creo_brl" );
	return;
    }

    /* TODO - add a setting to use CREO part ID as basis for object ID, rather than querying properties */

    /* check if user wants to eliminate small features */
    if ((status = ProUICheckbuttonGetState("creo_brl", "elim_small", &cinfo->do_elims)) != PRO_TK_NO_ERROR ) {
	creo_log(cinfo, MSG_FAIL, status, "Failed to get checkbutton setting (eliminate small features).\n");
	creo_conv_info_free(cinfo);
	ProUIDialogDestroy( "creo_brl" );
	return;
    }

    /* check if user wants surface normals in the BOT's */
    if ((status = ProUICheckbuttonGetState("creo_brl", "get_normals", &cinfo->get_normals)) != PRO_TK_NO_ERROR ) {
	creo_log(cinfo, MSG_FAIL, status, "Failed to get checkbutton setting (extract surface normals).\n");
	creo_conv_info_free(cinfo);
	ProUIDialogDestroy( "creo_brl" );
	return;
    }

    if (cinfo->do_elims) {

	/* get the minimum hole diameter */
	if ((status = ProUIInputpanelValueGet("creo_brl", "min_hole", &tmp_str)) != PRO_TK_NO_ERROR) {
	    creo_log(cinfo, MSG_FAIL, status, "Failed to get minimum hole diameter.\n");
	    creo_conv_info_free(cinfo);
	    ProUIDialogDestroy( "creo_brl" );
	    return;
	} else {
	    cinfo->min_hole_diameter = wstr_to_double(&tmp_str);
	    V_MAX(cinfo->min_hole_diameter, 0.0);
	    ProWstringFree(tmp_str);
	}

	/* get the minimum chamfer dimension */
	if ((status = ProUIInputpanelValueGet("creo_brl", "min_chamfer", &tmp_str)) != PRO_TK_NO_ERROR) {
	    creo_log(cinfo, MSG_FAIL, status, "Failed to get minimum chamfer diameter.\n");
	    creo_conv_info_free(cinfo);
	    ProUIDialogDestroy( "creo_brl" );
	    return;
	} else {
	    cinfo->min_chamfer_dim = wstr_to_double(&tmp_str);
	    V_MAX(cinfo->min_chamfer_dim, 0.0);
	    ProWstringFree(tmp_str);
	}

	/* get the minimum round radius */
	if ((status = ProUIInputpanelValueGet("creo_brl", "min_round", &tmp_str)) != PRO_TK_NO_ERROR) {
	    creo_log(cinfo, MSG_FAIL, status, "Failed to get minimum round radius.\n");
	    creo_conv_info_free(cinfo);
	    ProUIDialogDestroy( "creo_brl" );
	    return;
	} else {
	    cinfo->min_round_radius = wstr_to_double(&tmp_str);
	    V_MAX(cinfo->min_round_radius, 0.0);
	    ProWstringFree(&tmp_str);
	}

    } else {
	cinfo->min_hole_diameter = 0.0;
	cinfo->min_round_radius = 0.0;
	cinfo->min_chamfer_dim = 0.0;
    }

    /* get the currently displayed model in Pro/E */
    if ((status = ProMdlCurrentGet(&model)) == PRO_TK_BAD_CONTEXT ) {
	creo_log(cinfo, MSG_FAIL, status, "No model is displayed!!\n");
	creo_conv_info_free(cinfo);
	ProUIDialogDestroy( "creo_brl" );
	return;
    }

    /* get its type */
    if (status = ProMdlTypeGet(model, &type) == PRO_TK_BAD_INPUTS) {
	creo_log(cinfo, MSG_FAIL, status, "Cannot get type of current model!!\n");
	creo_conv_info_free(cinfo);
	ProUIDialogDestroy( "creo_brl" );
	return;
    }

    /* can only do parts and assemblies, no drawings, etc. */
    if ( type != PRO_MDL_ASSEMBLY && type != PRO_MDL_PART ) {
	creo_log(cinfo, MSG_FAIL, status, "Current model is not a solid object.\n");
	creo_conv_info_free(cinfo);
	ProUIDialogDestroy( "creo_brl" );
	return;
    }

    /* TODO -verify this is working correctly */
    ProUnitsystem us;
    ProUnititem lmu;
    ProUnititem mmu;
    ProUnitConversion conv;
    ProMdlPrincipalunitsystemGet(model, &us);
    ProUnitsystemUnitGet(&us, PRO_UNITTYPE_LENGTH, &lmu);
    ProUnitInit(model, L"mm", &mmu);
    ProUnitConversionGet(&lmu, &conv, &mmu);
    cinfo->creo_to_brl_conv = conv.scale;

    /* adjust tolerance for Pro/E units */
    cinfo->local_tol = cinfo->tol_dist / cinfo->creo_to_brl_conv;
    cinfo->local_tol_sq = cinfo->local_tol * cinfo->local_tol;

    /* output the top level object
     * this will recurse through the entire model
     */
    output_top_level_object(cinfo, model, type );

    /* let user know we are done */
    ProStringToWstring( tmp_line, "Conversion complete" );
    ProUILabelTextSet( "creo_brl", "curr_proc", tmp_line );

    creo_conv_info_free(cinfo);
    return;
}



extern "C" void
elim_small_activate( char *dialog_name, char *button_name, ProAppData data )
{
    ProBoolean state;

    if (ProUICheckbuttonGetState( dialog_name, button_name, &state) != PRO_TK_NO_ERROR) {
	fprintf( stderr, "checkbutton activate routine: failed to get state\n" );
	return;
    }

    if (state) {
	if (ProUIInputpanelEditable(dialog_name, "min_hole") != PRO_TK_NO_ERROR) {
	    fprintf(stderr, "Failed to activate \"minimum hole diameter\"\n");
	    return;
	}
	if (ProUIInputpanelEditable(dialog_name, "min_chamfer") != PRO_TK_NO_ERROR) {
	    fprintf(stderr, "Failed to activate \"minimum chamfer dimension\"\n");
	    return;
	}
	if (ProUIInputpanelEditable(dialog_name, "min_round") != PRO_TK_NO_ERROR) {
	    fprintf(stderr, "Failed to activate \"minimum round radius\"\n");
	    return;
	}
    } else {
	if (ProUIInputpanelReadOnly(dialog_name, "min_hole") != PRO_TK_NO_ERROR) {
	    fprintf(stderr, "Failed to de-activate \"minimum hole diameter\"\n");
	    return;
	}
	if (ProUIInputpanelReadOnly(dialog_name, "min_chamfer") != PRO_TK_NO_ERROR) {
	    fprintf(stderr, "Failed to de-activate \"minimum chamfer dimension\"\n");
	    return;
	}
	if (ProUIInputpanelReadOnly(dialog_name, "min_round") != PRO_TK_NO_ERROR) {
	    fprintf(stderr, "Failed to de-activate \"minimum round radius\"\n");
	    return;
	}
    }
}

extern "C" void
do_quit(char *dialog, char *compnent, ProAppData appdata)
{
    ProUIDialogDestroy("creo_brl");
}

/* driver routine for converting CREO to BRL-CAD */
extern "C" int
creo_brl(uiCmdCmdId command, uiCmdValue *p_value, void *p_push_cmd_data)
{
    ProFileName msgfil;
    int ret_status=0;

    ProStringToWstring(msgfil, CREO_BRL_MSG_FILE);

    ProMessageDisplay(msgfil, "USER_INFO", "Launching creo_brl...");

    /* use UI dialog */
    if (ProUIDialogCreate("creo_brl", "creo_brl") != PRO_TK_NO_ERROR) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&vls, "Failed to create dialog box for creo-brl\n");
	ProMessageDisplay(msgfil, "USER_INFO", bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return 0;
    }

    if (ProUICheckbuttonActivateActionSet("creo_brl", "elim_small", elim_small_activate, NULL) != PRO_TK_NO_ERROR) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&vls, "Failed to set action for \"eliminate small features\" checkbutton\n");
	ProMessageDisplay(msgfil, "USER_INFO", bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return 0;
    }

    if (ProUIPushbuttonActivateActionSet("creo_brl", "doit", doit, NULL) != PRO_TK_NO_ERROR) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&vls, "Failed to set action for 'Go' button\n");
	ProMessageDisplay(msgfil, "USER_INFO", bu_vls_addr(&vls));
	ProUIDialogDestroy("creo_brl");
	bu_vls_free(&vls);
	return 0;
    }

    if (ProUIPushbuttonActivateActionSet("creo_brl", "quit", do_quit, NULL) != PRO_TK_NO_ERROR) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&vls, "Failed to set action for 'Quit' button\n");
	ProMessageDisplay(msgfil, "USER_INFO", bu_vls_addr(&vls));
	ProUIDialogDestroy("creo_brl");
	bu_vls_free(&vls);
	return 0;
    }

    if (ProUIDialogActivate("creo_brl", &ret_status) != PRO_TK_NO_ERROR) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&vls, "Error in creo-brl Dialog: dialog returned %d\n", ret_status);
	ProMessageDisplay(msgfil, "USER_INFO", bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }

    return 0;
}

/* this routine determines whether the "creo-brl" menu item in Pro/E
 * should be displayed as available or greyed out
 */
extern "C" static uiCmdAccessState
creo_brl_access( uiCmdAccessMode access_mode )
{
    /* doing the correct checks appears to be unreliable */
    return ACCESS_AVAILABLE;
}

extern "C" int user_initialize()
{
    ProError status;
    ProCharLine astr;
    ProFileName msgfil;
    int i;
    uiCmdCmdId cmd_id;
    wchar_t errbuf[80];

    ProStringToWstring(msgfil, CREO_BRL_MSG_FILE);

    /* Pro/E says always check the size of w_char */
    status = ProWcharSizeVerify (sizeof (wchar_t), &i);
    if ( status != PRO_TK_NO_ERROR || (i != sizeof (wchar_t)) ) {
	sprintf(astr, "ERROR wchar_t Incorrect size (%d). Should be: %d", sizeof(wchar_t), i );
	status = ProMessageDisplay(msgfil, "USER_ERROR", astr);
	printf("%s\n", astr);
	ProStringToWstring(errbuf, astr);
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	return -1;
    }

    /* add a command that calls our creo-brl routine */
    status = ProCmdActionAdd( "CREO-BRL", (uiCmdCmdActFn)creo_brl, uiProe2ndImmediate, creo_brl_access, PRO_B_FALSE, PRO_B_FALSE, &cmd_id );
    if ( status != PRO_TK_NO_ERROR ) {
	sprintf( astr, "Failed to add creo-brl action" );
	fprintf( stderr, "%s\n", astr);
	ProMessageDisplay(msgfil, "USER_ERROR", astr);
	ProStringToWstring(errbuf, astr);
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	return -1;
    }

    /* add a menu item that runs the new command */
    status = ProMenubarmenuPushbuttonAdd( "File", "CREO-BRL", "CREO-BRL", "CREO-BRL-HELP", "File.psh_exit", PRO_B_FALSE, cmd_id, msgfil );
    if ( status != PRO_TK_NO_ERROR ) {
	sprintf( astr, "Failed to add creo-brl menu button" );
	fprintf( stderr, "%s\n", astr);
	ProMessageDisplay(msgfil, "USER_ERROR", astr);
	ProStringToWstring(errbuf, astr);
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	return -1;
    }

    ShowMsg();

    /* let user know we are here */
    ProMessageDisplay( msgfil, "OK" );
    (void)ProWindowRefresh( PRO_VALUE_UNUSED );

    return 0;
}

extern "C" void user_terminate()
{
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
