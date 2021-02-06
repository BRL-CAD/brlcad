/*                    U T I L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2017-2021 United States Government as represented by
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

extern "C" void
creo_log(struct creo_conv_info *cinfo, int msg_type, const char *fmt, ...) {
    /* NOTE - need creo specific semaphore lock for this if it's going to be used
     * in multi-threading situations... - probably can't use libbu's logging safely */

    /* CREO gui */
    ProFileName msgfil = {'\0'};
    ProStringToWstring(msgfil, CREO_BRL_MSG_FILE);

    /* Can't do nested variable argument functions, so printf the message here */
    va_list ap;
    char msg[CREO_MSG_MAX];
    va_start(ap, fmt);
    vsprintf(msg, fmt, ap);
    va_end(ap);

    if (msg_type == MSG_STATUS) {
	ProMessageClear();
	ProMessageDisplay(msgfil, "USER_INFO", msg);
	return;
    }

    /* if we're logging and not reporting status, do it */
    if (cinfo && cinfo->logger && msg_type != MSG_STATUS) {
	cinfo->curr_msg_type = msg_type;
	struct bu_vls vmsg = BU_VLS_INIT_ZERO;

	if (cinfo->logger_type == LOGGER_TYPE_NONE)
	    return;

	switch (cinfo->curr_msg_type) {
	    case MSG_FAIL:
		bu_vls_sprintf(&vmsg, "FAILURE!!!: %s", msg);
		break;
	    case MSG_OK:
		bu_vls_sprintf(&vmsg, "SUCCESS: %s", msg);
		break;
	    case MSG_DEBUG:
		bu_vls_sprintf(&vmsg, "DEBUG:   %s", msg);
		break;
	    default:
		bu_vls_sprintf(&vmsg, "%s", msg);
		break;
	}

	if ( (cinfo->curr_msg_type == MSG_FAIL && cinfo->logger_type != LOGGER_TYPE_SUCCESS) ||
		(cinfo->curr_msg_type == MSG_OK && cinfo->logger_type != LOGGER_TYPE_FAILURE) ||
		(cinfo->curr_msg_type != MSG_DEBUG && cinfo->logger_type == LOGGER_TYPE_FAILURE_OR_SUCCESS) ||
		(cinfo->logger_type == LOGGER_TYPE_ALL)
	   ) {
	    fprintf(cinfo->logger, "%s", bu_vls_addr(&vmsg));
	    fflush(cinfo->logger);
	}
	bu_vls_free(&vmsg);
    }
}


extern "C" long int
wstr_to_long(struct creo_conv_info *cinfo, wchar_t *tmp_str)
{
    long int ret;
    char *endptr = NULL;
    char astr[CREO_MSG_MAX];
    ProWstringToString(astr, tmp_str);
    errno = 0;
    ret = strtol(astr, &endptr, 0);
    if (endptr != NULL && strlen(endptr) > 0) {
	/* Had some invalid character in the input, fail */
	creo_log(cinfo, MSG_DEBUG, "Invalid string specifier for int: %s\n", astr);
	return -1;
    }
    return ret;
}

extern "C" double
wstr_to_double(struct creo_conv_info *cinfo, wchar_t *tmp_str)
{
    double ret;
    char *endptr = NULL;
    char astr[CREO_MSG_MAX];
    ProWstringToString(astr, tmp_str);
    errno = 0;
    ret = strtod(astr, &endptr);
    if (endptr != NULL && strlen(endptr) > 0) {
	/* Had some invalid character in the input, fail */
	creo_log(cinfo, MSG_DEBUG, "Invalid string specifier for double: %s\n", astr);
	return -1.0;
    }
    return ret;
}

