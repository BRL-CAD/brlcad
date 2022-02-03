/*                      A N A L Y Z E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file analyze.cpp
 *
 * The analyze command
 *
 */

#include "common.h"

extern "C" {
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
}

#include <limits>

extern "C" {
#include "bu/cmd.h"
#include "bu/opt.h"
#include "../ged_private.h"
#include "./ged_analyze.h"
}

#define DB_SOLID INT_MAX
#define DB_NON_SOLID INT_MAX - 1


#define HELPFLAG "--print-help"
#define PURPOSEFLAG "--print-purpose"

struct _ged_analyze_info {
    struct ged *gedp = NULL;
    const struct bu_cmdtab *cmds = NULL;
    struct bu_opt_desc *gopts = NULL;
    int verbosity = 0;
    std::map<std::pair<int, int>, op_func_ptr> *union_map;
    std::map<std::pair<int, int>, op_func_ptr> *isect_map;
    std::map<std::pair<int, int>, op_func_ptr> *subtr_map;
};


static struct _ged_analyze_info *
_analyze_info_create()
{
    struct _ged_analyze_info *gc = new struct _ged_analyze_info;
    gc->verbosity = 0;
    gc->union_map = new std::map<std::pair<int, int>, op_func_ptr>;
    gc->isect_map = new std::map<std::pair<int, int>, op_func_ptr>;
    gc->subtr_map = new std::map<std::pair<int, int>, op_func_ptr>;


    // Populate the maps with known pair analysis functions
    (*gc->union_map)[std::make_pair(DB5_MINORTYPE_BRLCAD_PNTS, DB_SOLID)] = op_pnts_vol;
    (*gc->isect_map)[std::make_pair(DB5_MINORTYPE_BRLCAD_PNTS, DB_SOLID)] = op_pnts_vol;
    (*gc->subtr_map)[std::make_pair(DB5_MINORTYPE_BRLCAD_PNTS, DB_SOLID)] = op_pnts_vol;

    return gc;
}

static void
_analyze_info_destroy(struct _ged_analyze_info *s)
{
    delete s->union_map;
    delete s->isect_map;
    delete s->subtr_map;
    delete s;
}

static bool
db_solid_type(int type)
{
    switch (type) {
	case DB5_MINORTYPE_BRLCAD_ARB8:
	case DB5_MINORTYPE_BRLCAD_ARBN:
	case DB5_MINORTYPE_BRLCAD_ARS:
	case DB5_MINORTYPE_BRLCAD_BOT:
	case DB5_MINORTYPE_BRLCAD_BREP:
	case DB5_MINORTYPE_BRLCAD_BSPLINE:
	case DB5_MINORTYPE_BRLCAD_CLINE:
	case DB5_MINORTYPE_BRLCAD_COMBINATION:
	case DB5_MINORTYPE_BRLCAD_DSP:
	case DB5_MINORTYPE_BRLCAD_EBM:
	case DB5_MINORTYPE_BRLCAD_EHY:
	case DB5_MINORTYPE_BRLCAD_ELL:
	case DB5_MINORTYPE_BRLCAD_EPA:
	case DB5_MINORTYPE_BRLCAD_ETO:
	case DB5_MINORTYPE_BRLCAD_EXTRUDE:
	case DB5_MINORTYPE_BRLCAD_HALF:
	case DB5_MINORTYPE_BRLCAD_HF:
	case DB5_MINORTYPE_BRLCAD_HRT:
	case DB5_MINORTYPE_BRLCAD_HYP:
	case DB5_MINORTYPE_BRLCAD_METABALL:
	case DB5_MINORTYPE_BRLCAD_NMG:
	case DB5_MINORTYPE_BRLCAD_PARTICLE:
	case DB5_MINORTYPE_BRLCAD_PIPE:
	case DB5_MINORTYPE_BRLCAD_POLY:
	case DB5_MINORTYPE_BRLCAD_REC:
	case DB5_MINORTYPE_BRLCAD_REVOLVE:
	case DB5_MINORTYPE_BRLCAD_RHC:
	case DB5_MINORTYPE_BRLCAD_RPC:
	case DB5_MINORTYPE_BRLCAD_SKETCH:
	case DB5_MINORTYPE_BRLCAD_SPH:
	case DB5_MINORTYPE_BRLCAD_SUBMODEL:
	case DB5_MINORTYPE_BRLCAD_SUPERELL:
	case DB5_MINORTYPE_BRLCAD_TGC:
	case DB5_MINORTYPE_BRLCAD_TOR:
	case DB5_MINORTYPE_BRLCAD_VOL:
	    return true;
	case DB5_MINORTYPE_BRLCAD_ANNOT:
	case DB5_MINORTYPE_BRLCAD_CONSTRAINT:
	case DB5_MINORTYPE_BRLCAD_DATUM:
	case DB5_MINORTYPE_BRLCAD_GRIP:
	case DB5_MINORTYPE_BRLCAD_JOINT:
	case DB5_MINORTYPE_BRLCAD_PNTS:
	case DB5_MINORTYPE_BRLCAD_SCRIPT:
	default:
	    return false;
    };
}

