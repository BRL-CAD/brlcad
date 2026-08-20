/**
 *                  U T I L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2017-2024 United States Government as represented by
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

/*                                                                               */
/* Synopsis of Utility Routines:                                                 */
/*                                                                               */
/* component_filter   Component item filter for the feature visit routine        */
/* creo_conv_to_mm    Extracts scale factor to convert model units to mm         */
/* creo_log           Report conversion status and log file messages             */
/* creo_model_units   Extracts Creo model units                                  */
/* creo_param_name    Returns first valid alpha-numeric parameter name string    */
/* creo_param_val     Extract parameter value from specified Creo model          */
/* find_btn_name      Returns position of radio button name                      */
/* find_control_attr  Returns location of control attribute from controls table  */
/* find_matl          Determine if specified material is on the material list    */
/* find_profile       Returns path of profile settings (.g) file                 */
/* find_unit_str      Returns location of unit string in units table             */
/* get_brlcad_name    Returns a unique BRL-CAD object name                       */
/* get_input_str      Returns input string from inputs table                     */
/* get_length_conv    Returns length unit conversion value from length table     */
/* get_mtl_input      Process input from specified material translation file     */
/* get_unit_abbr      Returns unit abbreviation from units table                 */
/* get_unit_sys       Returns unit system from units table                       */
/* get_username       Returns name of current user                               */
/* global_dir         Determines if database directory meets _GLOBAL criteria    */
/* load_defaults      Load default control settings into input panel             */
/* load_profile       Process input from user profile settings (.g) file         */
/* load_resource      Load user-supplied resource setting into input panel       */
/* lower_case         Converts string to lower case                              */
/* param_append       Append parameter to the array                              */
/* param_collect      Collect available parameters from the specified model      */
/* param_export       Export list of model parameters                            */
/* param_preserve     Preserve available model parameters                        */
/* params_to_attrs    Preserve a list of model-specific parameters as attributes */
/* parent_dir         Extract parent directory path                              */
/* parse_param_list   Parse list of user-supplied parameters                     */
/* PopupMsg           Display a message in a Creo dialog box                     */
/* regex_key          Utilize regular expression match for Creo parameter name   */
/* report_xform       Report current transformation matrix                       */
/* rgb4lmin           Modify RGB values to achieve minimum luminance threshold   */
/* scrub_vls          Removes unwanted characters from a variable-length string  */
/* set_radio_btn      Set radio button value in Creo UI panel                    */
/* stable_wchar       Map a string to the "stable" version found in parts/assems */
/* trim               Purge string of leading and trailing whitespace            */
/* util_fgets         fgets replacement function that also handles CR as an EOL  */
/* wstr_to_double     Convert wide string to double precision value              */
/* wstr_to_long       Convert wide string to long int value                      */
/*                                                                               */

#include "common.h"
#include <algorithm>
#include <regex.h>
#include "creo-brl.h"


/*---------------------------------------------------------------------*/
/*      Data structure to support Creo-BRL profile path lookups        */
/*---------------------------------------------------------------------*/

struct path_table
{
    char *pcmd;
    char *prel;
    int   indx;
};

/* Reference table for path lookups */
static path_table paths[] = {
    /*---------------------------------------------*/
    /*  command         relative path        index */
    /*---------------------------------------------*/
    {"CD"         , "."                     ,  1},
    {"UP"         , ".."                    ,  2},
    {"USERPROFILE", "Creo_to_BRL"           ,  3},
    {"HOMEDRIVE"  , "DevTools\\Creo_to_BRL" ,  4},
    { NULL        , NULL                    , -1}
};

/*---------------------------------------------------------------------*/
/*      Data structure to support Creo-BRL input panel processing      */
/*---------------------------------------------------------------------*/

struct inputs_table
{
    char *istr;
    int   indx;
}; 

/* Reference table for default input strings */
static inputs_table inputs[] = {
    /*--------------------------------------------------*/
    /*              input string                  index */
    /*--------------------------------------------------*/
    {"./ptc_materials.mtl"                       ,  1},
    {"0.0"                                       ,  2},
    {"0.3"                                       ,  3},
    {"0.5"                                       ,  4},
    {"1.0"                                       ,  5},
    {"1000"                                      ,  6},
    {"30"                                        ,  7},
    {"all/(debug)"                               ,  8},
    {"failure"                                   ,  9},
    {"failure/success"                           , 10},
    {"millimeter"                                , 11},
    {"nomenclature"                              , 12},
    {"nomenclature,part_number,ptc_material_name", 13},
    {"none"                                      , 14},
    {"off"                                       , 15},
    {"on"                                        , 16},
    {"percent"                                   , 17},
    {"success"                                   , 18},
    {"x_to_z"                                    , 19},
    {"y_to_z"                                    , 20},
    { NULL                                       , -1}
};

struct controls_table
{
    char *catr;
    char *cres;
    char *ctyp;
    int   cinp[MAX_RADIO_BTNS];
    int   indx;
};

