/*                        N I R T . C X X
 * BRL-CAD
 *
 * Copyright (c) 2004-2018 United States Government as represented by
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
/** @file nirt.cxx
 *
 * Implementation of Natalie's Interactive Ray-Tracer (NIRT)
 * functionality.
 *
 */

/* BRL-CAD includes */
#include "common.h"

#include <set>
#include <vector>

extern "C" {
#include "bu/color.h"
#include "bu/cmd.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "analyze.h"
}

/* NIRT segment types */
#define NIRT_HIT_SEG      1    /**< @brief Ray segment representing a solid region */
#define NIRT_MISS_SEG     2    /**< @brief Ray segment representing a gap */
#define NIRT_AIR_SEG      3    /**< @brief Ray segment representing an air region */
#define NIRT_OVERLAP_SEG  4    /**< @brief Ray segment representing an overlap region */

struct nirt_state {
    struct application *ap;
    struct db_i *dbip;
    /* Parallel structures needed for operation w/ and w/o air */
    struct rt_i *rtip;
    struct resource res;
    struct rt_i *rtip_air;
    struct resource res_air;
    /* scripts */
    int script_cnt;
    char **scripts;
    /* runtime options */
    int use_air;
    int rt_bot_minpieces;
    struct bu_color hit_color;
    struct bu_color miss_color;
    struct bu_color overlap_color;
    /* state variables */
    double azimuth;
    double elevation;
    vect_t direct;
    vect_t target;
    vect_t grid;
    /* output options */
    int print_header;
    int print_ident_flag;
    unsigned int rt_debug;
    unsigned int nirt_debug;

    /* list of active attributes */
    std::set<std::string> attrs;
    /* list of active paths for raytracer */
    std::set<std::string> active_paths;

    /* Output */
    struct bu_vls *out;
    int out_accumulate;
    struct bu_vls *err;
    int err_accumulate;
    struct bn_vlist *segs;
    //TODO - int segs_accumulate;

    /* Callbacks */
    nirt_hook_t h_state;   // any state change
    nirt_hook_t h_out;     // output changes
    nirt_hook_t h_err;     // err changes
    nirt_hook_t h_segs;    // segement list is changed
    nirt_hook_t h_scripts; // enqueued scripts change
    nirt_hook_t h_objs;    // active list of objects in scene changes
    nirt_hook_t h_frmts;   // output formatting is changed
    nirt_hook_t h_view;    // the camera view is changed

    /* internal */
    bool b_state;   // updated for any state change
    bool b_out;     // updated when output changes
    bool b_err;     // updated when err changes
    bool b_segs;    // updated when segement list is changed
    bool b_scripts; // updated when enqueued scripts change
    bool b_objs;    // updated when active list of objects in scene changes
    bool b_frmts;   // updated when output formatting is changed
    bool b_view;    // updated when the camera view is changed

    int enqueued;   // have enqueued scripts to run

    int ret;  // return code to be returned by nirt_exec after execution
    void *u_data; // user data
};

/**************************
 * Internal functionality *
 **************************/

void lout(struct nirt_state *nss, const char *fmt, ...) _BU_ATTR_PRINTF23
{
    va_list ap;
    struct bu_vls *vls = nss->out;
    BU_CK_VLS(vls);
    va_start(ap, fmt);
    bu_vls_vprintf(vls, fmt, ap);
    nss->b_state = nss->b_out = true;
    va_end(ap);
}

void lerr(struct nirt_state *nss, const char *fmt, ...) _BU_ATTR_PRINTF23
{
    va_list ap;
    struct bu_vls *vls = nss->err;
    BU_CK_VLS(vls);
    va_start(ap, fmt);
    bu_vls_vprintf(vls, fmt, ap);
    nss->b_state = nss->b_err = true;
    va_end(ap);
}