static op_func_ptr
_analyze_find_processor(struct _ged_analyze_info *s, db_op_t op, int t1, int t2)
{
    int type1 = t1;
    int type2 = t2;
    std::map<std::pair<int, int>, op_func_ptr> *omap;
    switch (op) {
	case DB_OP_UNION:
	    omap = s->union_map;
	    break;
	case DB_OP_INTERSECT:
	    omap = s->isect_map;
	    break;
	case DB_OP_SUBTRACT:
	    omap = s->subtr_map;
	    break;
	default:
	    return NULL;
    }

    if (omap->find(std::make_pair(type1, type2)) != omap->end()) {
	return (*omap)[std::make_pair(t1, t2)];
    }

    // If there isn't a specific type, see if there's a generic match for t2
    type2 = (db_solid_type(t2)) ? DB_SOLID : DB_NON_SOLID;
    if (omap->find(std::make_pair(type1, type2)) != omap->end()) {
	return (*omap)[std::make_pair(type1, type2)];
    }

    // If there isn't a specific type, see if there's a generic match for t1
    type1 = (db_solid_type(t1)) ? DB_SOLID : DB_NON_SOLID;
    type2 = t2;
    if (omap->find(std::make_pair(type1, type2)) != omap->end()) {
	return (*omap)[std::make_pair(type1, type2)];
    }
    // If there isn't a match, see if there's a generic match for t1 and t2
    type1 = (db_solid_type(t1)) ? DB_SOLID : DB_NON_SOLID;
    type2 = (db_solid_type(t2)) ? DB_SOLID : DB_NON_SOLID;
    if (omap->find(std::make_pair(type1, type2)) != omap->end()) {
	return (*omap)[std::make_pair(type1, type2)];
    }

    // Nope, nothing
    return NULL;
}

static int
_analyze_cmd_msgs(void *cs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_analyze_info *gc = (struct _ged_analyze_info *)cs;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gc->gedp->ged_result_str, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gc->gedp->ged_result_str, "%s\n", ps);
	return 1;
    }
    return 0;
}

/* Analyze solid in internal form.
 * TODO - this switch table probably indicates this should
 * be a functab callback... */
/**
 * TODO: primitives that still need implementing
 * ehy
 * metaball
 * nmg
 */
static void
analyze_do_summary(struct ged *gedp, const struct rt_db_internal *ip)
{
    /* XXX Could give solid name, and current units, here */

    switch (ip->idb_type) {

	case ID_ARB8:
	    analyze_arb8(gedp, ip);
	    break;

	case ID_BOT:
	    analyze_general(gedp, ip);
	    break;

	case ID_ARBN:
	    analyze_arbn(gedp, ip);
	    break;

	case ID_ARS:
	    analyze_ars(gedp, ip);
	    break;

	case ID_TGC:
	    analyze_general(gedp, ip);
	    break;

	case ID_ELL:
	    analyze_general(gedp, ip);
	    break;

	case ID_TOR:
	    analyze_general(gedp, ip);
	    break;

	case ID_RPC:
	    analyze_general(gedp, ip);
	    break;

	case ID_ETO:
	    analyze_general(gedp, ip);
	    break;

	case ID_EPA:
	    analyze_general(gedp, ip);
	    break;

	case ID_PARTICLE:
	    analyze_general(gedp, ip);
	    break;

	case ID_SUPERELL:
	    analyze_superell(gedp, ip);
	    break;

	case ID_SKETCH:
	    analyze_sketch(gedp, ip);
	    break;

	case ID_HYP:
	    analyze_general(gedp, ip);
	    break;

	case ID_PIPE:
	    analyze_general(gedp, ip);
	    break;

	case ID_VOL:
	    analyze_general(gedp, ip);
	    break;

	 case ID_EXTRUDE:
	    analyze_general(gedp, ip);
	    break;

	 case ID_RHC:
	    analyze_general(gedp, ip);
	    break;

	default:
	    bu_vls_printf(gedp->ged_result_str, "\nanalyze: unable to process %s solid\n",
			  OBJ[ip->idb_type].ft_name);
	    break;
    }
}