/* Reference table for input panel controls */
static controls_table controls[] = {
    /*--------------------------------------------------------------------------*/
    /*        attribute               resource       type      inputs     index */
    /*--------------------------------------------------------------------------*/
    {"process_log_criteria"     , "log_file_type" , "RAD",  8,  9, 18, 10,  1},
    {"material_file_name"       , "mtl_fname"     , "STR",  1, -1, -1, -1,  2},
    {"create_object_names"      , "param_rename"  , "STR", 12, -1, -1, -1,  3},
    {"preserved_attributes"     , "param_save"    , "STR", 13, -1, -1, -1,  4},
    {"coordinate_transformation", "transform"     , "RAD", 14, 19, 20, -1,  5},
    {"initial_region_counter"   , "region_counter", "STR",  6, -1, -1, -1,  6},
    {"minimum_luminance"        , "min_luminance" , "STR",  7, -1, -1, -1,  7},
    {"chord_mode"               , "chord_mode"    , "RAD", 17, 11, -1, -1,  8},
    {"maximum_chord_height"     , "max_chord"     , "STR",  3, -1, -1, -1,  9},
    {"minimum_angle_control"    , "min_angle"     , "STR",  4, -1, -1, -1, 10},
    {"eliminate_small_features" , "elim_small"    , "BOX", 15, -1, -1, -1, 11},
    {"minimum_hole_diameter"    , "min_hole"      , "STR",  2, -1, -1, -1, 12},
    {"minimum_chamfer_dimension", "min_chamfer"   , "STR",  2, -1, -1, -1, 13},
    {"minimum_blend_radius"     , "min_round"     , "STR",  2, -1, -1, -1, 14},
    {"facetize_everything"      , "facets_only"   , "BOX", 16, -1, -1, -1, 15},
    {"export_facets_to_stl"     , "export_stl"    , "BOX", 15, -1, -1, -1, 16},
    {"reject_failed_bots"       , "check_solidity", "BOX", 15, -1, -1, -1, 17},
    {"box_replaces_failed_part" , "create_boxes"  , "BOX", 15, -1, -1, -1, 18},
    {"write_surface_normals"    , "write_normals" , "BOX", 15, -1, -1, -1, 19},
    { NULL                      ,  NULL           ,  NULL, -1, -1, -1, -1, -1}
};


/*----------------------------------------------------------------------*/
/*     Data structure to support Creo-BRL unit conversion processing    */
/*----------------------------------------------------------------------*/
/* Type           System                     Force       Mass   Length  */
/*----------------------------------------------------------------------*/
/*  FLT  Foot Pound Second (FPS)              lbf        slug     ft    */
/*  FLT  Inch Pound Second (IPS)              lbf        blob     in    */
/*  FLT  millimeter Newton Second (mmNs)       N        tonne     mm    */
/*  MLT  Centimeter Gram Second (CGS)         dyne        g       cm    */
/*  MLT  Inch lbm Second (Pro/E Default)  lbm-in/sec^2   lbm      in    */
/*  MLT  Meter Kilogram Second (MKS)           N         kg        m    */
/*  MLT  millimeter Kilogram Sec (mmKs)       mN         kg       mm    */
/*----------------------------------------------------------------------*/

struct length_table
{
    char  *ulen;
    char  *uopr;
    double ucnv;
};

/* Reference table for length conversions to inches */
static length_table length_conv[] = {
    /*--------------------------------------------*/
    /* length  operator  conversion      purpose  */
    /*--------------------------------------------*/
    {   "ft" ,   "x" ,     12.0   },  /* ft to in */
    {   "in" ,   "=",       1.0   },  /*   none   */
    {   "cm" ,   "/" ,      2.54  },  /* cm to in */
    {   "mm" ,   "/" ,     25.4   },  /* mm to in */
    {   "m"  ,   "/" ,      0.0254},  /*  m to in */
    {   NULL ,  NULL ,     -1.0   }
};

struct units_table
{
    char *usys;
    char *ustr;
    char *abbr;
    int   indx;
};

/* Reference table for equivalent units */
static units_table unit_equiv[] = {
    /*---------------------------------------------------------------*/
    /* system     unit string      abbreviation   index      system  */
    /*---------------------------------------------------------------*/
    {  "PTC" , "[lbm-in]/sec^2", "[lbm-in]/sec^2",  1},  /*   PTC    */
    {  "CGS" , "[g-cm]/sec^2"  , "dyne"          ,  2},  /*   CGS    */
    {  "FPS" , "[lbf-sec^2]/ft", "slug"          ,  3},  /*   FPS    */
    {  "IPS" , "[lbf-sec^2]/in", "blob"          ,  4},  /*   IPS    */
    {  "MKS" , "[kg-m]/sec^2"  , "N"             ,  5},  /*   MKS    */
    {  "MMKS", "[kg-mm]/sec^2" , "mN"            ,  6},  /*   MMKS   */
    {  "MMNS", "[N-sec^2]/mm"  , "tonne"         ,  7},  /*   MMNS   */
    {   NULL ,  NULL           , "---"           , -1}
};

/*----------------------------------------------------------------------*/
/*       Data structure to support Creo-BRl parameter processing        */
/*----------------------------------------------------------------------*/
struct pparam_data {
    struct creo_conv_info *cinfo;
    char *key;
    char *val;
};


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


