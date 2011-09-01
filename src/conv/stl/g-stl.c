/*                         G - S T L . C
 * BRL-CAD
 *
 * Copyright (c) 2003-2011 United States Government as represented by
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
 *
 */
/** @file g-stl.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to an STL file by
 * calling on the NMG booleans.  Based on g-acad.c.
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include "bio.h"
#include "bin.h"

/* interface headers */
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "gcv.h"


#define V3ARGSIN(a)       (a)[X]/25.4, (a)[Y]/25.4, (a)[Z]/25.4
#define VSETIN(a, b) {\
    (a)[X] = (b)[X]/25.4; \
    (a)[Y] = (b)[Y]/25.4; \
    (a)[Z] = (b)[Z]/25.4; \
}


static char usage[] = "Usage: %s [-bviM8] [-xX lvl] [-a abs_tess_tol] [-r rel_tess_tol] [-n norm_tess_tol] [-D dist_calc_tol] [-o output_file_name.stl | -m directory_name] brlcad_db.g object(s)\n";

static int verbose;
static int NMG_debug;			/* saved arg of -X, for longjmp handling */
static int ncpu = 1;			/* Number of processors */
static int binary = 0;			/* Default output is ASCII */
static char *output_file = NULL;	/* output filename */
static char *output_directory = NULL;	/* directory name to hold output files */
static FILE *fp;			/* Output file pointer */
static int bfd;				/* Output binary file descriptor */
static struct db_i *dbip;
static struct model *the_model;
static struct bu_vls file_name;		/* file name built from region name */
static struct rt_tess_tol ttol;		/* tesselation tolerance in mm */
static struct bn_tol tol;		/* calculation tolerance */
static struct db_tree_state tree_state;	/* includes tol & model */

static int regions_tried = 0;
static int regions_converted = 0;
static int regions_written = 0;
static int inches = 0;
static unsigned int tot_polygons = 0;


/* Byte swaps a four byte value */
void
lswap(unsigned int *v)
{
    unsigned int r;

    r =*v;
    *v = ((r & 0xff) << 24) | ((r & 0xff00) << 8) | ((r & 0xff0000) >> 8)
	| ((r & 0xff000000) >> 24);
}


