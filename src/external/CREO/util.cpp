/*                    U T I L . C P P
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
/** @file util.cpp
 *
 */

#include "common.h"
#include <regex.h>
#include "creo-brl.h"

int creo_logging_opts(void *data, void *vstr){
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct creo_conv_info *cinfo = (struct creo_conv_info *)data;
    const char *str = (const char *)vstr;

    if (cinfo->logger_type == LOGGER_TYPE_NONE) return 0;

    switch (cinfo->curr_msg_type) {
	case MSG_FAIL:
	    bu_vls_sprintf(&msg, "FAILURE: %s", str);
	    break;
	case MSG_OK:
	    bu_vls_sprintf(&msg, "SUCCESS: %s", str);
	    break;
	case MSG_DEBUG:
	    bu_vls_sprintf(&msg, "DEBUG:   %s", str);
	    break;
	default:
	    bu_vls_sprintf(&msg, "%s", str);
	    break;
    }

    if ( (cinfo->curr_msg_type == MSG_FAIL && cinfo->logger_type != LOGGER_TYPE_SUCCESS) ||
	    (cinfo->curr_msg_type == MSG_OK && cinfo->logger_type != LOGGER_TYPE_FAILURE) ||
	    (cinfo->curr_msg_type != MSG_DEBUG && cinfo->logger_type == LOGGER_TYPE_FAILURE_OR_SUCCESS) ||
	    (cinfo->logger_type == LOGGER_TYPE_ALL)
       ) {
	if (cinfo->logger) fprintf(cinfo->logger, "%s", bu_vls_addr(&msg));
	if (cinfo->print_to_console) {
	    if (LIKELY(stderr != NULL)) {
		fprintf(stderr, "%s", bu_vls_addr(&msg));
		fflush(stderr);
	    }
	}
    }
    bu_vls_free(&msg);
    return 0;
}

extern "C" ProError
creo_log(struct creo_conv_info *cinfo, int msg_type, ProError status, const char *fmt, ...) {
    /* TODO - need creo specific semaphore lock for this if it's going to be used
     * in multi-threading situations... - probably can't use libbu's logging safely */

    /* Can't do nested variable argument functions, so printf the message here */
    va_list ap;
    char msg[100000];
    va_start(ap, fmt);
    vsprintf(msg, fmt, ap);
    va_end(ap);

    /* Set the type and hooks, then call libbu */
    cinfo->curr_msg_type = msg_type;
    bu_log_add_hook(creo_logging_opts, (void *)cinfo);
    bu_log("%s", msg);
    bu_log_delete_hook(creo_logging_opts, (void *)cinfo);

    return status;
}


extern "C" long int
wstr_to_long(struct creo_conv_info *cinfo, wchar_t *tmp_str)
{
    long int ret;
    char *endptr = NULL;
    ProCharLine astr;
    ProWstringToString(astr, tmp_str);
    errno = 0;
    ret = strtol(astr, &endptr, 0);
    if (endptr != NULL && strlen(endptr) > 0) {
        /* Had some invalid character in the input, fail */
        creo_log(cinfo, MSG_FAIL, PRO_TK_NO_ERROR, "Invalid string specifier for int: %s\n", astr);
        return -1;
    }
    return ret;
}

extern "C" double
wstr_to_double(struct creo_conv_info *cinfo, wchar_t *tmp_str)
{
    double ret;
    char *endptr = NULL;
    ProCharLine astr;
    ProWstringToString(astr, tmp_str);
    errno = 0;
    ret = strtod(astr, &endptr);
    if (endptr != NULL && strlen(endptr) > 0) {
        /* Had some invalid character in the input, fail */
        creo_log(cinfo, MSG_FAIL, PRO_TK_NO_ERROR, "Invalid string specifier for double: %s\n", astr);
        return -1.0;
    }
    return ret;
}

extern "C" void
kill_error_dialog( char *dialog, char *component, ProAppData appdata )
{
    (void)ProUIDialogDestroy( "creo_brl_error" );
}

extern "C" void
kill_gen_error_dialog( char *dialog, char *component, ProAppData appdata )
{
    (void)ProUIDialogDestroy( "creo_brl_gen_error" );
}


extern "C" void
lower_case( char *name )
{
    unsigned char *c;

    c = (unsigned char *)name;
    while ( *c ) {
	(*c) = tolower( *c );
	c++;
    }
}

extern "C" void
make_legal( char *name )
{
    unsigned char *c;

    c = (unsigned char *)name;
    while ( *c ) {
	if ( *c <= ' ' || *c == '/' || *c == '[' || *c == ']' ) {
	    *c = '_';
	} else if ( *c > '~' ) {
	    *c = '_';
	}
	c++;
    }
}

struct pparam_data {
    struct creo_conv_info *cinfo;
    char *key;
    char *val;
}