/* Extracts scale factor to convert model units to mm */
extern "C" ProError
creo_conv_to_mm(double *scale, ProMdl model)
{
    ProError      err = PRO_TK_GENERAL_ERROR;
    ProUnitsystem unit_sys;
    ProUnititem   length;

    char   lstr[PRO_NAME_SIZE + 1];
    double conv_to_in = -1.0;

    if (!scale)
        return err;

    err = ProMdlPrincipalunitsystemGet(model, &unit_sys);
    if (err != PRO_TK_NO_ERROR)
        return err;

    err = ProUnitsystemUnitGet(&unit_sys, PRO_UNITTYPE_LENGTH, &length);
    if (err != PRO_TK_NO_ERROR)
        return err;

    ProWstringToString(lstr, length.name);

    conv_to_in = get_length_conv(lstr);
    if (conv_to_in < 0.0)
        err = PRO_TK_BAD_INPUTS;

    /* Convert from in to mm */
    (*scale) = abs(conv_to_in)*25.4;

    return err;
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
    ProStringToWstring(msgfil, CREO_BRL_MSG_FNAME);

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

        if (cinfo->curr_log_type == LOGGER_TYPE_NONE)
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
            case MSG_STATUS:
                break;
            case MSG_PLAIN:
                bu_vls_sprintf(&vmsg, "%s", msg);
                break;
            case MSG_ASSEM:
                bu_vls_sprintf(&vmsg, "  ASSEM: %s", msg);
                break;
            case MSG_COLOR:
                bu_vls_sprintf(&vmsg, "  COLOR: %s", msg);
                break;
            case MSG_FEAT:
                bu_vls_sprintf(&vmsg, "   FEAT: %s", msg);
                break;
            case MSG_FILE:
                bu_vls_sprintf(&vmsg, "   FILE: %s", msg);
                break;
            case MSG_MASS:
                bu_vls_sprintf(&vmsg, "   MASS: %s", msg);
                break;
            case MSG_MATL:
                bu_vls_sprintf(&vmsg, "   MATL: %s", msg);
                break;
            case MSG_MODEL:
                bu_vls_sprintf(&vmsg, "  MODEL: %s", msg);
                break;
            case MSG_NAME:
                bu_vls_sprintf(&vmsg, "   NAME: %s", msg);
                break;
            case MSG_PARAM:
                bu_vls_sprintf(&vmsg, "  PARAM: %s", msg);
                break;
            case MSG_PART:
                bu_vls_sprintf(&vmsg, "   PART: %s", msg);
                break;
            case MSG_SOLID:
                bu_vls_sprintf(&vmsg, "  SOLID: %s", msg);
                break;
            case MSG_STRING:
                bu_vls_sprintf(&vmsg, " STRING: %s", msg);
                break;
            case MSG_TESS:
                bu_vls_sprintf(&vmsg, "   TESS: %s", msg);
                break;
            case MSG_UNITS:
                bu_vls_sprintf(&vmsg, "  UNITS: %s", msg);
                break;
            case MSG_WARN:
                bu_vls_sprintf(&vmsg, "WARNING: %s", msg);
                break;
            default:
                bu_vls_sprintf(&vmsg, "  OTHER: %s", msg);
            }

        if ((cinfo->curr_msg_type == MSG_FAIL    && cinfo->curr_log_type != LOGGER_TYPE_SUCCESS)            ||
            (cinfo->curr_msg_type == MSG_SUCCESS && cinfo->curr_log_type != LOGGER_TYPE_FAILURE)            ||
            (cinfo->curr_msg_type != MSG_DEBUG   && cinfo->curr_log_type == LOGGER_TYPE_FAILURE_OR_SUCCESS) ||
            (cinfo->curr_log_type == LOGGER_TYPE_ALL)) {
            fprintf(cinfo->fplog, "%s", bu_vls_addr(&vmsg));
            fflush(cinfo->fplog);
        }
        bu_vls_free(&vmsg);
    }
}


