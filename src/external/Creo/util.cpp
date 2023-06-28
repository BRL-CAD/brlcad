/**
 *                  U T I L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2017-2023 United States Government as represented by
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
/**
 * @file util.cpp
 */


/*                                                                                  */
/* Synopsis of Utiliity Routines:                                                   */
/*                                                                                  */
/* component_filter      Component item filter for the feature visit routine        */
/* creo_log              Report conversion status and log file messages             */
/* creo_model_units      Extracts Creo model units and conversion factor            */
/* creo_param_name       Returns first valid alpha-numeric parameter name string    */
/* creo_param_val        Extract parameter value from specified Creo model          */
/* find_matl             Determine if specified material is on the material list    */
/* get_brlcad_name       Returns a unique BRL-CAD object name                       */
/* get_mtl_input         Process input from specified material translation file     */
/* lower_case            Converts string to lower case                              */
/* param_append          Append parameter to the array                              */
/* param_collect         Collect available parameters from the specified model      */
/* param_export          Export list of model parameters                            */
/* param_preserve        Preserve available model parameters                        */
/* params_to_attrs       Preserve a list of model-specific parameters as attributes */
/* parse_param_list      Parse list of user-supplied parameters                     */
/* PopupMsg              Display a message in a Creo dialog box                     */
/* regex_key             Utilize regular expression match for Creo parameter name   */
/* rgb4lmin              Modify RGB values to achieve minimum luminance threshold   */
/* scrub_vls             Removes unwanted characters from a variable-length string  */
/* stable_wchar          Map a string to the "stable" version found in parts/assems */
/* trim                  Purge string of leading and trailing whitespace            */
/* wstr_to_double        Convert wide string to double precision value              */
/* wstr_to_long          Convert wide string to long int value                      */



#include "common.h"
#include <algorithm>
#include <regex.h>
#include "creo-brl.h"


/*
 * Component item filter for the feature visit routine
 * (should be only parts and assemblies)
 */
extern "C" ProError
component_filter(ProFeature *feat, ProAppData *UNUSED(data))
{
    ProFeattype   ftype;
    ProFeatStatus feat_stat;

    if (ProFeatureTypeGet(feat, &ftype) != PRO_TK_NO_ERROR || ftype != PRO_FEAT_COMPONENT)
        return PRO_TK_CONTINUE;
    if (ProFeatureStatusGet(feat, &feat_stat) != PRO_TK_NO_ERROR || feat_stat != PRO_FEAT_ACTIVE)
        return PRO_TK_CONTINUE;

    return PRO_TK_NO_ERROR;
}


/* Report conversion status and log file messages */
extern "C" void
creo_log(struct creo_conv_info *cinfo, int msg_type, const char *fmt, ...) {
    /*
     * NOTE - need Creo specific semaphore lock for this if it's going to be used
     * in multi-threading situations... - probably can't use libbu's logging safely
     */

    /* Creo GUI */
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
    if (cinfo && cinfo->fplog && msg_type != MSG_STATUS) {
        cinfo->curr_msg_type = msg_type;
        struct bu_vls vmsg = BU_VLS_INIT_ZERO;

        if (cinfo->logger_type == LOGGER_TYPE_NONE)
            return;

        switch (cinfo->curr_msg_type) {
            case MSG_FAIL:
                bu_vls_sprintf(&vmsg, "FAILURE: %s", msg);
                break;
            case MSG_SUCCESS:
                bu_vls_sprintf(&vmsg, "SUCCESS: %s", msg);
                break;
            case MSG_DEBUG:
                bu_vls_sprintf(&vmsg, "  DEBUG: %s", msg);
                break;
            case MSG_PLAIN:
                bu_vls_sprintf(&vmsg, "%s", msg);
                break;
            default:
                bu_vls_sprintf(&vmsg, "  OTHER: %s", msg);
            }

        if ((cinfo->curr_msg_type == MSG_FAIL    && cinfo->logger_type != LOGGER_TYPE_SUCCESS)            ||
            (cinfo->curr_msg_type == MSG_SUCCESS && cinfo->logger_type != LOGGER_TYPE_FAILURE)            ||
            (cinfo->curr_msg_type != MSG_DEBUG   && cinfo->logger_type == LOGGER_TYPE_FAILURE_OR_SUCCESS) ||
            (cinfo->logger_type == LOGGER_TYPE_ALL)) {
            fprintf(cinfo->fplog, "%s", bu_vls_addr(&vmsg));
            fflush(cinfo->fplog);
        }
        bu_vls_free(&vmsg);
    }
}


