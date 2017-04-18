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
    /* Output file */
    BU_GET(cinfo->output_file, struct bu_vls);
    bu_vls_init(cinfo->output_file);

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
    cinfo->empty = new std::set<wchar_t *, WStrCmp>;
    cinfo->name_map = new std::map<wchar_t *, struct bu_vls *, WStrCmp>;
    cinfo->brlcad_names = new std::set<struct bu_vls *, StrCmp>;
    cinfo->model_parameters = new std::vector<char *>;
    cinfo->attrs = new std::vector<char *>;

}

extern "C" void
creo_conv_info_free(struct creo_conv_info *cinfo)
{

    bu_vls_free(cinfo->output_file);
    BU_PUT(cinfo->output_file, struct bu_vls);

    std::set<wchar_t *, WStrCmp>::iterator d_it;
    for (d_it = cinfo->parts->begin(); d_it != cinfo->parts->end(); d_it++) {
	bu_free(*d_it, "free wchar str copy");
    }
    for (d_it = cinfo->assems->begin(); d_it != cinfo->assems->end(); d_it++) {
	bu_free(*d_it, "free wchar str copy");
    }

    std::set<struct bu_vls *, StrCmp>::iterator s_it;
    for (s_it = cinfo->brlcad_names->begin(); s_it != cinfo->brlcad_names->end(); s_it++) {
	struct bu_vls *v = *s_it;
	bu_vls_free(v);
	BU_PUT(v, struct bu_vls);
    }


    for (unsigned int i = 0; i < cinfo->model_parameters->size(); i++) {
	char *str = cinfo->model_parameters->at(i);
	bu_free(str, "free model param string");
    }


    for (unsigned int i = 0; i < cinfo->attrs->size(); i++) {
	char *str = cinfo->attrs->at(i);
	bu_free(str, "free attrs string");
    }

    delete cinfo->parts;
    delete cinfo->assems;
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
    wchar_t wname[CREO_NAME_MAX];
    char name[CREO_NAME_MAX];
    std::set<wchar_t *, WStrCmp>::iterator d_it;
    for (d_it = cinfo->parts->begin(); d_it != cinfo->parts->end(); d_it++) {
	ProMdl m;
	if (ProMdlnameInit(*d_it, PRO_MDLFILE_PART, &m) != PRO_TK_NO_ERROR) return;
	if (ProMdlNameGet(m, wname) != PRO_TK_NO_ERROR) return;
	(void)ProWstringToString(name, wname);
	creo_log(cinfo, MSG_DEBUG, PRO_TK_NO_ERROR, "Processing part %s\n", name);
	if (output_part(cinfo, m) == PRO_TK_NOT_EXIST) cinfo->empty->insert(*d_it);
    }
}

extern "C" void
output_assems(struct creo_conv_info *cinfo)
{
    wchar_t wname[CREO_NAME_MAX];
    char name[CREO_NAME_MAX];
    std::set<wchar_t *, WStrCmp>::iterator d_it;
    for (d_it = cinfo->assems->begin(); d_it != cinfo->assems->end(); d_it++) {
	ProMdl parent;
	ProMdlnameInit(*d_it, PRO_MDLFILE_ASSEMBLY, &parent);
	if (ProMdlNameGet(parent, wname) != PRO_TK_NO_ERROR) return;
	(void)ProWstringToString(name, wname);
	creo_log(cinfo, MSG_DEBUG, PRO_TK_NO_ERROR, "Processing assembly %s\n", name);
	output_assembly(cinfo, parent);
    }
}


/* Logic that builds up the sets of assemblies and parts.  Doing a feature
 * visit for all top level objects will result in a recursive walk of the
 * hierarchy that adds all active objects into one of the converter lists.
 *
 * The "app_data" pointer holds the creo_conv_info container. 
 */