/* Extracts Creo model units */
extern "C" ProError
creo_model_units(struct creo_conv_info *cinfo)
{
    ProError      err   = PRO_TK_GENERAL_ERROR;
    ProMdl        model = cinfo->curr_model;
    ProUnitsystem unit_sys;
    ProUnititem   angle, force, mass, length, time;

    /* Unit system flag, 1 => MLT or 2 => FLT */
    ProUnitsystemType unit_sys_type;

    char  astr[PRO_NAME_SIZE + 1];
    char  fstr[PRO_NAME_SIZE + 1];
    char  mstr[PRO_NAME_SIZE + 1];
    char  lstr[PRO_NAME_SIZE + 1];
    char  tstr[PRO_NAME_SIZE + 1];
    char   tmp[PRO_NAME_SIZE + 1];

    int loc = -1;

    err = ProMdlPrincipalunitsystemGet(model, &unit_sys);
    if (err != PRO_TK_NO_ERROR)
        return err;

    /* Extract the unit system type */
    err = ProUnitsystemTypeGet(&unit_sys, &unit_sys_type);
    if (err != PRO_TK_NO_ERROR)
        return err;

    /* Extract the angle units */
    err = ProUnitsystemUnitGet(&unit_sys, PRO_UNITTYPE_ANGLE, &angle);
    if (err != PRO_TK_NO_ERROR)
        return err;
    else
        ProWstringToString(astr, angle.name);

    if (unit_sys_type == PRO_UNITSYSTEM_MLT) {
        /* MLT: Force defined by:  mass, length, time */
        ProUnitsystemUnitGet(&unit_sys, PRO_UNITTYPE_MASS,   &mass);
        ProUnitsystemUnitGet(&unit_sys, PRO_UNITTYPE_LENGTH, &length);
        ProUnitsystemUnitGet(&unit_sys, PRO_UNITTYPE_TIME,   &time);
        ProWstringToString(mstr,   mass.name);
        ProWstringToString(lstr, length.name);
        ProWstringToString(tstr,   time.name);
        /* Substitute equivalent force units */
        sprintf(tmp,"[%s-%s]/%s^2", mstr, lstr, tstr);   /* [M-L]/t^2 */
        loc = find_unit_str(tmp);
        if (loc > 0) {
            bu_vls_sprintf(cinfo->unitsys, "%s", get_unit_sys(loc));
            bu_vls_sprintf(cinfo->funits , "%s", get_unit_abbr(loc));
        } else {
            bu_vls_sprintf(cinfo->unitsys, "%s", "Unknown");
            bu_vls_sprintf(cinfo->funits,  "%s", tmp);
        }
        /* Accept existing mass units */
        bu_vls_sprintf(cinfo->munits, "%s", mstr);
    } else {
        /* FLT: Mass defined by:  force, length, time */
        ProUnitsystemUnitGet(&unit_sys, PRO_UNITTYPE_FORCE,  &force);
        ProUnitsystemUnitGet(&unit_sys, PRO_UNITTYPE_LENGTH, &length);
        ProUnitsystemUnitGet(&unit_sys, PRO_UNITTYPE_TIME,   &time);
        ProWstringToString(fstr,  force.name);
        ProWstringToString(lstr, length.name);
        ProWstringToString(tstr,   time.name);
        /* Substitute equivalent mass units */
        sprintf(tmp,"[%s-%s^2]/%s", fstr, tstr, lstr);   /* [F-T^2]/L */
        loc = find_unit_str(tmp);
        if (loc > 0) {
            bu_vls_sprintf(cinfo->unitsys, "%s", get_unit_sys(loc));
            bu_vls_sprintf(cinfo->munits,  "%s", get_unit_abbr(loc));
        } else {
            bu_vls_sprintf(cinfo->unitsys, "%s", "Unknown");
            bu_vls_sprintf(cinfo->munits,  "%s", tmp);
        }
        /* Accept existing force units */
        bu_vls_sprintf(cinfo->funits, "%s", fstr);
    }

    /* Retain units for angle, length, time */
    bu_vls_sprintf(cinfo->aunits, "%s", astr);
    bu_vls_sprintf(cinfo->lunits, "%s", lstr);
    bu_vls_sprintf(cinfo->tunits, "%s", tstr);

    return PRO_TK_NO_ERROR;
}