/* Extracts Creo model units and conversion factor */
extern "C" ProError
creo_model_units(double *scale, ProMdl mdl)
{
    ProError          err = PRO_TK_GENERAL_ERROR;
    ProUnitsystem     us;
    ProUnititem       lmu, mmu;
    ProUnitConversion conv;

    if (!scale)
        return err;

    err = ProMdlPrincipalunitsystemGet(mdl, &us);
    if (err != PRO_TK_NO_ERROR)
        return err;

    err = ProUnitsystemUnitGet(&us, PRO_UNITTYPE_LENGTH, &lmu);
    if (err != PRO_TK_NO_ERROR)
        return err;

    ProUnitInit(mdl, L"mm", &mmu);

    err = ProUnitConversionCalculate(&lmu, &mmu, &conv);
    if (err != PRO_TK_NO_ERROR)
        return err;

    (*scale) = conv.scale;
    return PRO_TK_NO_ERROR;
}


struct pparam_data {
    struct creo_conv_info *cinfo;
    char *key;
    char *val;
};


/* Returns first valid alpha-numeric parameter name string */
extern "C" char *
creo_param_name(struct creo_conv_info *cinfo, wchar_t *creo_name, int flag)
{
    struct pparam_data pdata;
    pdata.cinfo = cinfo;
    pdata.val = NULL;
    char *val = NULL;

    ProModelitem mitm;
    ProMdl model;

    if (flag == N_REGION || flag == N_SOLID) {
        if (ProMdlnameInit(creo_name, PRO_MDLFILE_PART, &model) != PRO_TK_NO_ERROR)
            return NULL;
    } else if (flag == N_ASSEM) {
        if (ProMdlnameInit(creo_name, PRO_MDLFILE_ASSEMBLY, &model) != PRO_TK_NO_ERROR)
            return NULL;
    } else
        return NULL;

    if (ProMdlToModelitem(model, &mitm) != PRO_TK_NO_ERROR)
        return NULL;

    for (unsigned int i = 0; i < cinfo->obj_name_params->size(); i++) {
        pdata.key = cinfo->obj_name_params->at(i);
        /* First, try a direct lookup */
        creo_param_val(&pdata.val, pdata.key, model);
        /* If that didn't work, and it looks like we have regex characters,
         * try a regex match */
        if (!pdata.val) {
            int non_alnum = 0;
            for (unsigned int j = 0; j < strlen(pdata.key); j++)
                non_alnum += !isalnum(pdata.key[j]);
            if (non_alnum)
                ProParameterVisit(&mitm,  NULL, regex_key, (ProAppData)&pdata);
        }
        if (pdata.val && strlen(pdata.val) > 0) {
            int is_al = 0;
            for (unsigned int j = 0; j < strlen(pdata.val); j++)
                is_al += isalpha(pdata.val[j]);
            if (is_al > 0) {
                /* Have key - we're done here */
                val = pdata.val;
                break;
            } else {
                /* not good enough - keep trying */
                pdata.val = NULL;
            }
        } else
            pdata.val = NULL;
    }
    return val;
}


