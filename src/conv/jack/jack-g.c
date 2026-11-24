/*                        J A C K - G . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @file jack-g.c
 *
 * Program to convert JACK Psurf file into a BRL-CAD NMG object.
 *
 * Jack, originally from the University of Pennsylvania's Computer
 * Graphics Research Laboratory, is a package used for human figure
 * animation and ergonomic analysis.
 *
 * This converter was written prior to JACK becoming a commercial
 * product in 1996, but it should be compatible with the more modern
 * Tecnomatix Jack version distributed by Siemens (maintainers of the
 * NX and Parasolid CAD software).  It reportedly still supports JACK
 * 5.1 JT files.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/path.h"
#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"


#define		MAX_NUM_PTS	15360

struct vlist {
    fastf_t		pt[3*MAX_NUM_PTS];
    struct vertex	*vt[MAX_NUM_PTS];
};

static struct bn_tol	tol;

struct jack_g_args {
    int help;
    const char *region_name;
    const char *group_name;
};

static const struct bu_arg_option jack_g_options[] = {
    BU_ARG_FLAG("h", "help", struct jack_g_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_VALUE("r", "region", struct jack_g_args, region_name,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "name",
	"Name the generated BRL-CAD region; default to the input basename "
	"without .pss, or stdin"),
    BU_ARG_VALUE("g", "group", struct jack_g_args, group_name,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "name",
	"Add the generated region to this BRL-CAD group, creating it if "
	"necessary"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand jack_g_operands[] = {
    BU_ARG_OPERAND("input.pss", BU_ARG_VALUE_FILE, 0, 1,
	"Input Jack Psurf file; omit or use - for standard input", NULL),
    BU_ARG_OPERAND("output.g", BU_ARG_VALUE_FILE, 1, 1,
	"Output BRL-CAD database", NULL),
    BU_ARG_OPERAND_NULL
};

static int
jack_g_validate(const struct bu_arg_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg,
	struct bu_arg_validate_result *result)
{
    if (!result || result->state == BU_ARG_VALIDATE_INVALID ||
	cursor_arg < argc || bu_arg_schema_operand_count(schema, argc, argv) != 0)
	return 0;

    bu_arg_validate_result_set(result, BU_ARG_VALIDATE_INCOMPLETE, argc,
	BU_ARG_EXPECT_OPERAND, BU_ARG_VALUE_FILE, "output.g", NULL);
    return 0;
}

static const struct bu_arg_schema jack_g_schema = BU_ARG_SCHEMA(
    "jack-g", "Convert a Jack Psurf model to BRL-CAD",
    jack_g_options, jack_g_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, jack_g_validate, NULL, NULL));

static const struct bu_arg_grammar jack_g_grammar = {
    &jack_g_schema, NULL, NULL
};

int	psurf_to_nmg(struct model *m, FILE *fp, char *jfile, struct bu_list *vlfree);
int	create_brlcad_db(struct rt_wdb *fpout, struct model *m, const char *reg_name, const char *grp_name);
void	jack_to_brlcad(FILE *fpin, struct rt_wdb *fpout, const char *reg_name, const char *grp_name, const char *jfile, struct bu_list *vlfree);

static int
jack_g_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&jack_g_schema, "jack-g");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "jack-g help");
    return status;
}

int
main(int argc, char **argv)
{
    struct jack_g_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    char		*bfile;
    char		*generated_reg_name = NULL;
    const char		*grp_name, *reg_name;
    const char *jfile;
    FILE		*fpin;
    struct rt_wdb	*fpout = NULL;
    size_t doti;
    int introspection;
    int operand_index;
    int remaining;
    struct bu_list *vlfree = &rt_vlfree;

    bu_setprogname(argv[0]);

    introspection = bu_arg_introspect(&msg, &jack_g_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&jack_g_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return jack_g_help(stdout, 0);

    grp_name = args.group_name;
    reg_name = args.region_name;

    remaining = argc - operand_index - 1;
    if (remaining == 1) {
	jfile = "-";
	fpin = stdin;
    } else if (remaining == 2) {
	jfile = argv[operand_index + 1];
	if (BU_STR_EQUAL(jfile, "-")) {
	    fpin = stdin;
	} else if ((fpin = fopen(jfile, "rb")) == NULL) {
	    bu_exit(1, "%s: cannot open %s for reading\n",
		    argv[0], jfile);
	}
    }

    bfile = argv[argc - 1];
    if ((fpout = wdb_fopen(bfile)) == NULL) {
	bu_exit(1, "%s: cannot open %s for writing\n",
		argv[0], bfile);
    }

    /* Output BRL-CAD database header.  No problem if more than one. */
    mk_id(fpout, jfile);

    /* Make default region name if none given. */
    if (!reg_name) {
	generated_reg_name = BU_STR_EQUAL(jfile, "-") ? bu_strdup("stdin") :
	    bu_path_basename(jfile, NULL);

	/* Ignore .pss extension if it's there. */
	doti = strlen(generated_reg_name);
	if (doti > 4) {
	    doti -= 4;
	    if (BU_STR_EQUAL(".pss", generated_reg_name+doti))
		generated_reg_name[doti] = '\0';
	}
	reg_name = generated_reg_name;
    }

    jack_to_brlcad(fpin, fpout, reg_name, grp_name, jfile, vlfree);
    if (fpin != stdin)
	fclose(fpin);
    if (generated_reg_name)
	bu_free(generated_reg_name, "region name");
    db_close(fpout->dbip);
    return 0;
}