extern "C" ProError
creo_model_units(double *scale, ProMdl mdl)
{
    ProError status = PRO_TK_GENERAL_ERROR;
    ProUnitsystem us;
    ProUnititem lmu;
    ProUnititem mmu;
    ProUnitConversion conv;

    if (!scale)
	return status;

    status = ProMdlPrincipalunitsystemGet(mdl, &us);
    if (status != PRO_TK_NO_ERROR)
	return status;

    status = ProUnitsystemUnitGet(&us, PRO_UNITTYPE_LENGTH, &lmu);
    if (status != PRO_TK_NO_ERROR)
	return status;

    ProUnitInit(mdl, L"mm", &mmu);

    status = ProUnitConversionCalculate(&lmu, &mmu, &conv);
    if (status != PRO_TK_NO_ERROR)
	return status;

    (*scale) = conv.scale;
    return PRO_TK_NO_ERROR;
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

struct pparam_data {
    struct creo_conv_info *cinfo;
    char *key;
    char *val;
};

extern "C" ProError regex_key(ProParameter *param, ProError UNUSED(status), ProAppData app_data)
{
    char    pname[CREO_NAME_MAX];
    char      val[CREO_NAME_MAX];
    wchar_t  wval[CREO_NAME_MAX];

    regex_t reg;

    ProParamvalue     pval;
    ProParamvalueType ptype;
    ProUnititem       punits;

    struct pparam_data *pdata = (struct pparam_data *)app_data;
    if (pdata->val)
	return PRO_TK_NO_ERROR;

    ProWstringToString(pname, param->id);
    (void)regcomp(&reg, pdata->key, REG_NOSUB|REG_EXTENDED);
    if (!(regexec(&reg, pname, 0, NULL, 0))) {
	regfree(&reg);
	return PRO_TK_CONTINUE;
    }
    regfree(&reg);

    if (ProParameterValueWithUnitsGet(param, &pval, &punits) != PRO_TK_NO_ERROR)
	return PRO_TK_CONTINUE;

    if (ProParamvalueTypeGet(&pval, &ptype) != PRO_TK_NO_ERROR)
	return PRO_TK_CONTINUE;

    if (ptype == PRO_PARAM_STRING) {
	if (ProParamvalueValueGet(&pval, ptype, wval) != PRO_TK_NO_ERROR)
	    return PRO_TK_CONTINUE;

	ProWstringToString(val, wval);
	if (strlen(val) > 0) {
	    pdata->val = bu_strdup(val);
	}
    }

    return PRO_TK_NO_ERROR;
}


extern "C" ProError
creo_attribute_val(char **val, const char *key, ProMdl m)
{
    struct bu_vls cpval = BU_VLS_INIT_ZERO;
    wchar_t  wkey[CREO_NAME_MAX];
    wchar_t w_val[CREO_NAME_MAX];
    char     cval[CREO_NAME_MAX];
    char    *fval = NULL;

    ProError          pstatus;
    ProModelitem      mitm;
    ProParameter      param;
    ProParamvalueType ptype;
    ProParamvalue     pval;
    ProUnititem       punits;

    short  b_val;
    int    i_val;
    double d_val;

    ProStringToWstring(wkey, (char *)key);
    ProMdlToModelitem(m, &mitm);
    pstatus = ProParameterInit(&mitm, wkey, &param);

    /* if param not found, return */
    if (pstatus != PRO_TK_NO_ERROR)
	return PRO_TK_CONTINUE;

    ProParameterValueWithUnitsGet(&param, &pval, &punits);
    ProParamvalueTypeGet(&pval, &ptype);
    switch (ptype) {
	case PRO_PARAM_STRING:
	    ProParamvalueValueGet(&pval, ptype, (void *)w_val);
	    ProWstringToString(cval, w_val);
	    if (strlen(cval) > 0) fval = bu_strdup(cval);
	    break;
	case PRO_PARAM_INTEGER:
	    ProParamvalueValueGet(&pval, ptype, (void *)&i_val);
	    bu_vls_sprintf(&cpval, "%d", i_val);
	    if (bu_vls_strlen(&cpval) > 0) fval = bu_strdup(bu_vls_cstr(&cpval));
	    break;
	case PRO_PARAM_DOUBLE:
	    ProParamvalueValueGet(&pval, ptype, (void *)&d_val);
	    bu_vls_sprintf(&cpval, "%g", d_val);
	    if (bu_vls_strlen(&cpval) > 0) fval = bu_strdup(bu_vls_cstr(&cpval));
	    break;
	case PRO_PARAM_BOOLEAN:
	    ProParamvalueValueGet(&pval, ptype, (void *)&b_val);
	    fval = (b_val) ? bu_strdup("YES") : bu_strdup("NO");
	    break;
	default:
	    break;
    }

    *val = fval;
    bu_vls_free(&cpval);
    return PRO_TK_NO_ERROR;
}



extern "C" char *
creo_param_name(struct creo_conv_info *cinfo, wchar_t *creo_name, int flags)
{
    struct pparam_data pdata;
    pdata.cinfo = cinfo;
    pdata.val = NULL;
    char *val = NULL;

    ProModelitem itm;
    ProMdl model;

    if (flags == N_REGION || flags == N_SOLID) {
	if (ProMdlnameInit(creo_name, PRO_MDLFILE_PART, &model) != PRO_TK_NO_ERROR)
	    return NULL;
    } else if (flags == N_ASSEM) {
	if (ProMdlnameInit(creo_name, PRO_MDLFILE_ASSEMBLY, &model) != PRO_TK_NO_ERROR)
	    return NULL;
    } else {
	return NULL;
    }

    if (ProMdlToModelitem(model, &itm) != PRO_TK_NO_ERROR)
	return NULL;

    for (unsigned int i = 0; i < cinfo->model_parameters->size(); i++) {
	pdata.key = cinfo->model_parameters->at(i);
	/* first, try a direct lookup */
	creo_attribute_val(&pdata.val, pdata.key, model);
	/* If that didn't work, and it looks like we have regex characters,
	 * try a regex match */
	if (!pdata.val) {
	    int non_alnum = 0;
	    for (unsigned int j = 0; j < strlen(pdata.key); j++) non_alnum += !isalnum(pdata.key[j]);
	    if (non_alnum) ProParameterVisit(&itm,  NULL, regex_key, (ProAppData)&pdata);
	}
	if (pdata.val && strlen(pdata.val) > 0) {
	    int is_al = 0;
	    for (unsigned int j = 0; j < strlen(pdata.val); j++) is_al += isalpha(pdata.val[j]);
	    if (is_al > 0) {
		/* Have key - we're done here */
		val = pdata.val;
		break;
	    } else {
		/* not good enough - keep trying */
		pdata.val = NULL;
	    }
	} else {
	    pdata.val = NULL;
	}
    }
    return val;
}

/* Map a given string to the "stable" version of that string generated by objects_gather */
extern "C" wchar_t *
stable_wchar(struct creo_conv_info *cinfo, wchar_t *wc)
{
    wchar_t *stable = NULL;

    if (cinfo->parts->find(wc) != cinfo->parts->end())
	stable = *(cinfo->parts->find(wc));

    if (!stable && cinfo->assems->find(wc) != cinfo->assems->end())
	stable = *(cinfo->assems->find(wc));

    return stable;
}

/* create a unique BRL-CAD object name from a possibly illegal name */
extern "C" struct bu_vls *
get_brlcad_name(struct creo_conv_info *cinfo, wchar_t *name, const char *suffix, int flag)
{
    struct bu_vls gname_root = BU_VLS_INIT_ZERO;
    struct bu_vls *gname = NULL;
    char *param_name = NULL;
    long count = 0;
    wchar_t *stable = stable_wchar(cinfo, name);
    std::map<wchar_t *, struct bu_vls *, WStrCmp>::iterator n_it;
    std::map<wchar_t *, struct bu_vls *, WStrCmp> *nmap = NULL;
    std::set<struct bu_vls *, StrCmp> *nset = cinfo->brlcad_names;
    char astr[CREO_NAME_MAX];
    const char *keep_chars = "+-.=_";
    const char *collapse_chars = "_";

    ProWstringToString(astr, name);

    /* If we don't have a valid type, we don't know what to do */
    if (flag < N_REGION || flag > N_CREO)
	return NULL;

    /* If the name isn't in either the parts or assemblies sets, it's
     * not an active part of the conversion */
    if (!stable) {
	creo_log(cinfo, MSG_DEBUG, "%s - no stable version of name found???.\n", astr);
	return NULL;
    }

    /* Set up */
    creo_log(cinfo, MSG_DEBUG, "Generating name for %s...\n", astr);

    if (flag == N_REGION) {
	nmap = cinfo->region_name_map;
	creo_log(cinfo, MSG_DEBUG, "\t name type: region\n");
    }
    if (flag == N_ASSEM) {
	nmap = cinfo->assem_name_map;
	creo_log(cinfo, MSG_DEBUG, "\t name type: assembly\n");
    }
    if (flag == N_SOLID) {
	nmap = cinfo->solid_name_map;
	creo_log(cinfo, MSG_DEBUG, "\t name type: solid\n");
    }
    if (flag == N_CREO) {
	nmap = cinfo->creo_name_map;
	nset = cinfo->creo_names;
	creo_log(cinfo, MSG_DEBUG, "\t name type: CREO name\n");
    }

    /* If we somehow don't have a map, bail */
    if (!nmap) return NULL;

    /* If we've already got something, return it. */
    n_it = nmap->find(name);
    if (n_it != nmap->end()) {
	gname = n_it->second;
	creo_log(cinfo, MSG_DEBUG, "name \"%s\" already exists\n", bu_vls_addr(gname));
	return gname;
    }

    /* Nope - start generating */
    BU_GET(gname, struct bu_vls);
    bu_vls_init(gname);

    /* First try the parameters, if the user specified any */
    if (flag != N_CREO) {
	param_name = creo_param_name(cinfo, name, flag);
	bu_vls_sprintf(&gname_root, "%s", param_name);
    }

    /* If we don't already have a name, use the CREO name */
    if (!param_name) {
	char val[CREO_NAME_MAX];
	ProWstringToString(val, name);
	bu_vls_sprintf(&gname_root, "%s", val);
    } else {
	bu_free(param_name, "free original param name");
    }

    /* scrub */
    lower_case(bu_vls_addr(&gname_root));
    bu_vls_simplify(&gname_root, keep_chars, collapse_chars, collapse_chars);
    bu_vls_sprintf(gname, "%s", bu_vls_addr(&gname_root));

    /* if we don't have something by now, go with unknown */
    if (!bu_vls_strlen(gname)) bu_vls_sprintf(gname, "unknown");

    if (suffix) bu_vls_printf(gname, ".%s", suffix);

    /* create a unique name */
    if (nset->find(gname) != nset->end()) {
	bu_vls_sprintf(gname, "%s_1", bu_vls_addr(&gname_root));
	if (suffix) {bu_vls_printf(gname, ".%s", suffix);}
	while (nset->find(gname) != nset->end()) {
	    (void)bu_vls_incr(gname, NULL, NULL, NULL, NULL);
	    count++;
	    creo_log(cinfo, MSG_DEBUG, "\t trying name : %s\n", bu_vls_addr(gname));
	    if (count == LONG_MAX) {
		bu_vls_free(gname);
		BU_PUT(gname, struct bu_vls);
		creo_log(cinfo, MSG_DEBUG, "%s name generation FAILED.\n", astr);
		return NULL;
	    }
	}
    }

    /* Use the stable wchar_t string pointer for this name - don't
     * assume all callers will be using the parts/assems copies. */
    nset->insert(gname);
    nmap->insert(std::pair<wchar_t *, struct bu_vls *>(stable, gname));

    creo_log(cinfo, MSG_DEBUG, "name \"%s\" succeeded\n", bu_vls_addr(gname));

    return gname;
}

/* Filter for the feature visit routine that selects only "component" items
 * (should be only parts and assemblies) */
extern "C" ProError
component_filter(ProFeature *feat, ProAppData *UNUSED(data))
{
    ProFeattype type;
    ProFeatStatus feat_stat;

    if (ProFeatureTypeGet(feat, &type) != PRO_TK_NO_ERROR || type != PRO_FEAT_COMPONENT)
	return PRO_TK_CONTINUE;
    if (ProFeatureStatusGet(feat, &feat_stat) != PRO_TK_NO_ERROR || feat_stat != PRO_FEAT_ACTIVE)
	return PRO_TK_CONTINUE;

    return PRO_TK_NO_ERROR;
}

/* Throw a message up in a dialog */
ProError PopupMsg(const char *title, const char *msg)
{
    wchar_t wtitle[CREO_NAME_MAX];
    wchar_t wmsg[CREO_MSG_MAX];
    ProUIMessageButton* button = NULL;
    ProUIMessageButton bresult;

    ProArrayAlloc(1, sizeof(ProUIMessageButton), 1, (ProArray*)&button);
    button[0] = PRO_UI_MESSAGE_OK;
    ProStringToWstring(wtitle, (char *)title);
    ProStringToWstring(wmsg, (char *)msg);
    ProUIMessageDialogDisplay(PROUIMESSAGE_INFO, wtitle, wmsg, button, PRO_UI_MESSAGE_OK, &bresult);
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