HIDDEN int
raytrace_gettrees(struct nirt_state *nss)
{
    int i;

    // Prepare C-style arrays for rt prep
    std::vector<const char *> objs;
    std::vector<const char *> attrs;
    std::set<std::string>::iterator s_it;
    for (s_it = nss->active_paths.begin(); s_it != nss->active_paths.end(); s_it++) {
	objs.push_back((*s_it).c_str());
    }
    for (s_it = nss->attrs.begin(); s_it != nss->attrs.end(); s_it++) {
	attrs.push_back((*s_it).c_str());
    }
    lout(nss, "Get trees...\n");

    i = rt_gettrees_and_attrs(nss->ap->a_rt_i, (const char **)(attrs.data()), (int)nss->active_paths.size(), (const char **) objs.data(), 1);
    if (i) {
	lerr(nss, "rt_gettrees() failed\n");
	return -1;
    }

    return 0;
}

HIDDEN struct rt_i *
get_rtip(struct nirt_state *nss)
{
    if (nss->use_air) {
	if (!nss->rtip_air) {
	    nss->rtip_air = rt_new_rti(nss->dbip); /* clones dbip, so we can operate on the copy */
	    nss->rtip_air->rti_dbip->dbi_fp = fopen(nss->dbip->dbi_filename, "rb"); /* get read-only fp */
	    if (nss->rtip_air->rti_dbip->dbi_fp == NULL) {
		// TODO - free rtip_air...
		return NULL;
	    }
	    nss->rtip_air->rti_dbip->dbi_read_only = 1;
	    if (db_dirbuild(nss->rtip_air->rti_dbip) < 0) {
		// TODO - free rtip_air...
		return NULL;
	    }
	    rt_init_resource(&nss->res_air, 0, nss->rtip_air);
	}
	return nss->rtip_air;
    } else {
	if (!nss->rtip) {
	    nss->rtip = rt_new_rti(nss->dbip); /* clones dbip, so we can operate on the copy */
	    nss->rtip->rti_dbip->dbi_fp = fopen(nss->dbip->dbi_filename, "rb"); /* get read-only fp */
	    if (nss->rtip->rti_dbip->dbi_fp == NULL) {
		// TODO - free rtip...
		return NULL;
	    }
	    nss->rtip->rti_dbip->dbi_read_only = 1;
	    if (db_dirbuild(nss->rtip->rti_dbip) < 0) {
		// TODO - free rtip...
		return NULL;
	    }
	    rt_init_resource(&nss->res, 0, nss->rtip);
	}
	return nss->rtip;
    }
}

HIDDEN struct resource *
get_resource(struct nirt_state *nss)
{
    if (nss->use_air) return &(nss->res_air);
    return &(nss->res);
}

HIDDEN int
raytrace_prep(struct nirt_state *nss)
{
    std::set<std::string>::iterator s_it;
    lout(nss, "Prepping the geometry...\n");
    if (nss->use_air) {
	rt_prep(nss->rtip_air);
    } else {
	rt_prep(nss->rtip);
    }
    lout(nss, "%s", (nss->active_paths.size() == 1) ? "Object" : "Objects");
    for (s_it = nss->active_paths.begin(); s_it != nss->active_paths.end(); s_it++) {
	lout(nss, " %s", (*s_it).c_str());
    }
    lout(nss, " processed\n");
    return 0;
}

extern "C" int
nirt_if_hit(struct application *UNUSED(ap), struct partition *UNUSED(part_head), struct seg *UNUSED(finished_segs))
{
    bu_log("hit\n");
    return HIT;
}

extern "C" int
nirt_if_miss(struct application *UNUSED(ap))
{
    return MISS;
}

extern "C" int
nirt_if_overlap(struct application *ap, struct partition *pp, struct region *reg1, struct region *reg2, struct partition *InputHdp)
{
    bu_log("overlap\n");
    /* Match current BRL-CAD default behavior */
    return rt_defoverlap (ap, pp, reg1, reg2, InputHdp);
}

/**************
 *  Commands  *
 **************/

extern "C" int
cm_attr(void *ns, int argc, const char *argv[])
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    int i = 0;
    if (!ns) return -1;

    if (argc == 1) {
	// TODO print attrs and return
	return 0;
    }
    for (i = 1; i < argc; i++) {
	bu_log("cm_attr: inserting %s\n", argv[i]);
	nss->attrs.insert(argv[i]);
    }
    if (nss->active_paths.size() > 0) {
	// TODO - redo prep
    }
    return 0;
}

extern "C" int
az_el(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("az_el\n");
    return 0;
}