/* Extract parameter value from specified Creo model */
extern "C" ProError
creo_param_val(char **val, const char *key, ProMdl model)
{
    struct bu_vls cpval = BU_VLS_INIT_ZERO;
    wchar_t  wkey[CREO_NAME_MAX];
    wchar_t w_val[CREO_NAME_MAX];
    char    c_val[CREO_NAME_MAX];
    char    *fval = NULL;

    ProError          err = PRO_TK_GENERAL_ERROR;
    ProModelitem      mitm;
    ProParameter      param;
    ProParamvalueType ptype;
    ProParamvalue     pval;
    ProUnititem       punits;

    short  b_val;
    int    i_val;
    double d_val;

    ProStringToWstring(wkey, (char *)key);
    (void)ProMdlToModelitem(model, &mitm);
    err = ProParameterInit(&mitm, wkey, &param);

    /* if param not found, return */
    if (err != PRO_TK_NO_ERROR)
        return PRO_TK_CONTINUE;

    ProParameterValueWithUnitsGet(&param, &pval, &punits);
    ProParamvalueTypeGet(&pval, &ptype);
    switch (ptype) {
        case PRO_PARAM_STRING:
            ProParamvalueValueGet(&pval, ptype, (void *)w_val);
            ProWstringToString(c_val, w_val);
            lower_case(c_val);
            bu_vls_sprintf(&cpval, "%s", c_val);
            scrub_vls(&cpval);
            if (bu_vls_strlen(&cpval) > 0)
                fval = bu_strdup(bu_vls_cstr(&cpval));
            break;
        case PRO_PARAM_INTEGER:
            ProParamvalueValueGet(&pval, ptype, (void *)&i_val);
            bu_vls_sprintf(&cpval, "%d", i_val);
            if (bu_vls_strlen(&cpval) > 0)
                fval = bu_strdup(bu_vls_cstr(&cpval));
            break;
        case PRO_PARAM_DOUBLE:
            ProParamvalueValueGet(&pval, ptype, (void *)&d_val);
            bu_vls_sprintf(&cpval, "%g", d_val);
            if (bu_vls_strlen(&cpval) > 0)
                fval = bu_strdup(bu_vls_cstr(&cpval));
            break;
        case PRO_PARAM_BOOLEAN:
            ProParamvalueValueGet(&pval, ptype, (void *)&b_val);
            fval = (b_val) ? bu_strdup("yes") : bu_strdup("no");
            break;
    }

    *val = fval;
    bu_vls_free(&cpval);
    return PRO_TK_NO_ERROR;
}


/* Determine if specified material is on the material list */
extern "C" int
find_matl(struct creo_conv_info *cinfo)
{
    int keylen = int(strlen(cinfo->mtl_key));

    if (keylen < 1)
        return 0;

    cinfo->mtl_ptr = -1;

    for (int n = 0; n < MAX_FILE_RECS; n++)
        if (cinfo->mtl_str[n][0] != '\0')
            if (memcmp(cinfo->mtl_key, &(cinfo->mtl_str[n][0]), keylen+1) == 0) {
                cinfo->mtl_ptr = n;
                break;
            }

    return (cinfo->mtl_ptr >= 0);
}