/*
 *	Convert a UPenn Jack data base into a BRL-CAD data base.
 */
void
jack_to_brlcad(FILE *fpin, struct rt_wdb *fpout, const char *reg_name, const char *grp_name, const char *jfile, struct bu_list *vlfree)
{
    struct model	*m;

    m = nmg_mm();			/* Make nmg model. */
    char *ljfile = bu_strdup(jfile);
    psurf_to_nmg(m, fpin, ljfile, vlfree);	/* Convert psurf model to nmg. */
    bu_free(ljfile, "ljfile");
    create_brlcad_db(fpout, m, reg_name, grp_name);	/* Put in db. */
    nmg_km(m);			/* Destroy the nmg model. */
}

/*
 *	Read in vertices from a psurf file and store them in an
 *	array of nmg vertex structures.
 *
 *	Fix this!  Only allows set max of points and assumes
 *	no errors during reading...
 */
int
read_psurf_vertices(FILE *fp, struct vlist *vert)
/* Psurf file pointer. */
/* Array of read in vertices. */
{
    double x, y, z;
    int	i;
    int	bomb=0;
    size_t ret;

    /* Read vertices. */
    for (i = 0; fscanf(fp, "%lf %lf %lf", &x, &y, &z) == 3; i++) {
	if (i >= MAX_NUM_PTS) {
	    bomb = 1;
	} else {
	    vert->pt[3*i+0] = x * 10.;	/* Convert cm to mm. */
	    vert->pt[3*i+1] = y * 10.;
	    vert->pt[3*i+2] = z * 10.;
	    vert->vt[i] = (struct vertex *)0;
	}
	ret = fscanf(fp, "%*[^\n]");
	if (ret > 0)
	    bu_log("unknown parsing error\n");
    }
    ret = fscanf(fp, ";;");
    if (ret > 0)
	bu_log("unknown parsing error\n");

    if (bomb)
    {
	bu_exit(1, "ERROR: Dataset contains %d data points, code is dimensioned for %d\n", i, MAX_NUM_PTS);
    }

    return i;
}

/*
 *	Read in the vertexes describing a face of a psurf.
 */
int
read_psurf_face(FILE *fp, int *lst)
{
    int	i, n;
    size_t ret;

    for (i = 0; fscanf(fp, "%d", &n) == 1; i++)
	lst[i] = n;
    ret = fscanf(fp, "%*[^\n]");
    if (ret > 0)
	bu_log("unknown parsing error\n");

    return i;
}