extern "C" int
dir_vect(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("dir_vect\n");
    return 0;
}

extern "C" int
grid_coor(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("grid_coor\n");
    return 0;
}

extern "C" int
target_coor(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("target_coor\n");
    return 0;
}

extern "C" int
shoot(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    (void)rt_shootray(nss->ap);
    return 0;
}

extern "C" int
backout(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("backout\n");
    return 0;
}

extern "C" int
use_air(void *ns, int argc, const char *argv[])
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    if (argc == 1) {
	lout(nss, "use_air = %d\n", nss->use_air);
	return 0;
    }
    (void)bu_opt_int(NULL, 1, (const char **)&(argv[1]), (void *)&nss->use_air);
    nss->ap->a_rt_i = get_rtip(nss);
    nss->ap->a_resource = get_resource(nss);
    return 0;
}

extern "C" int
nirt_units(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("nirt_units\n");
    return 0;
}

extern "C" int
do_overlap_claims(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("do_overlap_claims\n");
    return 0;
}

extern "C" int
format_output(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("format_output\n");
    return 0;
}

extern "C" int
direct_output(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("direct_output\n");
    return 0;
}

extern "C" int
state_file(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("state_file\n");
    return 0;
}

extern "C" int
dump_state(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("dump_state\n");
    return 0;
}

extern "C" int
load_state(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("load_state\n");
    return 0;
}

extern "C" int
print_item(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("print_item\n");
    return 0;
}

extern "C" int
bot_minpieces(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("bot_minpieces\n");
    return 0;
}

extern "C" int
cm_libdebug(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("cm_libdebug\n");
    return 0;
}

extern "C" int
cm_debug(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("cm_debug\n");
    return 0;
}

extern "C" int
quit(void *UNUSED(ns), int UNUSED(argc), const char *UNUSED(argv[]))
{
    return 1;
}

extern "C" int
show_menu(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("show_menu\n");
    return 0;
}

extern "C" int
queue_cmd(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("queue_cmd\n");
    return 0;
}

extern "C" int
draw_cmd(void *ns, int argc, const char *argv[])
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    int i = 0;
    int ret = 0;
    if (!ns || argc < 2) return -1;
    for (i = 1; i < argc; i++) {
	bu_log("draw_cmd: drawing %s\n", argv[i]);
	nss->active_paths.insert(argv[i]);
    }
    ret = raytrace_gettrees(nss);
    if (ret) return ret;
    return raytrace_prep(nss);
}

extern "C" int
erase_cmd(void *ns, int UNUSED(argc), const char *UNUSED(argv[]))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("erase_cmd\n");
    return 0;
}

extern "C" int
nreport_cmd(void *ns, int argc, const char *argv[])
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    struct rt_i *rtip = nss->ap->a_rt_i;
    double base2local = rtip->rti_dbip->dbi_base2local;
    if (argc == 1) {
	// TODO some kind of generic report...
	return 0;
    }
    if (BU_STR_EQUAL(argv[1], "model_bounds")) {
	lout(nss, "model_min = (%g, %g, %g)    model_max = (%g, %g, %g)\n",
		rtip->mdl_min[X] * base2local,
		rtip->mdl_min[Y] * base2local,
		rtip->mdl_min[Z] * base2local,
		rtip->mdl_max[X] * base2local,
		rtip->mdl_max[Y] * base2local,
		rtip->mdl_max[Z] * base2local);
    }
    return 0;
}

/* Low level manipulation of nirt state - usually will be used
 * by programs.  TODO This and nreport should probably go in
 * a separate cmdtbl so documentation won't automatically get
 * generated for them... */
extern "C" int
nstate_cmd(void *ns, int argc, const char *argv[])
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    if (BU_STR_EQUAL(argv[1], "out_accumulate")) {
	int setting = 0;
	if (argc == 2) {
	    lout(nss, "%d\n", nss->out_accumulate);
	    return 0;
	}
	(void)bu_opt_int(NULL, 1, (const char **)&(argv[2]), (void *)&setting);
	nss->out_accumulate = setting;
	return 0;
    }
    if (BU_STR_EQUAL(argv[1], "err_accumulate")) {
	int setting = 0;
	if (argc == 2) {
	    lout(nss, "%d\n", nss->err_accumulate);
	    return 0;
	}
	(void)bu_opt_int(NULL, 1, (const char **)&(argv[2]), (void *)&setting);
	nss->err_accumulate = setting;
	return 0;
    }


    /* Unknown state command - return -1 */
    return -1;
}