/* Returns a unique BRL-CAD object name */
extern "C" struct bu_vls *
get_brlcad_name(struct creo_conv_info *cinfo, wchar_t *name, const char *suffix, int flag)
{
    struct bu_vls  gname_root = BU_VLS_INIT_ZERO;
    struct bu_vls *gname;
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
    lower_case(astr);

    if (!stable) {
        creo_log(cinfo, MSG_PLAIN, "   NAME: \"%s\" unable to find stable version of name\n", astr);
        return NULL;
    }

    creo_log(cinfo, MSG_PLAIN, "   NAME: Generating name for \"%s\"...\n", astr);

    switch (flag) {
        case N_REGION:
            nmap = cinfo->region_name_map;
            creo_log(cinfo, MSG_PLAIN, "   NAME: Name type: region\n");
            break;
        case N_ASSEM:
            nmap = cinfo->assem_name_map;
            creo_log(cinfo, MSG_PLAIN, "   NAME: Name type: assembly\n");
            break;
        case N_SOLID:
            nmap = cinfo->solid_name_map;
            creo_log(cinfo, MSG_PLAIN, "   NAME: Name type: solid\n");
            break;
        case N_CREO:
            nmap = cinfo->creo_name_map;
            nset = cinfo->creo_names;
            creo_log(cinfo, MSG_PLAIN, "   NAME: Name type: Creo name\n");
            break;
        default:
            return NULL;               /* Ignore unknown name type */
    }

    /* If we somehow don't have a map, bail */
    if (!nmap)
        return NULL;

    /* If we've already got something, return it. */
    n_it = nmap->find(name);
    if (n_it != nmap->end()) {
        gname = n_it->second;
        creo_log(cinfo, MSG_PLAIN, "   NAME: \"%s\" already exists\n", bu_vls_addr(gname));
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

    /* If we don't already have a name, use the Creo name */
    if (!param_name) {
        char val[CREO_NAME_MAX];
        ProWstringToString(val, name);
        bu_vls_sprintf(&gname_root, "%s", val);
    } else
        bu_free(param_name, "free original param name");

    /* scrub */
    lower_case(bu_vls_addr(&gname_root));
    bu_vls_simplify(&gname_root, keep_chars, collapse_chars, collapse_chars);
    bu_vls_sprintf(gname, "%s", bu_vls_addr(&gname_root));

    /* if we don't have something by now, go with unknown */
    if (!bu_vls_strlen(gname))
        bu_vls_sprintf(gname, "unknown");

    if (suffix)
        bu_vls_printf(gname, ".%s", suffix);

    /* create a unique name */
    if (nset->find(gname) != nset->end()) {
        bu_vls_sprintf(gname, "%s_1", bu_vls_addr(&gname_root));
        if (suffix)
            bu_vls_printf(gname, ".%s", suffix);
        for (count = 0; nset->find(gname) != nset->end(); count++) {
            (void)bu_vls_incr(gname, NULL, NULL, NULL, NULL);
            if (count == 2)
                creo_log(cinfo, MSG_PLAIN, "   NAME: Seeking unique object name \"%s\"\n", bu_vls_addr(&gname_root));
            else if (count >= MAX_UNIQUE_NAMES) {
                bu_vls_free(gname);
                BU_PUT(gname, struct bu_vls);
                creo_log(cinfo, MSG_PLAIN, "   NAME: Failed name generation for \"%s\"\n", astr);
                return NULL;
            }
        }
    }

    /*
     * Use the stable wchar_t string pointer for this name - don't
     * assume all callers will be using the parts/assems copies.
     */
    nset->insert(gname);
    nmap->insert(std::pair<wchar_t *, struct bu_vls *>(stable, gname));

    creo_log(cinfo, MSG_PLAIN, "   NAME: \"%s\" succeeded\n", bu_vls_addr(gname));

    return gname;
}


/* Process input from specified material translation file */
extern "C" int
get_mtl_input(FILE *fpmtl, char *mtl_str, int *mtl_id, int *mtl_los)
{
    const int  cols = MAX_MATL_NAME + 1;
    const int  mbuf = MAX_LINE_BUFFER;
    const char comments[] = "!@#$%^&*/<>?";
    char buf[MAX_LINE_BUFFER];
    char mtl[MAX_LINE_BUFFER];
    char firstc;
    int  id, los;
    int  recs;

    /* Initialize counter */
    recs = 0;
    while (bu_fgets(buf, mbuf, fpmtl) != NULL) {
        trim(buf);
        firstc = buf[0];
        if(strchr(comments, firstc))   /* skip comments */
            continue;
        else if (sscanf(buf, "%s%d%d", mtl, &id, &los) != 3)
            continue;                  /* skip invalid input */
        else if (recs < MAX_FILE_RECS) {
            for (int n = 0; mtl[n]; n++)
                mtl_str[recs*cols + n] = tolower(mtl[n]);
            mtl_id[recs]  = id;
            mtl_los[recs] = los;
            recs++;
        } else {
            mtl_str[recs*cols + 1] = '\0';
            break;
        }
    }
    return recs;
}


/* Converts string to lower case */
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


/* Append parameter to the array */
extern "C" ProError
param_append(void *p_object, ProError UNUSED(filt_err), ProAppData app_data)
{
    ProError err = PRO_TK_GENERAL_ERROR;
    ProArray *p_array;

    p_array = (ProArray*)((void**)app_data)[0];

    err = ProArrayObjectAdd(p_array, PRO_VALUE_UNUSED, 1, p_object );

    return err;
}


/* Collect available parameters from the specified model */
extern "C" ProError
param_collect(ProModelitem *p_modelitem, ProParameter **p_parameters)
{
    ProError err = PRO_TK_GENERAL_ERROR;

    if (p_parameters != NULL) {
        err = ProArrayAlloc(0, sizeof(ProParameter), 1, (ProArray*)p_parameters);
        if (err == PRO_TK_NO_ERROR ) {
            err = ProParameterVisit(p_modelitem,
                                    NULL,
                                    (ProParameterAction)param_append,
                                    (ProAppData)&p_parameters);
            if (err != PRO_TK_NO_ERROR) {
                (void)ProArrayFree((ProArray*)p_parameters);
                *p_parameters = NULL;
            }
        }
    }
    else
        err = PRO_TK_BAD_INPUTS;

    return err;
}


/* Export list of model parameters */
extern "C" void
param_export(struct creo_conv_info *cinfo, ProMdl model, const char *name)
{

    if (cinfo->obj_attr_params->size() > 0)
        for (unsigned int i = 0; i < cinfo->obj_attr_params->size(); i++) {
            char *attr_val = NULL;
            const char *arg = cinfo->obj_attr_params->at(i);
            creo_param_val(&attr_val, arg, model);
            if (attr_val) {
                bu_avs_add(&cinfo->avs, arg, attr_val);
                bu_free(attr_val, "value string");
                }
        }
    else
        (void) param_preserve(cinfo, model, name);

}


/* Preserve available model parameters */
extern "C" ProError
param_preserve(struct creo_conv_info *cinfo, ProMdl model, const char *name)
{
    ProError      err = PRO_TK_GENERAL_ERROR;
    ProModelitem  mitm;
    ProParameter *pars;

    err = ProMdlToModelitem(model, &mitm);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_PLAIN, "FAILURE: Unable to get \"%s\" model item identifier\n", name);
        return err;
    }

    err = param_collect(&mitm, &pars);
    if (err == PRO_TK_BAD_INPUTS)
        creo_log(cinfo, MSG_PLAIN, "FAILURE: Invalid inputs for \"%s\" parameter collection\n", name);
    else if (err != PRO_TK_NO_ERROR)
        creo_log(cinfo, MSG_PLAIN, "FAILURE: Unable to collect \"%s\" model parameters\n", name);
    else {
        err = params_to_attrs(cinfo, model, pars);
        if (err == PRO_TK_BAD_INPUTS)
            creo_log(cinfo, MSG_PLAIN, "FAILURE: Invalid inputs for \"%s\" attribute creation\n", name);
        else if (err == PRO_TK_NOT_EXIST)
            creo_log(cinfo, MSG_PLAIN, "  PARAM: No parameters for \"%s\" are available\n", name);
    }

    (void)ProArrayFree((ProArray*)&pars);
    return err;
}