extern "C" ProError getkey(ProParameter *param, ProError status, ProAppData app_data)
{
    ProError lstatus;
    char pname[100000];
    char val[100000];
    wchar_t wval[10000];
    ProParamvalue pval;
    ProParamvalueType ptype;
    regex_t reg;
    struct pparam_data *pdata = (struct pparam_data *)app_data;
    struct creo_conv_info *cinfo = pdata->cinfo;
    if (pdata->val) return PRO_TK_NO_ERROR;
    ProWstringToString(pname, param->id);
    (void)regcomp(&reg, pdata->key, REG_NOSUB|REG_EXTENDED);
    if (!(regexec(&reg, db_path_to_string(db_node->path), 0, NULL, 0))) {
	regfree(&reg);
       	return PRO_TK_CONTINUE;
    }
    regfree(&reg);
    if (ProParameterValueGet(param, &pval) != PRO_TK_NO_ERROR) return PRO_TK_CONTINUE;
    if (ProParamvalueTypeGet(&pval, &ptype) != PRO_TK_NO_ERROR) return PRO_TK_CONTINUE;
    if (ptype == PRO_PARAM_STRING) {
	if (ProParamvalueValueGet(&pval, ptype, wval) != PRO_TK_NO_ERROR) return PRO_TK_CONTINUE;
	ProWstringToString(val, wval);
	if (strlen(val) > 0) {
	    pdata->val = bu_strdup(val);
	}
    }
    return PRO_TK_NO_ERROR;
}


extern "C" char *
creo_param_name(struct creo_conv_info *cinfo, wchar_t *creo_name, ProType type)
{
    struct pparam_data pdata;
    pdata.cinfo = cinfo;
    pdata.val = NULL;

    ProModelitem itm;
    ProMdl model;
    if (ProMdlnameInit(creo_name, type, &m) != PRO_TK_NO_ERROR) return NULL;
    if (ProMdlToModelitem(model, &itm) != PRO_TK_NO_ERROR) return NULL;
    for (int i = 0; int i < cinfo->model_parameters->size(); i++) {
	if (!pdata.val) {
	    pdata.key = cinfo->model_parameters[i];
	    ProParameterVisit(&itm,  NULL, getkey, (ProAppData)&pdata);
	}
    }
    return pdata.val;
}


/* create a unique BRL-CAD object name from a possibly illegal name */
extern "C" struct bu_vls *
get_brlcad_name(struct creo_conv_info *cinfo, wchar_t *name, ProType type)
{
    struct bu_vls gname_root = BU_VLS_INIT_ZERO;
    struct bu_vls *gname;
    char *param_name = NULL;
    long count = 0;
    long have_name = 0;

    if ( cinfo->logger_type == LOGGER_TYPE_ALL ) {
	fprintf( cinfo->logger, "create_unique_name( %s )\n", name );
    }
    BU_GET(gname, struct bu_vls);
    bu_vls_init(gname);

    /* First try the parameters, if the user specified any */
    param_name = creo_param_name(cinfo, name, type);

    /* Fall back on the CREO name, if we don't get a param name */
    if (!param_name) {
	char val[100000];
	ProWstringToString(val, name);
	bu_vls_sprintf(&gname_root, "%s", val);
    } else {
	bu_vls_sprintf(&gname_root, "%s", param_name);
	bu_free(param_name, "free original param name");
    }

    /* scrub */
    lower_case(bu_vls_addr(&gname_root));
    make_legal(bu_vls_addr(&gname_root));
    bu_vls_sprintf(gname, "%s", bu_vls_addr(&gname_root));

    /* Provide a suffix for solids */
    if (type != PRO_MDL_ASSEMBLY) {bu_vls_printf(gname, ".s");}

    /* create a unique name */
    if (cinfo->brlcad_names->find(gname) != cinfo->brlcad_names->end()) {
	bu_vls_sprintf(gname, "%s-1", bu_vls_addr(&gname_root));
	if (type != PRO_MDL_ASSEMBLY) {bu_vls_printf(gname, ".s");}
	while (cinfo->brlcad_names->find(gname) != cinfo->brlcad_names->end()) {
	    (void)bu_namegen(gname, NULL, NULL);
	    count++;
	    if ( cinfo->logger_type == LOGGER_TYPE_ALL ) {
		fprintf( cinfo->logger, "\tTrying %s\n", bu_vls_addr(tmp_name) );
	    }
	    if (count == LONG_MAX) {
		bu_vls_free(gname);
		BU_PUT(gname, struct bu_vls);
		return NULL;
	    }
	}
    }
    cinfo->brlcad_names->insert(tmp_name);

    if ( cinfo->logger_type == LOGGER_TYPE_ALL ) {
	fprintf( cinfo->logger, "\tnew name for %s is %s\n", name, bu_vls_addr(tmp_name) );
    }

    return gname;
}

/* Test function */
ProError ShowMsg()
{
    ProUIMessageButton* button;
    ProUIMessageButton bresult;
    ProArrayAlloc(1, sizeof(ProUIMessageButton), 1, (ProArray*)&button);
    button[0] = PRO_UI_MESSAGE_OK;
    ProUIMessageDialogDisplay(PROUIMESSAGE_INFO, L"Hello World", L"Hello world!", button, PRO_UI_MESSAGE_OK, &bresult);
    ProArrayFree((ProArray*)&button);
    return PRO_TK_NO_ERROR;
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