const struct bu_cmdtab nirt_cmds[] = {
    { "attr",           cm_attr},
    { "ae",             az_el},
    { "dir",            dir_vect},
    { "hv",             grid_coor},
    { "xyz",            target_coor},
    { "s",              shoot},
    { "backout",        backout},
    { "useair",         use_air},
    { "units",          nirt_units},
    { "overlap_claims", do_overlap_claims},
    { "fmt",            format_output},
    { "dest",           direct_output},
    { "statefile",      state_file},
    { "dump",           dump_state},
    { "load",           load_state},
    { "print",          print_item},
    { "bot_minpieces",  bot_minpieces},
    { "libdebug",       cm_libdebug},
    { "debug",          cm_debug},
    { "q",              quit},
    { "?",              show_menu},
    // New commands...
    { "queue",          queue_cmd},
    { "draw",           draw_cmd},
    { "erase",          erase_cmd},
    { "nreport",        nreport_cmd},
    { "nstate",         nstate_cmd},
    { (char *)NULL,     NULL}
};



/* Parse command line command and execute */
HIDDEN int
nirt_exec_cmd(NIRT *ns, const char *cmdstr)
{
    int ac;
    int ret = 0;
    char *cmdp = NULL;
    char **av = NULL;
    if (!ns || !cmdstr || strlen(cmdstr) > MAXPATHLEN) return -1;

    /* Take the quoting level down by one, now that we are about to execute */

    // TODO - should we be using bu_vls_decode for this instead of
    // bu_str_unescape?
    cmdp = (char *)bu_calloc(strlen(cmdstr)+2, sizeof(char), "unescaped str");
    bu_str_unescape(cmdstr, cmdp, strlen(cmdstr)+1);

    /* get an argc/argv array for bu_cmd style command execution */
    av = (char **)bu_calloc(strlen(cmdp)+1, sizeof(char *), "av");
    ac = bu_argv_from_string(av, strlen(cmdp), cmdp);

    if (bu_cmd(nirt_cmds, ac, (const char **)av, 0, (void *)ns, &ret) != BRLCAD_OK) {
	ret = -1;
    }

    bu_free(av, "free av array");
    bu_free(cmdp, "free unescaped copy of input cmd");
    return ret;
}

HIDDEN size_t
find_first_unescaped(std::string &s, const char *keys, int offset)
{
    int off = offset;
    int done = 0;
    size_t candidate = std::string::npos;
    while (!done) {
	candidate = s.find_first_of(keys, off);
	if (!candidate || candidate == std::string::npos) return candidate;
	if (s.at(candidate - 1) == '\\') {
	    off = candidate + 1;
	} else {
	    done = 1;
	}
    }
    return candidate;
}

/* Parse command line script into individual commands, and run
 * the supplied callback on each command. The semicolon ';' char
 * denotes the end of one command and the beginning of another, unless
 * the semicolon is inside single or double quotes*/
