/**
 *                    U T I L . C P P
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

#include "common.h"
#include <algorithm>
#include <regex.h>
#include "creo-brl.h"


/*
 * Filter for the feature visit routine that selects only "component" items
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


extern "C" ProError
creo_attribute_val(char **val, const char *key, ProMdl m)
{
    struct bu_vls cpval = BU_VLS_INIT_ZERO;
    wchar_t  wkey[CREO_NAME_MAX];
    wchar_t w_val[CREO_NAME_MAX];
    char     cval[CREO_NAME_MAX];
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
    ProMdlToModelitem(m, &mitm);
    err = ProParameterInit(&mitm, wkey, &param);

    /* if param not found, return */
    if (err != PRO_TK_NO_ERROR)
        return PRO_TK_CONTINUE;

    ProParameterValueWithUnitsGet(&param, &pval, &punits);
    ProParamvalueTypeGet(&pval, &ptype);
    switch (ptype) {
        case PRO_PARAM_STRING:
            ProParamvalueValueGet(&pval, ptype, (void *)w_val);
            ProWstringToString(cval, w_val);
            if (strlen(cval) > 0)
                fval = bu_strdup(cval);
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
            fval = (b_val) ? bu_strdup("YES") : bu_strdup("NO");
            break;
    }

    *val = fval;
    bu_vls_free(&cpval);
    return PRO_TK_NO_ERROR;
}


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
    } else
        return NULL;

    if (ProMdlToModelitem(model, &itm) != PRO_TK_NO_ERROR)
        return NULL;

    for (unsigned int i = 0; i < cinfo->model_parameters->size(); i++) {
        pdata.key = cinfo->model_parameters->at(i);
        /* First, try a direct lookup */
        creo_attribute_val(&pdata.val, pdata.key, model);
        /* If that didn't work, and it looks like we have regex characters,
         * try a regex match */
        if (!pdata.val) {
            int non_alnum = 0;
            for (unsigned int j = 0; j < strlen(pdata.key); j++)
                non_alnum += !isalnum(pdata.key[j]);
            if (non_alnum)
                ProParameterVisit(&itm,  NULL, regex_key, (ProAppData)&pdata);
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


/* determine if specified material is on the material list */
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


/* Create a unique BRL-CAD object name from a possibly illegal name */
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
        creo_log(cinfo, MSG_DEBUG, "\"%s\" unable to find stable version of name\n", astr);
        return NULL;
    }

    creo_log(cinfo, MSG_DEBUG, "Generating name for \"%s\"...\n", astr);

    switch (flag) {
        case N_REGION:
            nmap = cinfo->region_name_map;
            creo_log(cinfo, MSG_DEBUG, "Name type: region\n");
            break;
        case N_ASSEM:
            nmap = cinfo->assem_name_map;
            creo_log(cinfo, MSG_DEBUG, "Name type: assembly\n");
            break;
        case N_SOLID:
            nmap = cinfo->solid_name_map;
            creo_log(cinfo, MSG_DEBUG, "Name type: solid\n");
            break;
        case N_CREO:
            nmap = cinfo->creo_name_map;
            nset = cinfo->creo_names;
            creo_log(cinfo, MSG_DEBUG, "Name type: Creo name\n");
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
        creo_log(cinfo, MSG_DEBUG, "Name \"%s\" already exists\n", bu_vls_addr(gname));
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
                creo_log(cinfo, MSG_DEBUG, "Seeking unique name for object \"%s\"\n", bu_vls_addr(&gname_root));
            else if (count >= MAX_UNIQUE_NAMES) {
                bu_vls_free(gname);
                BU_PUT(gname, struct bu_vls);
                creo_log(cinfo, MSG_DEBUG, "Failed name generation for \"%s\"\n", astr);
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

    creo_log(cinfo, MSG_DEBUG, "Name \"%s\" succeeded\n", bu_vls_addr(gname));

    return gname;
}


/* process input from specified material translation file */
extern "C" int
get_mtl_input(FILE *fpmtl, char *mtl_str, int *mtl_id, int *mtl_los)
{
    const int  cols = MAX_LINE_SIZE + 1;
    const char comments[] = "!@#$%^&*/<>?";
    char buf[MAX_LINE_SIZE + 1];
    char mtl[sizeof(buf) - 4];
    char firstc;
    int  id, los;
    int  recs;

    /* Initialize counter */
    recs = 0;
    while (bu_fgets(buf, cols, fpmtl) != NULL) {
        trim(buf);
        firstc = buf[0];
        if(strchr(comments, firstc))   /* skip comments */
            continue;
        else if (sscanf(buf, "%s%d%d", mtl, &id, &los) != 3)
            continue;                  /* skip invalid input */
        else if (recs < MAX_FILE_RECS) {
            for (int n = 0; n < cols; n++)
                if (n <= int(strlen(mtl)))
                    mtl_str[recs*cols + n] = tolower(mtl[n]);
                else {
                    mtl_str[recs*cols + n] = '\0';
                    break;
                }
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


/* Throw a message up in a dialog */
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


/* modify RGB values to achieve minimum luminance threshold */
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

    rp = std::min<long int>(std::max<long int>(lrint(rgb[0]*255.0),(long int)0),(long int)255);    /* control input range */
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

    rgb[0] = std::min<fastf_t>(std::max<fastf_t>((rf + mp),0.0),1.0);        /* control output range */
    rgb[1] = std::min<fastf_t>(std::max<fastf_t>((gf + mp),0.0),1.0);
    rgb[2] = std::min<fastf_t>(std::max<fastf_t>((bf + mp),0.0),1.0);

    return 1;
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


/* left-shift string to suppress leading and trailing whitespace */
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
        creo_log(cinfo, MSG_PLAIN, "ERROR: Invalid string \"%s\" specified for type double\n", astr);
        return -1.0;
    }
    return ret;
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
        creo_log(cinfo, MSG_PLAIN, "ERROR: Invalid string \"%s\" specified for integer type\n", astr);
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