int
psurf_to_nmg(struct model *m, FILE *fp, char *jfile, struct bu_list *vlfree)
/* Input/output, nmg model. */
/* Input, pointer to psurf data file. */
/* Name of Jack data base file. */
{
    int		face, fail, i, lst[MAX_NUM_PTS], nf, nv;
    struct faceuse	*outfaceuses[MAX_NUM_PTS];
    struct nmgregion *r;
    struct shell	*s;
    struct vertex	*vertlist[MAX_NUM_PTS];
    struct vlist	vert;

    /* Copied from proc-db/nmgmodel.c */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.01;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 0.001;
    tol.para = 0.999;

    face = 0;
    r = nmg_mrsv(m);	/* Make region, empty shell, vertex. */
    s = BU_LIST_FIRST(shell, &r->s_hd);

    while ((nv = read_psurf_vertices(fp, &vert)) != 0) {
	size_t ret;

	while ((nf = read_psurf_face(fp, lst)) != 0) {

	    /* Make face out of vertices in lst (ccw ordered). */
	    for (i = 0; i < nf; i++)
		vertlist[i] = vert.vt[lst[i]-1];
	    outfaceuses[face] = nmg_cface(s, vertlist, nf);
	    face++;

	    /* Save (possibly) newly created vertex structs. */
	    for (i = 0; i < nf; i++)
		vert.vt[lst[i]-1] = vertlist[i];
	}
	ret = fscanf(fp, ";;");
	if (ret > 0)
	    bu_log("unknown parsing error\n");

	/* Associate the vertex geometry, ccw. */
	for (i = 0; i < nv; i++)
	    if (vert.vt[i])
		nmg_vertex_gv(vert.vt[i], &vert.pt[3*i]);
	    else
		fprintf(stderr, "%s, vertex %d is unused\n",
			jfile, i+1);
    }

    nmg_vertex_fuse(&m->magic, vlfree, &tol);

    /* Associate the face geometry. */
    for (i = 0, fail = 0; i < face; i++)
    {
	struct loopuse *lu;
	plane_t pl;

	lu = BU_LIST_FIRST(loopuse, &outfaceuses[i]->lu_hd);
	if (nmg_loop_plane_area(lu, pl) < 0.0)
	{
	    fail = 1;
	    nmg_kfu(outfaceuses[i]);
	}
	else
	    nmg_face_g(outfaceuses[i], pl);
    }
    if (fail)
	return -1;

    if (face)
    {
	int empty_model;
	empty_model = nmg_kill_zero_length_edgeuses(m);
	if (!empty_model) {

	  /* Compute "geometry" for region and shell */
	  nmg_region_a(r, &tol);

	  nmg_break_e_on_v(&m->magic, vlfree, &tol);
	  empty_model = nmg_kill_zero_length_edgeuses(m);

	  /* Glue edges of outward pointing face uses together. */
	  if (!empty_model) nmg_edge_fuse(&m->magic, vlfree, &tol);
	}
    }

    return 0;
}

/*
 *	Write the nmg to a BRL-CAD style data base.
 */
int
create_brlcad_db(struct rt_wdb *fpout, struct model *m, const char *reg_name, const char *grp_name)
{
    char *rname, *sname;
    int empty_model;
    size_t name_size;
    struct shell *s;
    struct nmgregion *r;

    name_size = strlen(reg_name) + 3;
    rname = (char *)bu_malloc(name_size, "rname");	/* Region name. */
    sname = (char *)bu_malloc(name_size, "sname");	/* Solid name. */

    snprintf(sname, name_size, "s.%s", reg_name);
    empty_model = nmg_kill_zero_length_edgeuses(m);
    if (empty_model) {
	bu_log("Warning: skipping empty model.");
	bu_free(sname, "sname");
	bu_free(rname, "rname");
	return 0;
    }
    nmg_rebound(m, &tol);
    r = BU_LIST_FIRST(nmgregion, &m->r_hd);
    s = BU_LIST_FIRST(shell, &r->s_hd);
    mk_bot_from_nmg(fpout, sname,  s);		/* Make BOT object. */
    snprintf(rname, name_size, "r.%s", reg_name);
    mk_comb1(fpout, rname, sname, 1);	/* Put object in a region. */
    if (grp_name) {
	mk_comb1(fpout, grp_name, rname, 1);	/* Region in group. */
    }
    bu_free(sname, "sname");
    bu_free(rname, "rname");
    return 0;
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