HIDDEN int
nirt_parse_script(NIRT *ns, const char *script, int (*nc)(NIRT *ns, const char *cmdstr))
{
    if (!ns || !script || !nc) return -1;
    int ret = 0;
    std::string s(script);
    std::string cmd;
    struct bu_vls tcmd = BU_VLS_INIT_ZERO;
    size_t pos = 0;
    size_t q_start, q_end;
    struct bu_vls echar = BU_VLS_INIT_ZERO;

    /* Start by initializing the position markers for quoted substrings. */
    q_start = find_first_unescaped(s, "'\"", 0);
    if (q_start != std::string::npos) {
	bu_vls_sprintf(&echar, "%c", s.at(q_start));
	q_end = find_first_unescaped(s, bu_vls_addr(&echar), q_start + 1);
    } else {
	q_end = std::string::npos;
    }

    /* Slice and dice to get individual commands. */
    while ((pos = s.find(";", pos)) != std::string::npos) {
	/* If we're inside matched quotes, only an un-escaped quote char means
	 * anything */
	if (q_end != std::string::npos && pos > q_start && pos < q_end) {
	    pos = q_end + 1;
	    continue;
	}
	/* Extract and operate on the command specific subset string */
	cmd = s.substr(0, pos);
	bu_vls_sprintf(&tcmd, "%s", cmd.c_str());
	bu_vls_trimspace(&tcmd);
	if (bu_vls_strlen(&tcmd) > 0) {
	    ret = (*nc)(ns, bu_vls_addr(&tcmd));
	    if (ret) goto nps_done;
	}

	/* Prepare the rest of the script string for processing */
	s.erase(0, pos + 1);
	q_start = find_first_unescaped(s, "'\"", 0);
	if (q_start != std::string::npos) {
	    bu_vls_sprintf(&echar, "%c", s.at(q_start));
	    q_end = find_first_unescaped(s, bu_vls_addr(&echar), q_start + 1);
	} else {
	    q_end = std::string::npos;
	}
	pos = 0;
    }

    /* If there's anything left over, it's a tailing command */
    bu_vls_sprintf(&tcmd, "%s", s.c_str());
    bu_vls_trimspace(&tcmd);
    if (bu_vls_strlen(&tcmd) > 0) {
	ret = (*nc)(ns, bu_vls_addr(&tcmd));
    }
nps_done:
    bu_vls_free(&tcmd);
    bu_vls_free(&echar);
    return ret;
}


//////////////////////////
/* Public API functions */
//////////////////////////

int
nirt_alloc(NIRT **ns)
{
    NIRT *n = NULL;

    if (!ns) return -1;

    /* Get memory */
    n = new nirt_state;
    BU_GET(n->ap, struct application);
    BU_GET(n->out, struct bu_vls);
    BU_GET(n->err, struct bu_vls);
    BU_GET(n->segs, struct bn_vlist);
    bu_vls_init(n->out);
    bu_vls_init(n->err);

    n->dbip = DBI_NULL;

    (*ns) = n;
    return 0;
}

int
nirt_init(NIRT *ns, struct db_i *dbip)
{
    if (!ns || !dbip) return -1;

    RT_CK_DBI(dbip);

    ns->dbip = dbip;

    /* initialize the application structure */
    RT_APPLICATION_INIT(ns->ap);
    ns->ap->a_hit = nirt_if_hit;        /* branch to if_hit routine */
    ns->ap->a_miss = nirt_if_miss;      /* branch to if_miss routine */
    ns->ap->a_overlap = nirt_if_overlap;/* branch to if_overlap routine */
    ns->ap->a_logoverlap = rt_silent_logoverlap;
    ns->ap->a_onehit = 0;               /* continue through shotline after hit */
    ns->ap->a_purpose = "NIRT ray";
    ns->ap->a_rt_i = get_rtip(ns);         /* rt_i pointer */
    ns->ap->a_resource = get_resource(ns); /* note: resource is initialized by get_rtip */
    ns->ap->a_zero1 = 0;           /* sanity check, sayth raytrace.h */
    ns->ap->a_zero2 = 0;           /* sanity check, sayth raytrace.h */

    return 0;
}

void
nirt_destroy(NIRT *ns)
{
    if (!ns) return;
    bu_vls_free(ns->err);
    bu_vls_free(ns->out);
    BU_PUT(ns->segs, struct bn_vlist);
    BU_GET(ns->err, struct bu_vls);
    BU_GET(ns->out, struct bu_vls);
    BU_GET(ns->ap, struct application);
    delete ns;
}

#define NIRT_HOOK(btype,htype) \
    do { if (ns->btype && ns->htype){(*ns->htype)(ns, ns->u_data);} } \
    while (0)