static void
nmg_to_stl(struct nmgregion *r, const struct db_full_path *pathp, int UNUSED(region_id), int UNUSED(material_id), float UNUSED(color[3]))
{
    struct model *m;
    struct shell *s;
    struct vertex *v;
    char *region_name;
    int region_polys=0;
    int ret;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string(pathp);

    if (output_directory) {
	char *c;

	bu_vls_trunc(&file_name, 0);
	bu_vls_strcpy(&file_name, output_directory);
	bu_vls_putc(&file_name, '/');
	c = region_name;
	c++;
	while (*c != '\0') {
	    if (*c == '/') {
		bu_vls_putc(&file_name, '@');
	    } else if (*c == '.' || isspace(*c)) {
		bu_vls_putc(&file_name, '_');
	    } else {
		bu_vls_putc(&file_name, *c);
	    }
	    c++;
	}
	bu_vls_strcat(&file_name, ".stl");
	if (!binary) {
	    /* Open ASCII output file */
	    if ((fp=fopen(bu_vls_addr(&file_name), "wb+")) == NULL)
	    {
		perror("g-stl");
		bu_exit(1, "Cannot open ASCII output file (%s) for writing\n", bu_vls_addr(&file_name));
	    }
	} else {
	    char buf[81] = {0};	/* need exactly 80 char for header */

	    /* Open binary output file */
	    if ((bfd=open(bu_vls_addr(&file_name), O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0)
	    {
		perror("g-stl");
		bu_log("ERROR: Cannot open binary output file (%s) for writing\n", bu_vls_addr(&file_name));
	    }

	    if (!region_name) {
		snprintf(buf, 80, "BRL-CAD generated STL FILE (Units=%s)", inches?"inches":"mm");
	    } else {
		if (region_name && strlen(region_name) > 0) {
		    snprintf(buf, 80, "BRL-CAD generated STL FILE (Units=%s) %s", inches?"inches":"mm", region_name);
		} else {
		    snprintf(buf, 80, "BRL-CAD generated STL FILE (Units=%s) %s", inches?"inches":"mm", region_name);
		}
	    }
	    ret = write(bfd, &buf, 80);
	    if (ret < 0)
		perror("write");

	    /* write a place keeper for the number of triangles */
	    memset(buf, 0, 4);
	    ret = write(bfd, &buf, 4);
	    if (ret < 0)
		perror("write");
	}
    }

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* Write pertinent info for this region */
    if (!binary)
	fprintf(fp, "solid %s\n", (region_name+1));

    /* triangulate model */
    nmg_triangulate_model(m, &tol);

    /* Check triangles */
    for (BU_LIST_FOR (s, shell, &r->s_hd))
    {
	struct faceuse *fu;

	NMG_CK_SHELL(s);

	for (BU_LIST_FOR (fu, faceuse, &s->fu_hd))
	{
	    struct loopuse *lu;
	    vect_t facet_normal;

	    NMG_CK_FACEUSE(fu);

	    if (fu->orientation != OT_SAME)
		continue;

	    /* Grab the face normal and save it for all the vertex loops */
	    NMG_GET_FU_NORMAL(facet_normal, fu);

	    for (BU_LIST_FOR (lu, loopuse, &fu->lu_hd))
	    {
		struct edgeuse *eu;
		int vert_count=0;
		float flts[12];
		float *flt_ptr = NULL;
		unsigned char vert_buffer[50];

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		memset(vert_buffer, 0, sizeof(vert_buffer));

		if (!binary) {
		    fprintf(fp, "  facet normal %f %f %f\n", V3ARGS(facet_normal));
		    fprintf(fp, "    outer loop\n");
		} else {
		    flt_ptr = flts;
		    VMOVE(flt_ptr, facet_normal);
		    flt_ptr += 3;
		}

		/* check vertex numbers for each triangle */
		for (BU_LIST_FOR (eu, edgeuse, &lu->down_hd))
		{
		    NMG_CK_EDGEUSE(eu);

		    vert_count++;

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);
		    if (!binary)
			fprintf(fp, "      vertex ");
		    if (inches)
			if (!binary) {
			    fprintf(fp, "%f %f %f\n", V3ARGSIN(v->vg_p->coord));
			} else {
			    VSETIN(flt_ptr, v->vg_p->coord);
			    flt_ptr += 3;
			}
		    else
			if (!binary) {
			    fprintf(fp, "%f %f %f\n", V3ARGS(v->vg_p->coord));
			} else {
			    VMOVE(flt_ptr, v->vg_p->coord);
			    flt_ptr += 3;
			}
		}
		if (vert_count > 3)
		{
		    bu_free(region_name, "region name");
		    bu_log("lu %p has %d vertices!\n", (void *)lu, vert_count);
		    bu_exit(1, "ERROR: LU is not a triangle");
		}
		else if (vert_count < 3)
		    continue;
		if (!binary) {
		    fprintf(fp, "    endloop\n");
		    fprintf(fp, "  endfacet\n");
		} else {
		    int i;

		    htonf(vert_buffer, (const unsigned char *)flts, 12);
		    for (i=0; i<12; i++) {
			lswap((unsigned int *)&vert_buffer[i*4]);
		    }
		    ret = write(bfd, vert_buffer, 50);
		    if (ret < 0)
			perror("write");
		}
		tot_polygons++;
		region_polys++;
	    }
	}
    }
    if (!binary)
	fprintf(fp, "endsolid %s\n", (region_name+1));

    if (output_directory) {
	if (binary) {
	    unsigned char tot_buffer[4];

	    /* Re-position pointer to 80th byte */
	    lseek(bfd, 80, SEEK_SET);

	    /* Write out number of triangles */
	    *(uint32_t *)tot_buffer = htonl((unsigned long)region_polys);
	    lswap((unsigned int *)tot_buffer);
	    ret = write(bfd, tot_buffer, 4);
	    if (ret < 0)
		perror("write");
	    close(bfd);
	} else {
	    fclose(fp);
	}
    }
    bu_free(region_name, "region name");
}


/* FIXME: this be a dumb hack to avoid void* conversion */
struct gcv_data {
    void (*func)(struct nmgregion *, const struct db_full_path *, int, int, float [3]);
};
static struct gcv_data gcvwriter = {nmg_to_stl};


/*
 *			M A I N
 */
int
main(int argc, char *argv[])
{
    int c;
    double percent;
    int i;
    int ret;
    int use_mc = 0;

    bu_setlinebuf(stderr);

    tree_state = rt_initial_tree_state;	/* struct copy */
    tree_state.ts_tol = &tol;
    tree_state.ts_ttol = &ttol;
    tree_state.ts_m = &the_model;

    /* Set up tesselation tolerance defaults */
    ttol.magic = RT_TESS_TOL_MAGIC;
    /* Defaults, updated by command line options. */
    ttol.abs = 0.0;
    ttol.rel = 0.01;
    ttol.norm = 0.0;

    /* Set up calculation tolerance defaults */
    /* FIXME: These need to be improved */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-5;
    tol.para = 1 - tol.perp;

    /* init resources we might need */
    rt_init_resource(&rt_uniresource, 0, NULL);

    /* make empty NMG model */
    the_model = nmg_mm();
    BU_LIST_INIT(&rt_g.rtg_vlfree);	/* for vlist macros */

    /* Get command line arguments. */
    while ((c = bu_getopt(argc, argv, "a:b8m:n:o:r:vx:D:P:X:i")) != -1) {
	switch (c) {
	    case 'a':		/* Absolute tolerance. */
		ttol.abs = atof(bu_optarg);
		ttol.rel = 0.0;
		break;
	    case 'b':		/* Binary output file */
		binary=1;
		break;
	    case 'n':		/* Surface normal tolerance. */
		ttol.norm = atof(bu_optarg);
		ttol.rel = 0.0;
		break;
	    case '8':
		use_mc = 1;
		break;
	    case 'o':		/* Output file name. */
		output_file = bu_optarg;
		break;
	    case 'm':
		output_directory = bu_optarg;
		bu_vls_init(&file_name);
		break;
	    case 'r':		/* Relative tolerance. */
		ttol.rel = atof(bu_optarg);
		break;
	    case 'v':
		verbose++;
		break;
	    case 'P':
		ncpu = atoi(bu_optarg);
		rt_g.debug = 1;
		break;
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *)&rt_g.debug);
		break;
	    case 'D':
		tol.dist = atof(bu_optarg);
		tol.dist_sq = tol.dist * tol.dist;
		rt_pr_tol(&tol);
		break;
	    case 'X':
		sscanf(bu_optarg, "%x", (unsigned int *)&rt_g.NMG_debug);
		NMG_debug = rt_g.NMG_debug;
		break;
	    case 'i':
		inches = 1;
		break;
	    default:
		bu_exit(1, usage, argv[0]);
		break;
	}
    }

    if (bu_optind+1 >= argc) {
	bu_exit(1, usage, argv[0]);
    }

    if (output_file && output_directory) {
	bu_log("ERROR: options \"-o\" and \"-m\" are mutually exclusive\n");
	bu_exit(1, usage, argv[0]);
    }

    if (!output_file && !output_directory) {
	if (binary) {
	    bu_exit(1, "Can't output binary to stdout\n");
	}
	fp = stdout;
    } else if (output_file) {
	if (!binary) {
	    /* Open ASCII output file */
	    if ((fp=fopen(output_file, "wb+")) == NULL)
	    {
		perror(argv[0]);
		bu_exit(1, "Cannot open ASCII output file (%s) for writing\n", output_file);
	    }
	} else {
	    /* Open binary output file */
	    if ((bfd=open(output_file, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0)
	    {
		perror(argv[0]);
		bu_exit(1, "Cannot open binary output file (%s) for writing\n", output_file);
	    }
	}
    }

    /* Open brl-cad database */
    argc -= bu_optind;
    argv += bu_optind;
    if ((dbip = db_open(argv[0], "r")) == DBI_NULL) {
	perror(argv[0]);
	bu_exit(1, "Unable to open geometry file (%s)\n", argv[0]);
    }
    if (db_dirbuild(dbip)) {
	bu_exit(1, "ERROR: db_dirbuild failed\n");
    }

    BN_CK_TOL(tree_state.ts_tol);
    RT_CK_TESS_TOL(tree_state.ts_ttol);

    if (verbose) {
	bu_log("Model: %s\n", argv[0]);
	bu_log("Objects:");
	for (i=1; i<argc; i++)
	    bu_log(" %s", argv[i]);
	bu_log("\nTesselation tolerances:\n\tabs = %g mm\n\trel = %g\n\tnorm = %g\n",
	       tree_state.ts_ttol->abs, tree_state.ts_ttol->rel, tree_state.ts_ttol->norm);
	bu_log("Calculational tolerances:\n\tdist = %g mm perp = %g\n",
	       tree_state.ts_tol->dist, tree_state.ts_tol->perp);
    }

    /* Write out STL header if output file is binary */
    if (binary && output_file) {
	char buf[81];	/* need exactly 80 char for header */

	memset(buf, 0, sizeof(buf));
	if (inches) {
	    bu_strlcpy(buf, "BRL-CAD generated STL FILE (Units=inches)", sizeof(buf));
	} else {
	    bu_strlcpy(buf, "BRL-CAD generated STL FILE (Units=mm)", sizeof(buf));
	}
	ret = write(bfd, &buf, 80);
	if (ret < 0)
	    perror("write");

	/* write a place keeper for the number of triangles */
	memset(buf, 0, 4);
	ret = write(bfd, &buf, 4);
	if (ret < 0)
	    perror("write");
    }

    /* Walk indicated tree(s).  Each region will be output separately */
    (void) db_walk_tree(dbip, argc-1, (const char **)(argv+1),
			1,			/* ncpu */
			&tree_state,
			0,			/* take all regions */
			use_mc?gcv_region_end_mc:gcv_region_end,
			use_mc?NULL:nmg_booltree_leaf_tess,
			(genptr_t)&gcvwriter);

    percent = 0;
    if (regions_tried>0) {
	percent = ((double)regions_converted * 100) / regions_tried;
	if (verbose)
	    bu_log("Tried %d regions, %d converted to NMG's successfully.  %g%%\n",
		   regions_tried, regions_converted, percent);
    }
    percent = 0;

    if (regions_tried > 0) {
	percent = ((double)regions_written * 100) / regions_tried;
	if (verbose)
	    bu_log("                  %d triangulated successfully. %g%%\n",
		   regions_written, percent);
    }

    bu_log("%lu triangles written\n", tot_polygons);

    if (output_file) {
	if (binary) {
	    unsigned char tot_buffer[4];

	    /* Re-position pointer to 80th byte */
	    lseek(bfd, 80, SEEK_SET);

	    /* Write out number of triangles */
	    *(uint32_t *)tot_buffer = htonl((unsigned long)tot_polygons);
	    lswap((unsigned int *)tot_buffer);
	    ret = write(bfd, tot_buffer, 4);
	    if (ret < 0)
		perror("write");
	    close(bfd);
	} else {
	    fclose(fp);
	}
    }

    /* Release dynamic storage */
    nmg_km(the_model);
    rt_vlist_cleanup();
    db_close(dbip);

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
