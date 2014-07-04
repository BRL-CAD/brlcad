/*                        J A C K - G . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"


#define		MAX_NUM_PTS	15360

struct vlist {
    fastf_t		pt[3*MAX_NUM_PTS];
    struct vertex	*vt[MAX_NUM_PTS];
};

static struct bn_tol	tol;

static const char usage[] = "Usage: %s [-r region] [-g group] [jack_db] [brlcad_db]\n";

extern fastf_t nmg_loop_plane_area(const struct loopuse *lu, plane_t pl);

int	psurf_to_nmg(struct shell *s, FILE *fp, char *jfile);
int	create_brlcad_db(struct rt_wdb *fpout, struct shell *s, char *reg_name, char *grp_name);
void	jack_to_brlcad(FILE *fpin, struct rt_wdb *fpout, char *reg_name, char *grp_name, char *jfile);


int
main(int argc, char **argv)
{
    char		*base, *bfile, *grp_name, *jfile, *reg_name;
    FILE		*fpin;
    struct rt_wdb	*fpout = NULL;
    int		doti;
    int	c;

    grp_name = reg_name = NULL;

    /* Get command line arguments. */
    while ((c = bu_getopt(argc, argv, "g:r:h?")) != -1) {
	switch (c) {
	    case 'g':
		grp_name = bu_optarg;
		/* BRL-CAD group to add psurf to. */
		break;
	    case 'r':
		/* BRL-CAD region name for psurf. */
		reg_name = bu_optarg;
		break;
	    default:
		bu_exit(1, usage, argv[0]);
		break;
	}
    }

    /* Get Jack psurf input file name. */
    if (bu_optind >= argc) {
	jfile = "-";
	fpin = stdin;
    } else {
	jfile = argv[bu_optind];
	if ((fpin = fopen(jfile, "rb")) == NULL) {
	    bu_exit(1, "%s: cannot open %s for reading\n",
		    argv[0], jfile);
	}
    }

    /* Get BRL-CAD output data base name. */
    bu_optind++;
    if (bu_optind >= argc) {
	bu_exit(1, usage, argv[0]);
    }
    bfile = argv[bu_optind];
    if ((fpout = wdb_fopen(bfile)) == NULL) {
	bu_exit(1, "%s: cannot open %s for writing\n",
		argv[0], bfile);
    }

    /* Output BRL-CAD database header.  No problem if more than one. */
    mk_id(fpout, jfile);

    /* Make default region name if none given. */
    if (!reg_name) {
	/* Ignore leading path info. */
	base = strrchr(argv[1], '/');
	if (base)
	    base++;
	else
	    base = argv[1];
	reg_name = (char *)bu_malloc(sizeof(base)+1, "reg_name");
	bu_strlcpy(reg_name, base, sizeof(base)+1);
	/* Ignore .pss extension if it's there. */
	doti = strlen(reg_name) - 4;
	if (doti > 0 && BU_STR_EQUAL(".pss", reg_name+doti))
	    reg_name[doti] = '\0';
    }

    jack_to_brlcad(fpin, fpout, reg_name, grp_name, jfile);
    fclose(fpin);
    wdb_close(fpout);
    return 0;
}

/*
 *	Convert a UPenn Jack data base into a BRL-CAD data base.
 */
void
jack_to_brlcad(FILE *fpin, struct rt_wdb *fpout, char *reg_name, char *grp_name, char *jfile)
{
    struct shell	*s;

    s = nmg_ms();			/* Make nmg model. */
    psurf_to_nmg(s, fpin, jfile);	/* Convert psurf model to nmg. */
    create_brlcad_db(fpout, s, reg_name, grp_name);	/* Put in db. */
    nmg_ks(s);			/* Destroy the nmg model. */
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
psurf_to_nmg(struct shell *s, FILE *fp, char *jfile)
/* Input/output, nmg model. */
/* Input, pointer to psurf data file. */
/* Name of Jack data base file. */
{
    int		face, fail, i, lst[MAX_NUM_PTS], nf, nv;
    struct faceuse	*outfaceuses[MAX_NUM_PTS];
    struct vertex	*vertlist[MAX_NUM_PTS];
    struct vlist	vert;

    /* Copied from proc-db/nmgmodel.c */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.01;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 0.001;
    tol.para = 0.999;

    face = 0;
    s = nmg_ms();

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

    nmg_vertex_fuse(&s->magic, &tol);

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
	int empty_shell;
	empty_shell = nmg_kill_zero_length_edgeuses(s);
	if (!empty_shell) {

	  /* Compute "geometry" for region and shell */
	  nmg_shell_a(s, &tol);

	  nmg_break_e_on_v(&s->magic, &tol);
	  empty_shell = nmg_kill_zero_length_edgeuses(s);

	  /* Glue edges of outward pointing face uses together. */
	  if (!empty_shell) nmg_edge_fuse(&s->magic, &tol);
	}
    }

    return 0;
}

/*
 *	Write the nmg to a BRL-CAD style data base.
 */
int
create_brlcad_db(struct rt_wdb *fpout, struct shell *s, char *reg_name, char *grp_name)
{
    char *rname, *sname;
    int empty_model;

    rname = (char *)bu_malloc(sizeof(reg_name) + 3, "rname");	/* Region name. */
    sname = (char *)bu_malloc(sizeof(reg_name) + 3, "sname");	/* Solid name. */

    snprintf(sname, sizeof(reg_name) + 2, "s.%s", reg_name);
    empty_model = nmg_kill_zero_length_edgeuses(s);
    if (empty_model) {
	bu_log("Warning: skipping empty model.");
	return 0;
    }
    nmg_rebound(s, &tol);
    mk_bot_from_nmg(fpout, sname,  s);		/* Make BOT object. */
    snprintf(rname, sizeof(reg_name) + 2, "r.%s", reg_name);
    mk_comb1(fpout, rname, sname, 1);	/* Put object in a region. */
    if (grp_name) {
	mk_comb1(fpout, grp_name, rname, 1);	/* Region in group. */
    }
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