/* Preserve a list of model-specific parameters as attributes */
extern "C" ProError
params_to_attrs(struct creo_conv_info *cinfo, ProMdl model, ProParameter* pars)
{
    ProError  err = PRO_TK_GENERAL_ERROR;
    int count = 0;
    int found = 0;

    err = ProArraySizeGet((ProArray)pars, &count);
    if (err != PRO_TK_NO_ERROR)
        return err;
    else if (count < 1)
        return PRO_TK_NOT_EXIST;

    /* Add every available parameter that has a value to the list */
    for (int i = 0; i < count; i++) {
        char *attr_val = NULL;
        char  attr_nam[CREO_NAME_MAX];

        ProWstringToString(attr_nam, pars[i].id);
        lower_case(attr_nam);
        creo_param_val(&attr_val, attr_nam, model);

        if (attr_val) {
            found++;
            if (found == 1) {
                creo_log(cinfo, MSG_PLAIN, "  PARAM: ==========================================================\n");
                creo_log(cinfo, MSG_PLAIN, "  PARAM:    n          ptc_parameter_name               value\n");
                                                  /*  xxx  xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx  xxxxxxxxxxxxxxxxxxxxx */
                creo_log(cinfo, MSG_PLAIN, "  PARAM: ----------------------------------------------------------\n");
            }
            creo_log(cinfo, MSG_PLAIN, "  PARAM:  %3d  %-32s  %-s\n", found, attr_nam, attr_val);
            bu_avs_add(&cinfo->avs, attr_nam, attr_val);
            bu_free(attr_val, "value string");
            }
    }

    if (found > 0) {
        creo_log(cinfo, MSG_PLAIN, "  PARAM: ----------------------------------------------------------\n");
        creo_log(cinfo, MSG_PLAIN, "  PARAM: Processed %d model parameters\n",      count);
        if (found == 1)
            creo_log(cinfo, MSG_PLAIN, "  PARAM: Extracted a single non-empty parameter\n");
        else
            creo_log(cinfo, MSG_PLAIN, "  PARAM: Extracted %d non-empty parameters \n", found);
        if (count > found)
            creo_log(cinfo, MSG_PLAIN, "  PARAM: Extraction ratio: %.1f%s\n",
                     double(found)/double(count)*100.0, "%");
        else
            creo_log(cinfo, MSG_PLAIN, "  PARAM: Extraction ratio: 100%s\n", "%");
    } else
        creo_log(cinfo, MSG_PLAIN, "  PARAM: No model parameters were found\n");

    return PRO_TK_NO_ERROR;
}