/* Returns first valid alpha-numeric parameter name string */
extern "C" char *
creo_param_name(struct creo_conv_info *cinfo, wchar_t *creo_name, int flag)
{
    struct pparam_data pdata;
    pdata.cinfo = cinfo;
    pdata.val   = NULL;
    char *val   = NULL;

    ProMdl       model;
    ProModelitem mitm;

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


/* Returns position of radio button name */
extern "C" int
find_btn_name(const char *res, const char *val)
{
    int* p;

    if (val) {
        for (controls_table *p = controls; p->cres != NULL; p++)
            if (bu_strcmp(res, p->cres) == 0) {
                for (unsigned int n = 0; n < MAX_RADIO_BTNS; n++)
                    if (bu_strcmp(get_input_str(p->cinp[n]), val) == 0)
                        return n;
                }
        }

    return -1;
}


/* Returns location of control attribute from controls table */
extern "C" int
find_control_attr(const char *attr)
{
    int* p;

    for (controls_table *p = controls; p->catr != NULL; p++)
        if (bu_strcmp(attr, p->catr) == 0)
            return p->indx;

    return -1;
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


/* Returns path of profile settings (.g) file */
extern "C" struct bu_vls *
find_profile(void)
{
    static const char gfile[] = CREO_PROFILE_FNAME;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_vls *fname;

    char    dir[MAXPATHLEN];
    char   *path;
    ProPath cwd;

    if (ProDirectoryCurrentGet(cwd) != PRO_TK_NO_ERROR) {
        creo_log(NULL, MSG_STATUS, "Failed to find current working directory...");
        bu_vls_free(&str);
        return NULL;
    } else {
        ProWstringToString(dir, cwd);
        creo_log(NULL, MSG_STATUS, "Current directory is \"%s\"", dir);
    }

    for (path_table *p = paths; p->pcmd != NULL; p++) {
        if (bu_strcmp(p->pcmd, "CD") == 0 || bu_strcmp(p->pcmd, "UP") == 0) {
            if (bu_strcmp(p->prel, "..") == 0)
                parent_dir(dir);
            bu_vls_sprintf(&str, "%s%s%s", dir, "\\", gfile);
            if (bu_file_exists(bu_vls_cstr(&str), NULL))
                break;
        } else if ((path = getenv(p->pcmd)) != (char *)NULL) {
            bu_vls_sprintf(&str, "%s%s%s%s%s", path, "\\", p->prel, "\\", gfile);
            if (bu_file_exists(bu_vls_cstr(&str), NULL))
                break;
        } else {
            bu_vls_free(&str);
            return NULL;
        }
    }

    /* Prepare filename */
    BU_GET(fname, struct bu_vls);
    bu_vls_init(fname);
    bu_vls_sprintf(fname, "%s", bu_vls_addr(&str));

    /* Clean-up */
    bu_vls_free(&str);
    return fname;
}


/* Returns location of unit string in units table */
extern "C" int
find_unit_str(const char *name)
{
    int* p;

    for (units_table *p = unit_equiv; p->usys != NULL; p++)
        if (bu_strcmp(p->ustr, name) == 0)
            return p->indx;

    return -1;
}


/* Returns a unique BRL-CAD object name */
extern "C" struct bu_vls *
get_brlcad_name(struct creo_conv_info *cinfo, wchar_t *wname, const char *suffix, int flag)
{
    struct bu_vls  gname_root = BU_VLS_INIT_ZERO;
    struct bu_vls *gname;
    char *param_name = NULL;
    long count = 0;
    wchar_t *stable = stable_wchar(cinfo, wname);
    std::map<wchar_t *, struct bu_vls *, WStrCmp>::iterator n_it;
    std::map<wchar_t *, struct bu_vls *, WStrCmp> *nmap = NULL;
    std::set<struct bu_vls *, StrCmp> *nset = cinfo->brlcad_names;
    char astr[CREO_NAME_MAX];

    const char *keep_chars = "+-.=_";
    const char *collapse_chars = "_";

    ProWstringToString(astr, wname);
    lower_case(astr);

    if (!stable) {
        creo_log(cinfo, MSG_NAME, "No stable version of \"%s\" was found\n", astr);
        return NULL;
    }

    switch (flag) {
        case N_REGION:
            nmap = cinfo->region_name_map;
            creo_log(cinfo, MSG_NAME, "Region \"%s\"\n", astr);
            break;
        case N_ASSEM:
            nmap = cinfo->assem_name_map;
            creo_log(cinfo, MSG_NAME, "Assembly \"%s\"\n", astr);
            break;
        case N_SOLID:
            nmap = cinfo->solid_name_map;
            creo_log(cinfo, MSG_NAME, "Solid \"%s\"\n", astr);
            break;
        case N_CREO:
            nmap = cinfo->creo_name_map;
            nset = cinfo->creo_names;
            creo_log(cinfo, MSG_NAME, "Part \"%s\"\n", astr);
            break;
        default:
            return NULL;               /* Ignore unknown name type */
    }

    /* If we somehow don't have a map, bail */
    if (!nmap)
        return NULL;

    /* If we've already got something, return it. */
    n_it = nmap->find(wname);
    if (n_it != nmap->end()) {
        gname = n_it->second;
        return gname;
    }

    /* Nope - start generating */
    BU_GET(gname, struct bu_vls);
    bu_vls_init(gname);

    /* First try the parameters, if the user specified any */
    if (flag != N_CREO) {
        param_name = creo_param_name(cinfo, wname, flag);
        bu_vls_sprintf(&gname_root, "%s", param_name);
    }

    /* If we don't already have a name, use the Creo name */
    if (!param_name) {
        char val[CREO_NAME_MAX];
        ProWstringToString(val, wname);
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
                creo_log(cinfo, MSG_NAME, "Using \"%s\" to seek a unique object name\n",
                                          bu_vls_addr(&gname_root));
            else if (count >= MAX_UNIQUE_NAMES) {
                bu_vls_free(gname);
                BU_PUT(gname, struct bu_vls);
                creo_log(cinfo, MSG_NAME, "Failed with \"%s\" in name generation\n", astr);
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

    return gname;
}


/* Returns input string from inputs table */
extern "C" char*
get_input_str(int indx)
{
    int* p;

    for (inputs_table *p = inputs; p->istr != NULL; p++) {
        if (p->indx == indx)
            return p->istr;
    }

    return NULL;
}


/* Return length unit conversion value from length table */
extern "C" double
get_length_conv(const char *name)
{
    char   str[2] = {0};
    int*   p;
    double cnv = -1.0;

    for (length_table *p = length_conv; p->ulen != NULL; p++) {
        if (bu_strcmp(p->ulen, name) == 0) {
            sprintf(str, "%s", p->uopr);
            cnv = p->ucnv;
            break;
        }
    }

    if (bu_strcmp(str, "/") == 0)
        cnv = 1.0/cnv;

    return cnv;
}


/* Process input from specified material translation file */
extern "C" int
get_mtl_input(FILE *fpmtl, char *mtl_str, int *mtl_ids, int *mtl_los)
{
    const int  cols = MAX_MATL_NAME + 1;
    const char comments[] = "!@#$%^&*/<>?";
    char buf[MAX_LINE_BUFFER];
    char mtl[MAX_LINE_BUFFER];
    char firstc;
    int  id, los;
    int  recs;

    /* Initialize counter */
    recs = 0;
    while (util_fgets(buf, sizeof(buf), fpmtl) != NULL) {
        trim(buf);
        firstc = buf[0];
        if(strchr(comments, firstc)) {
            continue;                  /* skip comments */
        } else if (sscanf(buf, "%s%d%d", mtl, &id, &los) != 3) {
            continue;                  /* skip invalid input */
        } else if (recs < MAX_FILE_RECS) {
            for (int n = 0; mtl[n]; n++)
                mtl_str[recs*cols + n] = tolower(mtl[n]);
            mtl_ids[recs] = id;
            mtl_los[recs] = los;
            recs++;
        } else {
            mtl_str[recs*cols + 1] = '\0';
            break;
        }
    }
    return recs;
}


/* Returns unit abbreviation from units table */
extern "C" char*
get_unit_abbr(int index)
{
    int* p;

    for (units_table *p = unit_equiv; p->usys != NULL; p++) {
        if (p->indx == index)
            return p->abbr;
    }

    return NULL;
}


/* Return unit system from units table */
extern "C" char*
get_unit_sys(int index)
{
    int* p;

    for (units_table *p = unit_equiv; p->usys != NULL; p++) {
        if (p->indx == index)
            return p->usys;
    }

    return NULL;
}


/* Returns name of current user */
extern "C" char *
get_username(void)
{
    char *name;

    name = getenv("USERNAME");
    if (name)
        return name;
    else
        return "unknown";
}


/* Determines if database directory meets _GLOBAL criteria */
extern "C" int
global_dir(struct directory *dp)
{
    int major, minor;

    if (!dp)
        return 0;

    major = (dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY);
    minor = (dp->d_minor_type == 0);

    return (major && minor);
}


/* Load default control settings into input panel */
extern "C" void
load_defaults(void)
{
    int*  p;

    for (controls_table *p = controls; p->cres != NULL; p++) {
        char* def = get_input_str(p->cinp[0]);
        load_resource(p->cres, p->ctyp, def, def);
    }

    return;
}


/* Process input from user profile settings (.g) file */
extern "C" void
load_profile(void)
{
    struct bu_attribute_value_set avs;
    struct bu_vls    *profile = NULL;
    struct db_i      *dbip    = NULL;
    struct directory *dp      = NULL;
    struct rt_wdb    *wdbp    = NULL;

    FILE *fp = NULL;

    /* Locate the user profile */
    profile = find_profile();
    if (!profile) {
        creo_log(NULL, MSG_STATUS, "Unable to locate user profile");
        load_defaults();
        return;
        }

    creo_log(NULL, MSG_STATUS, "User profile is \"%s\"", bu_vls_addr(profile));

    /* Open the user profile */
    fp = fopen(bu_vls_cstr(profile), "rb");
    if (!fp) {
        creo_log(NULL, MSG_STATUS, "Unable to open \"%s\"", bu_vls_addr(profile));
        load_defaults();
        return;
        }

    /* Open the database */
    dbip = db_open(bu_vls_cstr(profile), DB_OPEN_READONLY);
    if (dbip == DBI_NULL) {
        creo_log(NULL, MSG_STATUS, "\"db_open\" failed to open the user profile");
        load_defaults();
        return;
        }

    /* Build the database */
    RT_CK_DBI(dbip);
    if (db_dirbuild(dbip) < 0) {
        creo_log(NULL, MSG_STATUS, "\"db_dirbuild\" failed to build \"%s\"", bu_vls_addr(profile));
        load_defaults();
        goto cleanup;
    }

    /* Display the title */
    if (dbip->dbi_title[0])
        creo_log(NULL, MSG_STATUS, "Database title is \"%s\"", dbip->dbi_title);

    /* Locate first available directory */
    FOR_ALL_DIRECTORY_START(dp, dbip) {
        if (global_dir(dp))
            break;
    } FOR_ALL_DIRECTORY_END

    /* Extract the _GLOBAL attributes */
    if (db5_get_attributes(dbip, &avs, dp)) {
        creo_log(NULL, MSG_STATUS, "Failed to find any _GLOBAL attributes");
        bu_avs_free(&avs);
        load_defaults();
        goto cleanup;
        }

    /* Load the _GLOBAL attributes */
    if (avs.count) {
        /* Echo the _GLOBAL attributes */
        creo_log(NULL, MSG_STATUS, "Found %d _GLOBAL attributes", avs.count);
        creo_log(NULL, MSG_STATUS, "=========================================================");
        creo_log(NULL, MSG_STATUS, " Index      Attribute Name                value");
                                  /* xxx   xxxxxxxxxxxxxxxxxxxxxxxxxx  xxxxxxxxxxxxxxxxxxxxx */
        creo_log(NULL, MSG_STATUS, "---------------------------------------------------------");

        for (unsigned int n = 0; n < avs.count; n++)
            creo_log(NULL, MSG_STATUS, " %3d   %-26s  %-s", n+1, avs.avp[n].name, avs.avp[n].value);

        creo_log(NULL, MSG_STATUS, "---------------------------------------------------------");

        /* Load attributes by panel resource name */
        int*  p;
        for (controls_table *p = controls; p->catr != NULL; p++) {
            const char* def = get_input_str(p->cinp[0]);
            const char* val = bu_avs_get(&avs, p->catr);
            if (val)
                load_resource(p->cres, p->ctyp, val, def);
            else
                load_resource(p->cres, p->ctyp, def, def);
        }
    bu_avs_free(&avs);
    }

cleanup:

    if (fp)
        fclose(fp);
    if (dbip)
        db_close(dbip);

    return;
}


/* Load user-supplied resource setting into input panel */
extern "C" void
load_resource(const char *res, const char *typ, const char *val, const char *def)
{
    wchar_t wstr[CREO_NAME_MAX];

    if (bu_strcmp(typ, "STR") == 0) {
        ProStringToWstring(wstr, (char *)val);
        ProUIInputpanelValueSet(CREO_UI_NAME, (char *)res, wstr);
    } else if (bu_strcmp(typ, "BOX") == 0) {
        if (bu_strcmp(val, "on") == 0)
            ProUICheckbuttonSet(CREO_UI_NAME, (char *)res);
        else
            ProUICheckbuttonUnset(CREO_UI_NAME, (char *)res);
    } else if (bu_strcmp(typ, "RAD") == 0) {
        if (find_btn_name(res, val) > 0)
            set_radio_btn((char *)res, (char *)val);
        else
            set_radio_btn((char *)res, (char *)def);
    } else
        creo_log(NULL, MSG_STATUS, "Unknown control type: \"%s\"", typ);

    return;
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
param_export(struct creo_conv_info *cinfo, const char *name)
{
    if (cinfo->obj_attr_params->size() > 0)
        for (unsigned int i = 0; i < cinfo->obj_attr_params->size(); i++) {
            char *attr_val = NULL;
            const char *arg = cinfo->obj_attr_params->at(i);
            creo_param_val(&attr_val, arg, cinfo->curr_model);
            if (attr_val) {
                bu_avs_add(&cinfo->avs, arg, attr_val);
                bu_free(attr_val, "value string");
                }
        }
    else
        (void) param_preserve(cinfo, name);

}


/* Preserve available model parameters */
extern "C" ProError
param_preserve(struct creo_conv_info *cinfo, const char *name)
{
    ProError      err = PRO_TK_GENERAL_ERROR;
    ProModelitem  mitm;
    ProParameter *pars;

    err = ProMdlToModelitem(cinfo->curr_model, &mitm);
    if (err != PRO_TK_NO_ERROR) {
        creo_log(cinfo, MSG_WARN, "Unable to get \"%s\" model item identifier\n", name);
        return err;
    }

    err = param_collect(&mitm, &pars);
    if (err == PRO_TK_BAD_INPUTS)
        creo_log(cinfo, MSG_WARN, "Invalid inputs for \"%s\" parameter collection\n", name);
    else if (err != PRO_TK_NO_ERROR)
        creo_log(cinfo, MSG_WARN, "Unable to collect \"%s\" model parameters\n", name);
    else {
        err = params_to_attrs(cinfo, pars);
        if (err == PRO_TK_BAD_INPUTS)
            creo_log(cinfo, MSG_WARN, "Invalid inputs for \"%s\" attribute creation\n", name);
        else if (err == PRO_TK_NOT_EXIST)
            creo_log(cinfo, MSG_WARN, "No parameters for \"%s\" are available\n", name);
    }

    (void)ProArrayFree((ProArray*)&pars);
    return err;
}


/* Preserve a list of model-specific parameters as attributes */
extern "C" ProError
params_to_attrs(struct creo_conv_info *cinfo, ProParameter* pars)
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
        creo_param_val(&attr_val, attr_nam, cinfo->curr_model);

        if (attr_val) {
            found++;
            if (found == 1) {
                creo_log(cinfo, MSG_PARAM, "==========================================================\n");
                creo_log(cinfo, MSG_PARAM, "   n         ptc_parameter_name               value\n");
                                          /* xxx  xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx  xxxxxxxxxxxxxxxxxxxxx */
                creo_log(cinfo, MSG_PARAM, "----------------------------------------------------------\n");
            }
            creo_log(cinfo, MSG_PARAM,     " %3d  %-32s  %-s\n", found, attr_nam, attr_val);
            bu_avs_add(&cinfo->avs, attr_nam, attr_val);
            bu_free(attr_val, "value string");
            }
    }

    if (found > 0) {
        creo_log(cinfo, MSG_PARAM,     "----------------------------------------------------------\n");
        creo_log(cinfo, MSG_PARAM,     "Processed %d model parameters\n",      count);
        if (found == 1)
            creo_log(cinfo, MSG_PARAM, "Extracted a single non-empty parameter\n");
        else
            creo_log(cinfo, MSG_PARAM, "Extracted %d non-empty parameters \n", found);
        if (count > found)
            creo_log(cinfo, MSG_PARAM, "Extraction ratio: %.1f%s\n",
                                        double(found)/double(count)*100.0, "%");
        else
            creo_log(cinfo, MSG_PARAM, "Extraction ratio: 100%s\n", "%");
    } else
        creo_log(cinfo, MSG_PARAM,     "No model parameters were found\n");

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


/* Extract parent directory path */
extern "C" void
parent_dir(char *dir)
{
    int n;

    n = int(strlen(dir)) - 1;
    if (dir[n] == '\\')
        dir[n] = '\0';

    for (n = int(strlen(dir)) - 1; n > 2; n--) {
        if (dir[n] == '\\')
            break;
        else
            dir[n] = '\0';
    }

    return;
}


/* Display a message in a Creo dialog box */
extern "C" ProError
PopupMsg(const char *title, const char *msg)
{
    wchar_t wtitle[CREO_NAME_MAX];
    wchar_t wmsg[CREO_MSG_MAX];
    ProUIMessageButton* button = NULL;
    ProUIMessageButton bresult;

    (void)ProArrayAlloc(1, sizeof(ProUIMessageButton), 1, (ProArray*)&button);
    button[0] = PRO_UI_MESSAGE_OK;
    ProStringToWstring(wtitle, (char *)title);
    ProStringToWstring(wmsg, (char *)msg);
    ProUIMessageDialogDisplay(PROUIMESSAGE_INFO, wtitle, wmsg, button, PRO_UI_MESSAGE_OK, &bresult);
    (void)ProArrayFree((ProArray*)&button);

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


/* Report current transformation matrix */
extern "C" void
report_xform(struct creo_conv_info *cinfo, const char *name)
{
    /* Log the current xform matrix */
    creo_log(cinfo, MSG_PART, "----------------------------------------------------------\n");
    creo_log(cinfo, MSG_PART, "         Transformation matrix for \"%s\"\n", name);
    creo_log(cinfo, MSG_PART, "----------------------------------------------------------\n");
    for (int i = 0; i < 4; i++)
        creo_log(cinfo, MSG_PART, "%13.6f  %13.6f  %13.6f  %13.6f\n",
                                   cinfo->curr_xform[i][0],
                                   cinfo->curr_xform[i][1],
                                   cinfo->curr_xform[i][2],
                                   cinfo->curr_xform[i][3]);
    creo_log(cinfo, MSG_PART, "------------------------------------------\n");
}


/* Modify RGB values to achieve minimum luminance threshold */
extern "C" int
rgb4lmin(double *rgb, int lmin)
{
    /* employs macro-defined function:
     *   #define dmod(a,b) ((a) - floor(double((a))/double((b)))*(b))
     */

    int r,g,b;
    int rp,gp,bp,lp;
    int cmin,cmax;

    double del,hue,sat,lum;
    double cp,hp,mp,xp;
    double rf,gf,bf;

    if (UNLIKELY(!rgb))                          /* bad input? */
        return -1;

    /* Scale fractional rgb to 255 */
    r = lrint(rgb[0]*255.0);
    g = lrint(rgb[1]*255.0);
    b = lrint(rgb[2]*255.0);

    /* Restrict input range for: rgb, lmin */
    rp = (r    < 0) ? 0 : ((r    > 255) ? 255 : r);
    gp = (g    < 0) ? 0 : ((g    > 255) ? 255 : g);
    bp = (b    < 0) ? 0 : ((b    > 255) ? 255 : b);
    lp = (lmin < 0) ? 0 : ((lmin > 100) ? 100 : lmin);

    cmax = (rp < gp) ? ((bp < gp) ? gp : bp) : ((bp < rp) ? rp : bp);
    cmin = (rp > gp) ? ((bp > gp) ? gp : bp) : ((bp > rp) ? rp : bp);

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
    rgb[0] = ((rf + mp) < 0.0) ? 0.0 : (((rf + mp) > 1.0) ? 1.0 : (rf + mp));
    rgb[1] = ((gf + mp) < 0.0) ? 0.0 : (((gf + mp) > 1.0) ? 1.0 : (gf + mp));
    rgb[2] = ((bf + mp) < 0.0) ? 0.0 : (((bf + mp) > 1.0) ? 1.0 : (bf + mp));

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


/* Set radio button value in Creo UI panel */
extern "C" int
set_radio_btn(char *group, char *name)
{
    ProError err = PRO_TK_GENERAL_ERROR;

    err = ProUIRadiogroupSelectednamesSet(CREO_UI_NAME, group, 1, &name);
    if (err != PRO_TK_NO_ERROR)
        creo_log(NULL,  MSG_STATUS, "FAILURE: Unable to set radio button choice: \"%s\"", name);

    return (err == PRO_TK_NO_ERROR) ? 1 : 0;
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


/* fgets replacement function that also handles CR as an EOL  */
char *
util_fgets(char *s, int size, FILE *stream)
{
    int totBytesRead = 0;
    int isEOF = 0;

    /* if we are not asked to or can't read anything, just return */
    if (UNLIKELY(size < 1 || !s)) {
        return s;
    }

    /* if the buffer size is one, we have no space (we add a null)
     * so just return
     */
    if (UNLIKELY(size == 1)) {
        *s = '\0';
        return s;
    }

    /* check for EOF or error */
    if (UNLIKELY(!stream) || feof(stream) || ferror(stream)) {
        return (char *)NULL;
    }

    /* actually do some reading */
    while (totBytesRead < size - 1) {
        int c;

        c = fgetc(stream);
        if (c == EOF) {
            isEOF = 1;
            break;
        }

        s[totBytesRead++] = c;

        /* check for newline */
        if (c == '\n') {
            break;
        }

        /* chech for CR */
        if (c == '\r') {

            /* check for CR/LF combination */
            c = fgetc(stream);
            if (c != '\n') {
                /* not a CR/LF, so unget the last char */
                ungetc(c, stream);
            }

            break;
        }
    }

    /* add our null */
    s[totBytesRead] = '\0';

    if (isEOF && totBytesRead == 0)
        return (char *)NULL;
    else
        return s;
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
        creo_log(cinfo, MSG_STRING, "Invalid data \"%s\" specified for type double\n", astr);
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
        creo_log(cinfo, MSG_STRING, " Invalid data \"%s\" specified for integer type\n", astr);
        return -1;
    }
    return ret;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