extern "C" ProError
objects_gather( ProFeature *feat, ProError UNUSED(status), ProAppData app_data )
{
    ProError loc_status;
    ProMdl model;
    ProMdlType type;
    wchar_t wname[CREO_NAME_MAX];
    char name[CREO_NAME_MAX];
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
		return ProSolidFeatVisit(ProMdlToPart(model), objects_gather, (ProFeatureFilterAction)component_filter, app_data);
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

/*
 * Routine to output the top level object that is currently displayed in CREO.
 * This is the real beginning of the processing code - doit collects user
 * settings and calls this function. */
extern "C" void
output_top_level_object(struct creo_conv_info *cinfo, ProMdl model, ProMdlType type )
{
    wchar_t wname[CREO_NAME_MAX];
    char name[CREO_NAME_MAX];
    wchar_t *wname_saved;

    /* get object name */
    if (ProMdlNameGet( model, wname ) != PRO_TK_NO_ERROR ) return;
    (void)ProWstringToString(name, wname);

    /* save name */
    wname_saved = (wchar_t *)bu_calloc(wcslen(wname)+1, sizeof(wchar_t), "CREO name");
    wcsncpy(wname_saved, wname, wcslen(wname)+1);

    /* There are two possibilities - either we have a hierarchy, in which case we
     * need to walk it and collect the objects to process, or we have a single part
     * which we can process directly. */
    if ( type == PRO_MDL_PART ) {
	/* One part only */
	cinfo->parts->insert(wname_saved);
	output_parts(cinfo);
    } else if ( type == PRO_MDL_ASSEMBLY ) {
	/* Walk the hierarchy and process all necessary assemblies and parts */
	cinfo->assems->insert(wname_saved);
	ProSolidFeatVisit(ProMdlToPart(model), objects_gather, (ProFeatureFilterAction)component_filter, (ProAppData)cinfo);
	output_parts(cinfo);
	find_empty_assemblies(cinfo);
	output_assems(cinfo);
    } else {
	bu_log("Object %s is neither PART nor ASSEMBLY, skipping", name);
    }

    /* Make a final toplevel comb based on the file name to hold the orientation matrix */
    struct bu_vls *comb_name;
    struct wmember wcomb;
    BU_LIST_INIT(&wcomb.l);
    mat_t m;
    bn_decode_mat(m, "0 0 1 0 1 0 0 0 0 1 0 0 0 0 0 1");
    comb_name = get_brlcad_name(cinfo, wname, type, NULL);
    (void)mk_addmember(bu_vls_addr(comb_name), &(wcomb.l), m, WMOP_UNION);
    mk_lcomb(cinfo->wdbp, bu_vls_addr(cinfo->output_file), &wcomb, 0, NULL, NULL, NULL, 0);
}



extern "C" void
doit(char *UNUSED(dialog), char *UNUSED(compnent), ProAppData UNUSED(appdata))
{
    ProError status;
    ProMdl model;
    ProMdlType type;
    ProLine tmp_line = NULL;
    ProFileName msgfil = NULL;
    wchar_t *tmp_str;
    int n_selected_names;
    char **selected_names;

    /* This replaces the global variables used in the original Pro/E converter */
    struct creo_conv_info *cinfo = new creo_conv_info;
    creo_conv_info_init(cinfo);

    ProStringToWstring(tmp_line, "Not processing" );
    status = ProUILabelTextSet( "creo_brl", "curr_proc", tmp_line );

    /********************/
    /*  Set up logging  */
    /********************/
    {
	char log_file[128];
	char logger_type_str[128];

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
	    int dialog_return;
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

	/* Store the filename for later use */
	bu_vls_sprintf(cinfo->output_file, "%s", output_file);
    }

    /***********************************************************************************/
    /* Read a user supplied list of model parameters in which to look for naming info. */
    /***********************************************************************************/
    {
	/* Get string from dialog */
	char param_file[128];
	wchar_t *w_param_file;
	status = ProUIInputpanelValueGet("creo_brl", "name_file", &w_param_file);
	if ( status != PRO_TK_NO_ERROR ) {
	    creo_log(cinfo, MSG_FAIL, status, "Failed to get name of model parameter specification file.\n");
	    creo_conv_info_free(cinfo);
	    ProUIDialogDestroy( "creo_brl" );
	    return;
	}
	ProWstringToString(param_file, w_param_file);
	ProWstringFree(w_param_file);

	if (strlen(param_file) > 0) {
	    /* Parse the file contents into a list of parameter keys */
	    std::ifstream pfile(param_file);
	    std::string line;
	    if (!pfile) {
		creo_log(cinfo, MSG_FAIL, status, "Cannot read parameter keys file.\n");
		creo_conv_info_free(cinfo);
		ProUIDialogDestroy( "creo_brl" );
		return;
	    }
	    while (std::getline(pfile, line)) {
		std::string pkey;
		std::istringstream ls(line);
		while (std::getline(ls, pkey, ',')) {
		    /* Scrub leading and trailing whitespace */
		    size_t startpos = pkey.find_first_not_of(" \t\n\v\f\r");
		    if (std::string::npos != startpos) {
			pkey = pkey.substr(startpos);
		    }
		    size_t endpos = pkey.find_last_not_of(" \t\n\v\f\r");
		    if (std::string::npos != endpos) {
			pkey = pkey.substr(0 ,endpos+1);
		    }
		    if (pkey.length() > 0) {
			creo_log(cinfo, MSG_DEBUG, PRO_TK_NO_ERROR, "Found model parameter naming key: %s.\n", pkey.c_str());
			cinfo->model_parameters->push_back(bu_strdup(pkey.c_str()));
		    }
		}
	    }
	}
    }

    /***********************************************************************************/
    /* Read a user supplied list of model attributes to convert over to the .g file. */
    /***********************************************************************************/
    {
	/* Get string from dialog */
	char attr_file[128];
	wchar_t *w_attr_file;
	status = ProUIInputpanelValueGet("creo_brl", "attr_file", &w_attr_file);
	if ( status != PRO_TK_NO_ERROR ) {
	    creo_log(cinfo, MSG_FAIL, status, "Failed to get name of attribute list file.\n");
	    creo_conv_info_free(cinfo);
	    ProUIDialogDestroy( "creo_brl" );
	    return;
	}
	ProWstringToString(attr_file, w_attr_file);
	ProWstringFree(w_attr_file);

	if (strlen(attr_file) > 0) {
	    /* Parse the file contents into a list of parameter keys */
	    std::ifstream pfile(attr_file);
	    std::string line;
	    if (!pfile) {
		creo_log(cinfo, MSG_FAIL, status, "Cannot read attribute list file.\n");
		creo_conv_info_free(cinfo);
		ProUIDialogDestroy( "creo_brl" );
		return;
	    }
	    while (std::getline(pfile, line)) {
		std::string pkey;
		std::istringstream ls(line);
		while (std::getline(ls, pkey, ',')) {
		    /* Scrub leading and trailing whitespace */
		    size_t startpos = pkey.find_first_not_of(" \t\n\v\f\r");
		    if (std::string::npos != startpos) {
			pkey = pkey.substr(startpos);
		    }
		    size_t endpos = pkey.find_last_not_of(" \t\n\v\f\r");
		    if (std::string::npos != endpos) {
			pkey = pkey.substr(0 ,endpos+1);
		    }
		    if (pkey.length() > 0) {
			creo_log(cinfo, MSG_DEBUG, PRO_TK_NO_ERROR, "Found attribute key: %s.\n", pkey.c_str());
			cinfo->attrs->push_back(bu_strdup(pkey.c_str()));
		    }
		}
	    }
	}
    }

    /* get starting ident */
    if ((status = ProUIInputpanelValueGet("creo_brl", "starting_ident", &tmp_str)) != PRO_TK_NO_ERROR) {
	creo_log(cinfo, MSG_FAIL, status, "Failed to get starting ident.\n");
	creo_conv_info_free(cinfo);
	ProUIDialogDestroy( "creo_brl" );
	return;
    } else {
	cinfo->reg_id = wstr_to_long(cinfo, tmp_str);
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
	cinfo->max_error = wstr_to_double(cinfo, tmp_str);
	ProWstringFree(tmp_str);
    }


    /* get min error */
    if ((status = ProUIInputpanelValueGet("creo_brl", "min_error", &tmp_str)) != PRO_TK_NO_ERROR) {
	creo_log(cinfo, MSG_FAIL, status, "Failed to get min tessellation error.\n");
	creo_conv_info_free(cinfo);
	ProUIDialogDestroy( "creo_brl" );
	return;
    } else {
	cinfo->min_error = wstr_to_double(cinfo, tmp_str);
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
	cinfo->max_angle_cntrl = wstr_to_double(cinfo, tmp_str);
	ProWstringFree(tmp_str);
    }


    /* get the min angle control */
    if ((status = ProUIInputpanelValueGet("creo_brl", "min_angle_ctrl", &tmp_str)) != PRO_TK_NO_ERROR) {
	creo_log(cinfo, MSG_FAIL, status, "Failed to get min angle control.\n");
	creo_conv_info_free(cinfo);
	ProUIDialogDestroy( "creo_brl" );
	return;
    } else {
	cinfo->min_angle_cntrl = wstr_to_double(cinfo, tmp_str);
	ProWstringFree(tmp_str);
	V_MAX(cinfo->max_angle_cntrl, cinfo->min_angle_cntrl);
    }

    /* get the max to min steps */
    if ((status = ProUIInputpanelValueGet("creo_brl", "isteps", &tmp_str)) != PRO_TK_NO_ERROR) {
	creo_log(cinfo, MSG_FAIL, status, "Failed to get max to min steps.\n");
	creo_conv_info_free(cinfo);
	ProUIDialogDestroy( "creo_brl" );
	return;
    } else {
	cinfo->max_to_min_steps = wstr_to_long(cinfo, tmp_str);
	ProWstringFree(tmp_str);
	if (cinfo->max_to_min_steps <= 0) {
	    cinfo->max_to_min_steps = 0;
	} else {
	    cinfo->error_increment = (ZERO((cinfo->max_error - cinfo->min_error))) ? 0 : (cinfo->max_error - cinfo->min_error) / (double)cinfo->max_to_min_steps;
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
	    cinfo->min_hole_diameter = wstr_to_double(cinfo, tmp_str);
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
	    cinfo->min_chamfer_dim = wstr_to_double(cinfo, tmp_str);
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
	    cinfo->min_round_radius = wstr_to_double(cinfo, tmp_str);
	    V_MAX(cinfo->min_round_radius, 0.0);
	    ProWstringFree(tmp_str);
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
    if ((status = ProMdlTypeGet(model, &type)) == PRO_TK_BAD_INPUTS) {
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
elim_small_activate(char *dialog_name, char *button_name, ProAppData UNUSED(data))
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
do_quit(char *UNUSED(dialog), char *UNUSED(compnent), ProAppData UNUSED(appdata))
{
    ProUIDialogDestroy("creo_brl");
}

/* driver routine for converting CREO to BRL-CAD */
extern "C" int
creo_brl(uiCmdCmdId UNUSED(command), uiCmdValue *UNUSED(p_value), void *UNUSED(p_push_cmd_data))
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    ProFileName msgfil = NULL;
    int destroy_dialog = 0;
    ProError ret_status;

    ProStringToWstring(msgfil, CREO_BRL_MSG_FILE);
    ProMessageDisplay(msgfil, "USER_INFO", "Launching creo_brl...");

    /* use UI dialog */
    if (ProUIDialogCreate("creo_brl", "creo_brl") != PRO_TK_NO_ERROR) {
	bu_vls_printf(&vls, "Failed to create dialog box for creo-brl\n");
	goto print_msg;
    }

    if (ProUICheckbuttonActivateActionSet("creo_brl", "elim_small", elim_small_activate, NULL) != PRO_TK_NO_ERROR) {
	bu_vls_printf(&vls, "Failed to set action for \"eliminate small features\" checkbutton\n");
	goto print_msg;
    }

    if (ProUIPushbuttonActivateActionSet("creo_brl", "doit", doit, NULL) != PRO_TK_NO_ERROR) {
	bu_vls_printf(&vls, "Failed to set action for 'Go' button\n");
	destroy_dialog = 1;
	goto print_msg;
    }

    if (ProUIPushbuttonActivateActionSet("creo_brl", "quit", do_quit, NULL) != PRO_TK_NO_ERROR) {
	bu_vls_printf(&vls, "Failed to set action for 'Quit' button\n");
	destroy_dialog = 1;
	goto print_msg;
    }

    if (ProUIDialogActivate("creo_brl", &ret_status) != PRO_TK_NO_ERROR) {
	bu_vls_printf(&vls, "Error in creo-brl Dialog: dialog returned %d\n", ret_status);
    }

print_msg:
    ProMessageDisplay(msgfil, "USER_INFO", bu_vls_addr(&vls));
    if (destroy_dialog) ProUIDialogDestroy("creo_brl");
    bu_vls_free(&vls);
    return 0;
}

/* this routine determines whether the "creo-brl" menu item in Pro/E
 * should be displayed as available or greyed out
 */
extern "C" uiCmdAccessState
creo_brl_access(uiCmdAccessMode UNUSED(access_mode))
{
    /* doing the correct checks appears to be unreliable */
    return ACCESS_AVAILABLE;
}

/********************************************************************************
 * IMPORTANT - the names of the next two functions - user_initialize and
 * user_terminate - are dictated by the CREO API.  These are the hooks that tie
 * the rest of the code into the CREO system.  Both are *required* to be
 * present, even if the user_terminate function doesn't do any actual work.
 ********************************************************************************/

extern "C" int user_initialize()
{
    ProError status;
    ProCharLine astr = NULL;
    ProFileName msgfil = NULL;
    int i;
    uiCmdCmdId cmd_id;
    wchar_t errbuf[80];

    ProStringToWstring(msgfil, CREO_BRL_MSG_FILE);

    /* Pro/E says always check the size of w_char */
    status = ProWcharSizeVerify (sizeof (wchar_t), &i);
    if ( status != PRO_TK_NO_ERROR || (i != sizeof (wchar_t)) ) {
	sprintf(astr, "ERROR wchar_t Incorrect size (%d). Should be: %d", (int)sizeof(wchar_t), i );
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

    /* TODO - this is a test, but may want to convert it into a "loaded successfully" dialog */
    PopupMsg("Hello world", "Hello world");

    /* let user know we are here */
    //ProMessageDisplay( msgfil, "OK" );
    //(void)ProWindowRefresh( PRO_VALUE_UNUSED );

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