int
nirt_exec(NIRT *ns, const char *script)
{
    ns->ret = 0;
    ns->b_state = false;
    ns->b_out = false;
    ns->b_err = false;
    ns->b_segs = false;
    ns->b_scripts = false;
    ns->b_objs = false;
    ns->b_frmts = false;
    ns->b_view = false;

    /* Unless told otherwise, clear the textual outputs
     * before each nirt_exec call. */
    if (!ns->out_accumulate) bu_vls_trunc(ns->out, 0);
    if (!ns->err_accumulate) bu_vls_trunc(ns->err, 0);

    if (script) {
	ns->ret = nirt_parse_script(ns, script, &nirt_exec_cmd);
    } else {
	if (ns->enqueued == 1) {
	    // Execute enqueued logic
	} else {
	    return ns->ret;
	}
    }

    NIRT_HOOK(b_state, h_state);
    NIRT_HOOK(b_out, h_out);
    NIRT_HOOK(b_err, h_err);
    NIRT_HOOK(b_segs, h_segs);
    NIRT_HOOK(b_scripts, h_scripts);
    NIRT_HOOK(b_objs, h_objs);
    NIRT_HOOK(b_frmts, h_frmts);
    NIRT_HOOK(b_view, h_view);

    return ns->ret;
}

void *
nirt_udata(NIRT *ns, void *u_data)
{
    if (!ns) return NULL;
    if (u_data) ns->u_data = u_data;
    return ns->u_data;
}

void
nirt_hook(NIRT *ns, nirt_hook_t hf, int flag)
{
    if (!ns || !hf) return;
    switch (flag) {
	case NIRT_ALL:
	    ns->h_state = hf;
	    break;
	case NIRT_OUT:
	    ns->h_out = hf;
	    break;
	case NIRT_ERR:
	    ns->h_err = hf;
	    break;
	case NIRT_SEGS:
	    ns->h_segs = hf;
	    break;
	case NIRT_SCRIPTS:
	    ns->h_scripts = hf;
	    break;
	case NIRT_OBJS:
	    ns->h_objs = hf;
	    break;
	case NIRT_FRMTS:
	    ns->h_frmts = hf;
	    break;
	case NIRT_VIEW:
	    ns->h_view = hf;
	    break;
	default:
	    return;
    }
}

#define NCFC(nf) ((flags & nf) && !all_set) || (all_set &&  !((flags & nf)))

void
nirt_clear(NIRT *ns, int flags)
{
    int all_set;
    if (!ns) return;
    all_set = (flags & NIRT_ALL) ? 1 : 0;

    if (NCFC(NIRT_OUT)) {
	bu_vls_trunc(ns->out, 0);
    }

    if (NCFC(NIRT_ERR)) {
	bu_vls_trunc(ns->err, 0);
    }

    if (NCFC(NIRT_SEGS)) {
    }

    if (NCFC(NIRT_SCRIPTS)) {
    }

    if (NCFC(NIRT_OBJS)) {
    }

    if (NCFC(NIRT_FRMTS)) {
    }

    if (NCFC(NIRT_VIEW)) {
    }

    /* Standard outputs handled - now, if NIRT_ALL was
     * supplied, clear any internal state. */
    if (flags == NIRT_ALL) {
    }

}

void
nirt_log(struct bu_vls *o, NIRT *ns, int output_type)
{
    if (!o || !ns) return;
    switch (output_type) {
	case NIRT_ALL:
	    bu_vls_sprintf(o, "%s", "NIRT_ALL: TODO\n");
	    break;
	case NIRT_OUT:
	    bu_vls_strcpy(o, bu_vls_addr(ns->out));
	    break;
	case NIRT_ERR:
	    bu_vls_strcpy(o, bu_vls_addr(ns->err));
	    break;
	case NIRT_SEGS:
	    bu_vls_sprintf(o, "%s", "NIRT_SEGS: TODO\n");
	    break;
	case NIRT_SCRIPTS:
	    bu_vls_sprintf(o, "%s", "NIRT_SCRIPTS: TODO\n");
	    break;
	case NIRT_OBJS:
	    bu_vls_sprintf(o, "%s", "NIRT_OBJS: TODO\n");
	    break;
	case NIRT_FRMTS:
	    bu_vls_sprintf(o, "%s", "NIRT_FRMTS: TODO\n");
	    break;
	case NIRT_VIEW:
	    bu_vls_sprintf(o, "%s", "NIRT_VIEW: TODO\n");
	    break;
	default:
	    return;
    }

}

int
nirt_help(struct bu_vls *h, NIRT *ns,  bu_opt_format_t UNUSED(output_type))
{
    if (!h || !ns) return -1;
    return 0;
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