/* Parse list of user-supplied parameters */
extern "C" void
parse_param_list(struct creo_conv_info *cinfo, const char *param_list, int flag)
{
    if (strlen(param_list) > 0) {
        struct bu_vls msg = BU_VLS_INIT_ZERO;
        if (flag == NAME_PARAMS)
            bu_vls_printf(&msg, "#              Creates object names:");
        else if (flag == ATTR_PARAMS)
            bu_vls_printf(&msg, "#               Preserved attribute:");
        else
            bu_vls_printf(&msg, "    FAILURE: Unknown parameter type:");
        std::string filestr(param_list);
        std::istringstream ss(filestr);
        std::string line;
        while (std::getline(ss, line)) {
            std::string key;
            std::istringstream ls(line);
            while (std::getline(ls, key, ',')) {
                /* Scrub leading and trailing whitespace */
                size_t startpos = key.find_first_not_of(" \t\n\v\f\r");
                if (std::string::npos != startpos)
                    key = key.substr(startpos);
                size_t endpos = key.find_last_not_of(" \t\n\v\f\r");
                if (std::string::npos != endpos)
                    key = key.substr(0 ,endpos+1);
                if (key.length() > 0) {
                    if (flag == NAME_PARAMS)
                        cinfo->obj_name_params->push_back(bu_strdup(key.c_str()));
                    else if (flag == ATTR_PARAMS)
                        cinfo->obj_attr_params->push_back(bu_strdup(key.c_str()));
                    creo_log(cinfo, MSG_PLAIN, "%s \"%s\"\n", bu_vls_addr(&msg), key.c_str());
                }
            }
        }
        bu_vls_free(&msg);
    }
}


/* Display a message in a Creo dialog box */
extern "C" ProError
PopupMsg(const char *title, const char *msg)
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


/* Utilize regular expression match for Creo parameter name */
extern "C" ProError
regex_key(ProParameter *param, ProError UNUSED(status), ProAppData app_data)
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
        if (strlen(val) > 0)
            pdata->val = bu_strdup(val);
    }

    return PRO_TK_NO_ERROR;
}


/* Modify RGB values to achieve minimum luminance threshold */
extern "C" int
rgb4lmin(fastf_t *rgb, int lmin)
{
    /* employs macro-defined functions:
     *   #define max(a,b) (((a)>(b))?(a):(b))
     *   #define min(a,b) (((a)<(b))?(a):(b))
     *   #define dmod(a,b) ((a) - floor(double((a))/double((b)))*(b))
     */

    int rp,gp,bp,lp,cmax,cmin;

    fastf_t del,hue,sat,lum;
    fastf_t cp,hp,mp,xp;
    fastf_t rf,gf,bf;

    if (UNLIKELY(!rgb))                          /* bad input? */
        return -1;

    /* Restrict input range for: rgb, lmin */
    rp = std::min<long int>(std::max<long int>(lrint(rgb[0]*255.0),(long int)0),(long int)255);
    gp = std::min<long int>(std::max<long int>(lrint(rgb[1]*255.0),(long int)0),(long int)255);
    bp = std::min<long int>(std::max<long int>(lrint(rgb[2]*255.0),(long int)0),(long int)255);
    lp = std::min<int>(std::max<int>(lmin,0),100);

    cmax = std::max<int>(std::max<int>(rp,gp),bp);
    cmin = std::min<int>(std::min<int>(rp,gp),bp);

    del  = cmax-cmin;
    lum  = (cmax+cmin)/510.0;

    if (lrint(100*lum) >= lp)                    /* current luminance already */
        return 0;                                /* exceeds minimum threshold */

    if (cmax == cmin)
        hue = 0.0;
    else if (cmax == rp)
        hue = 60.0*dmod((gp-bp)/del,6.0);
    else if (cmax == gp)
        hue = 60.0*((bp-rp)/del + 2.0);
    else
        hue = 60.0*((rp-gp)/del + 4.0);

    if (cmax <= 0 || cmin >= 255)
        sat = 0.0;
    else
        sat = del/(255-abs(cmax+cmin-255));

    hp = hue/60.0;
    cp = (1.0-abs(lp/50.0-1.0))*sat;
    xp = (1.0-abs(dmod(hp,2)-1.0))*cp;
    mp = (lp/50.0-cp)/2.0;

    switch((int)hp)
    {
        case 0:    /*   0 <= hue <  60 */
            rf =  cp;  gf =  xp;  bf = 0.0;
            break;
        case 1:    /*  60 <= hue < 120 */
            rf =  xp;  gf =  cp;  bf = 0.0;
            break;
        case 2:    /* 120 <= hue < 180 */
            rf = 0.0;  gf =  cp;  bf =  xp;
            break;
        case 3:    /* 180 <= hue < 240 */
            rf = 0.0;  gf =  xp;  bf =  cp;
            break;
        case 4:    /* 240 <= hue < 300 */
            rf =  xp;  gf = 0.0;  bf =  cp;
            break;
        default:   /* 300 <= hue < 360 */
            rf =  cp;  gf = 0.0;  bf =  xp;
    }

    /* Restrict rgb output range */
    rgb[0] = std::min<fastf_t>(std::max<fastf_t>((rf + mp),0.0),1.0);
    rgb[1] = std::min<fastf_t>(std::max<fastf_t>((gf + mp),0.0),1.0);
    rgb[2] = std::min<fastf_t>(std::max<fastf_t>((bf + mp),0.0),1.0);

    return 1;
}