extern "C" int
_analyze_cmd_summarize(void *bs, int argc, const char **argv)
{
    const char *usage_string = "analyze [options] summarize obj1 <obj2 ...>";
    const char *purpose_string = "Summary of analytical information about listed objects";
    if (_analyze_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_analyze_info *gc = (struct _ged_analyze_info *)bs;
    struct ged *gedp = gc->gedp;
    struct rt_db_internal intern;

    argc--; argv++;
    if (!argc) {
	bu_vls_printf(gc->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* use the names that were input */
    for (int i = 0; i < argc; i++) {
	struct directory *ndp = db_lookup(gedp->dbip,  argv[i], LOOKUP_NOISY);
	if (ndp == RT_DIR_NULL)
	    continue;

	GED_DB_GET_INTERNAL(gedp, &intern, ndp, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);

	_ged_do_list(gedp, ndp, 1);
	analyze_do_summary(gedp, &intern);
	rt_db_free_internal(&intern);
    }

    return BRLCAD_OK;
}

static void
clear_obj(struct ged *gedp, const char *name)
{
    struct bu_vls tmpstr = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&tmpstr, "%s", bu_vls_cstr(gedp->ged_result_str));
    const char *av[4];
    av[0] = "kill";
    av[1] = "-f";
    av[2] = "-q";
    av[3] = name;
    ged_kill(gedp, 4, (const char **)av);
    bu_vls_sprintf(gedp->ged_result_str, "%s", bu_vls_cstr(&tmpstr));
}


static void
mv_obj(struct ged *gedp, const char *n1, const char *n2)
{
    clear_obj(gedp, n2);
    struct bu_vls tmpstr = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&tmpstr, "%s", bu_vls_cstr(gedp->ged_result_str));
    const char *av[3];
    av[0] = "mv";
    av[1] = n1;
    av[2] = n2;
    ged_move(gedp, 3, (const char **)av);
    bu_vls_sprintf(gedp->ged_result_str, "%s", bu_vls_cstr(&tmpstr));
}

extern "C" int
_analyze_cmd_intersect(void *bs, int argc, const char **argv)
{
    const char *usage_string = "analyze [options] intersect [-o out_obj] obj1 obj2 <...>";
    const char *purpose_string = "Intersect obj1 with obj2 and any subsequent objs";
    if (_analyze_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_analyze_info *gc = (struct _ged_analyze_info *)bs;
    struct ged *gedp = gc->gedp;

    argc--; argv++;
    if (!argc) {
	bu_vls_printf(gc->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    // See if we are going to output an object
    int help = 0;
    struct bu_vls oname = BU_VLS_INIT_ZERO;
    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",    "",      NULL,        &help,  "Print help");
    BU_OPT(d[1], "o", "output",  "name",  &bu_opt_vls, &oname, "Specify output object");
    BU_OPT_NULL(d[2]);

    int ac = bu_opt_parse(NULL, argc, argv, d);
    if (help) {
	bu_vls_printf(gc->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_HELP;
    }
    if (ac < 2) {
	bu_vls_printf(gc->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_HELP;
    }
    argc = ac;

    if (bu_vls_strlen(&oname)) {
	struct directory *dp_out = db_lookup(gedp->dbip, bu_vls_cstr(&oname), LOOKUP_QUIET);
	if (dp_out != RT_DIR_NULL) {
	    bu_vls_sprintf(gedp->ged_result_str, "specified output object %s already exists.\n", bu_vls_cstr(&oname));
	    bu_vls_free(&oname);
	    return BRLCAD_ERROR;
	}
    }

    long ret = 0;
    const char *tmpname = "___analyze_cmd_intersect_tmp_obj__";
    struct directory *dp1 = db_lookup(gedp->dbip, argv[0], LOOKUP_NOISY);
    struct directory *dp2 = db_lookup(gedp->dbip, argv[1], LOOKUP_NOISY);
    op_func_ptr of = _analyze_find_processor(gc, DB_OP_INTERSECT, dp1->d_minor_type, dp2->d_minor_type);
    if (!of) {
	bu_vls_sprintf(gedp->ged_result_str, "Unsupported type pairing\n");
	bu_vls_free(&oname);
	return BRLCAD_ERROR;
    }
    clear_obj(gc->gedp, tmpname);
    ret = (*of)(tmpname, gc->gedp,  DB_OP_INTERSECT, argv[0], argv[1]);
    if (ret == -1) {
	clear_obj(gc->gedp, tmpname);
	bu_vls_free(&oname);
	return BRLCAD_ERROR;
    }

    if (argc > 2) {
	const char *tmpname2 = "___analyze_cmd_intersect_tmp_obj_2__";
	for (int i = 2; i < argc; i++) {
	    const char *n1 = tmpname;
	    const char *n2 = argv[i];
	    dp1 = db_lookup(gedp->dbip, n1, LOOKUP_NOISY);
	    dp2 = db_lookup(gedp->dbip, n2, LOOKUP_NOISY);
	    of = _analyze_find_processor(gc, DB_OP_INTERSECT, dp1->d_minor_type, dp2->d_minor_type);
	    if (!of) {
		bu_vls_sprintf(gedp->ged_result_str, "Unsupported type pairing\n");
		clear_obj(gc->gedp, tmpname);
		clear_obj(gc->gedp, tmpname2);
		bu_vls_free(&oname);
		return BRLCAD_ERROR;
	    }
	    ret = (*of)(tmpname2, gc->gedp,  DB_OP_INTERSECT, n1, n2);
	    mv_obj(gc->gedp, tmpname2, tmpname);
	    if (ret == -1) {
		clear_obj(gc->gedp, tmpname);
		clear_obj(gc->gedp, tmpname2);
		bu_vls_free(&oname);
		return BRLCAD_ERROR;
	    }
	}
    }

    if (bu_vls_strlen(&oname)) {
	mv_obj(gc->gedp, tmpname, bu_vls_cstr(&oname));
    }

    clear_obj(gc->gedp, tmpname);

    bu_vls_sprintf(gedp->ged_result_str, "%ld\n", ret);
    bu_vls_free(&oname);

    return BRLCAD_OK;
}

extern "C" int
_analyze_cmd_subtract(void *bs, int argc, const char **argv)
{
    const char *usage_string = "analyze [options] subtract [-o out_obj] obj1 obj2 <...>";
    const char *purpose_string = "Subtract obj2 (and any subsequent objects) from obj1";
    if (_analyze_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_analyze_info *gc = (struct _ged_analyze_info *)bs;
    struct ged *gedp = gc->gedp;

    argc--; argv++;
    if (!argc) {
	bu_vls_printf(gc->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    // See if we are going to output an object
    int help = 0;
    struct bu_vls oname = BU_VLS_INIT_ZERO;
    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",    "",      NULL,        &help,  "Print help");
    BU_OPT(d[1], "o", "output",  "name",  &bu_opt_vls, &oname, "Specify output object");
    BU_OPT_NULL(d[2]);

    int ac = bu_opt_parse(NULL, argc, argv, d);
    if (help) {
	bu_vls_printf(gc->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_HELP;
    }
    if (ac < 2) {
	bu_vls_printf(gc->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_HELP;
    }
    argc = ac;

    if (bu_vls_strlen(&oname)) {
	struct directory *dp_out = db_lookup(gedp->dbip, bu_vls_cstr(&oname), LOOKUP_QUIET);
	if (dp_out != RT_DIR_NULL) {
	    bu_vls_sprintf(gedp->ged_result_str, "specified output object %s already exists.\n", bu_vls_cstr(&oname));
	    bu_vls_free(&oname);
	    return BRLCAD_ERROR;
	}
    }

    long ret = 0;
    const char *tmpname = "___analyze_cmd_subtract_tmp_obj__";
    struct directory *dp1 = db_lookup(gedp->dbip, argv[0], LOOKUP_NOISY);
    struct directory *dp2 = db_lookup(gedp->dbip, argv[1], LOOKUP_NOISY);
    op_func_ptr of = _analyze_find_processor(gc, DB_OP_SUBTRACT, dp1->d_minor_type, dp2->d_minor_type);
    if (!of) {
	bu_vls_sprintf(gedp->ged_result_str, "Unsupported type pairing\n");
	bu_vls_free(&oname);
	return BRLCAD_ERROR;
    }
    clear_obj(gc->gedp, tmpname);
    ret = (*of)(tmpname, gc->gedp,  DB_OP_SUBTRACT, argv[0], argv[1]);
    if (ret == -1) {
	clear_obj(gc->gedp, tmpname);
	bu_vls_free(&oname);
	return BRLCAD_ERROR;
    }

    if (argc > 2) {
	const char *tmpname2 = "___analyze_cmd_subtract_tmp_obj_2__";
	for (int i = 2; i < argc; i++) {
	    const char *n1 = tmpname;
	    const char *n2 = argv[i];
	    dp1 = db_lookup(gedp->dbip, n1, LOOKUP_NOISY);
	    dp2 = db_lookup(gedp->dbip, n2, LOOKUP_NOISY);
	    of = _analyze_find_processor(gc, DB_OP_SUBTRACT, dp1->d_minor_type, dp2->d_minor_type);
	    if (!of) {
		bu_vls_sprintf(gedp->ged_result_str, "Unsupported type pairing\n");
		clear_obj(gc->gedp, tmpname);
		clear_obj(gc->gedp, tmpname2);
		bu_vls_free(&oname);
		return BRLCAD_ERROR;
	    }
	    ret = (*of)(tmpname2, gc->gedp,  DB_OP_SUBTRACT, n1, n2);
	    mv_obj(gc->gedp, tmpname2, tmpname);
	    if (ret == -1) {
		clear_obj(gc->gedp, tmpname);
		clear_obj(gc->gedp, tmpname2);
		bu_vls_free(&oname);
		return BRLCAD_ERROR;
	    }
	}
    }

    if (bu_vls_strlen(&oname)) {
	mv_obj(gc->gedp, tmpname, bu_vls_cstr(&oname));
    }

    clear_obj(gc->gedp, tmpname);

    bu_vls_sprintf(gedp->ged_result_str, "%ld\n", ret);
    bu_vls_free(&oname);

    return BRLCAD_OK;
}

extern "C" int
_analyze_cmd_help(void *bs, int argc, const char **argv)
{
    struct _ged_analyze_info *gc = (struct _ged_analyze_info *)bs;
    if (!argc || !argv || BU_STR_EQUAL(argv[0], "help")) {
	bu_vls_printf(gc->gedp->ged_result_str, "analyze [options] subcommand [args]\n");
	if (gc->gopts) {
	    char *option_help = bu_opt_describe(gc->gopts, NULL);
	    if (option_help) {
		bu_vls_printf(gc->gedp->ged_result_str, "Options:\n%s\n", option_help);
		bu_free(option_help, "help str");
	    }
	}
	bu_vls_printf(gc->gedp->ged_result_str, "Available subcommands:\n");
	const struct bu_cmdtab *ctp = NULL;
	int ret;
	const char *helpflag[2];
	helpflag[1] = PURPOSEFLAG;
	size_t maxcmdlen = 0;
	for (ctp = gc->cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    maxcmdlen = (maxcmdlen > strlen(ctp->ct_name)) ? maxcmdlen : strlen(ctp->ct_name);
	}
	for (ctp = gc->cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    bu_vls_printf(gc->gedp->ged_result_str, "  %s%*s", ctp->ct_name, (int)(maxcmdlen - strlen(ctp->ct_name)) +   2, " ");
	    if (!BU_STR_EQUAL(ctp->ct_name, "help")) {
		helpflag[0] = ctp->ct_name;
		bu_cmd(gc->cmds, 2, helpflag, 0, (void *)gc, &ret);
	    } else {
		bu_vls_printf(gc->gedp->ged_result_str, "print help and exit\n");
	    }
	}
    } else {
	int ret;
	const char **helpargv = (const char **)bu_calloc(argc+1, sizeof(char *), "help argv");
	helpargv[0] = argv[0];
	helpargv[1] = HELPFLAG;
	for (int i = 1; i < argc; i++) {
	    helpargv[i+1] = argv[i];
	}
	bu_cmd(gc->cmds, argc+1, helpargv, 0, (void *)gc, &ret);
	bu_free(helpargv, "help argv");
	return ret;
    }

    return BRLCAD_OK;
}


const struct bu_cmdtab _analyze_cmds[] = {
      { "summarize",           _analyze_cmd_summarize},
      { "intersect",           _analyze_cmd_intersect},
      { "subtract",            _analyze_cmd_subtract},
      { (char *)NULL,      NULL}
  };


extern "C" int
ged_analyze_core(struct ged *gedp, int argc, const char *argv[])
{
    int help = 0;
    struct _ged_analyze_info *gc = _analyze_info_create();
    gc->gedp = gedp;
    gc->cmds = _analyze_cmds;

    // Sanity
    if (UNLIKELY(!gedp || !argc || !argv)) {
	_analyze_info_destroy(gc);
	return BRLCAD_ERROR;
    }

    // Clear results
    bu_vls_trunc(gedp->ged_result_str, 0);

    // See if we have any high level options set
    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",    "",  NULL, &help,          "Print help");
    BU_OPT(d[1], "v", "verbose", "",  NULL, &gc->verbosity, "Verbose output");
    BU_OPT_NULL(d[2]);

    gc->gopts = d;

    if (argc == 1) {
	_analyze_cmd_help(gc, 0, NULL);
	_analyze_info_destroy(gc);
	return BRLCAD_OK;
    }

    // High level options are only defined prior to the subcommand
    int cmd_pos = -1;
    for (int i = 1; i < argc; i++) {
	if (bu_cmd_valid(_analyze_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;

    bu_opt_parse(NULL, acnt, argv, d);

    if (help) {
	if (cmd_pos >= 0) {
	    argc = argc - cmd_pos;
	    argv = &argv[cmd_pos];
	    _analyze_cmd_help(gc, argc, argv);
	} else {
	    _analyze_cmd_help(gc, 0, NULL);
	}
	_analyze_info_destroy(gc);
	return BRLCAD_OK;
    }


    // Jump the processing past any options specified. If we don't have a
    // subcommand, assume all args are geometry objects and the command mode is
    // summarize. This will get us the old behavior, except in the case where
    // we happen to have an object with a name that matches a subcommand of
    // analyze.  In that case, the full "analyze summarize objname" is needed.
    const char *scmd = "summarize";
    if (cmd_pos != -1) {
	argc = argc - cmd_pos;
	argv = &argv[cmd_pos];
    } else {
	argv[0] = scmd;
    }

    int ret;
    if (bu_cmd(_analyze_cmds, argc, argv, 0, (void *)gc, &ret) == BRLCAD_OK) {
	_analyze_info_destroy(gc);
	return ret;
    } else {
	bu_vls_printf(gedp->ged_result_str, "subcommand %s not defined", argv[0]);
    }

    _analyze_info_destroy(gc);
    return BRLCAD_ERROR;
}

// Local Variables:

#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
    struct ged_cmd_impl analyze_cmd_impl = { "analyze", ged_analyze_core, GED_CMD_DEFAULT };
    const struct ged_cmd analyze_cmd = { &analyze_cmd_impl };

    const struct ged_cmd *analyze_cmds[] = { &analyze_cmd, NULL };

    static const struct ged_plugin pinfo = { GED_API,  analyze_cmds, 1 };

    COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
    {
	return &pinfo;
    }
}
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