/* Removes unwanted characters from a variable-length string */
extern "C" void
scrub_vls(struct bu_vls *vls)
{
    struct bu_vls tmp_str = BU_VLS_INIT_ZERO;

    const char *keep_chars = "+-.=_";
    const char *collapse_chars = "_";

    if (bu_vls_strlen(vls) > 0) {
        bu_vls_sprintf(&tmp_str, "%s", bu_vls_cstr(vls));
        bu_vls_trimspace(&tmp_str);
        bu_vls_simplify(&tmp_str, keep_chars, collapse_chars, collapse_chars);
        bu_vls_sprintf(vls, "%s", bu_vls_addr(&tmp_str));
    }

    bu_vls_free(&tmp_str);
    return;
}


/* Map a string to the "stable" version found in parts/assems */
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


/* Purge string of leading and trailing whitespace */
extern "C" void
trim(char *str)
{
    int n, m, p;

    if (str[0] == '\012' || str[0] == '\015') {
        str[0] = '\0';
        return;
    }

    n = 0;
    while (str[n] != '\0' && isspace((int)str[n]))
        n++;

    m = int(strlen(str)) - 1;
    while (m && (isspace((int)str[m]) || str[m] == '\012' || str[m] == '\015')) {
        str[m] = '\0';  /* pad with nulls */
        m--;
    }

    if (m > n) {
        for (p = 0; p < (m-n); p++)
            str[p] = str[n+p];
        str[m-n+1] = '\0';
    }

    return;
}


/* Convert wide string to double precision value */
extern "C" double
wstr_to_double(struct creo_conv_info *cinfo, wchar_t *tmp_str)
{
    double ret = 0.0;
    char *endptr = NULL;
    char astr[CREO_MSG_MAX];

    ProWstringToString(astr, tmp_str);

    ret = strtod(astr, &endptr);
    if (endptr != NULL && strlen(endptr) > 0) {
        /* Had some invalid character in the input, fail */
        creo_log(cinfo, MSG_PLAIN, " STRING: Invalid data \"%s\" specified for type double\n", astr);
        return -1.0;
    }
    return ret;
}


/* Convert wide string to long int value */
extern "C" long int
wstr_to_long(struct creo_conv_info *cinfo, wchar_t *tmp_str)
{
    long int ret = 0;
    char *endptr = NULL;
    char astr[CREO_MSG_MAX];

    ProWstringToString(astr, tmp_str);

    ret = strtol(astr, &endptr, 0);
    if (endptr != NULL && strlen(endptr) > 0) {
        /* Had some invalid character in the input, fail */
        creo_log(cinfo, MSG_PLAIN, " STRING: Invalid data \"%s\" specified for integer type\n", astr);
        return -1;
    }
    return ret;
}


/// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
