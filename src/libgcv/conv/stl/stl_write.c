/*                         S T L _ W R I T E . C
 * BRL-CAD
 *
 * Copyright (c) 2003-2014 United States Government as represented by
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
/** @file stl_write.c
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
#include "bnetwork.h"
#include "bio.h"

/* interface headers */
#include "bu/getopt.h"
#include "bu/cv.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "../../plugin.h"


#define V3ARGSIN(a)       (a)[X]/25.4, (a)[Y]/25.4, (a)[Z]/25.4
#define VSETIN(a, b) {\
    (a)[X] = (b)[X]/25.4; \
    (a)[Y] = (b)[Y]/25.4; \
    (a)[Z] = (b)[Z]/25.4; \
}


static char usage[] =
    "[-bvi8] [-xX lvl] [-a abs_tess_tol] [-r rel_tess_tol] [-n norm_tess_tol] [-D dist_calc_tol] [-o output_file_name.stl | -m directory_name] brlcad_db.g object(s)\n";

static void
print_usage(const char *progname)
{
    bu_log("Usage: %s %s", progname, usage);
}


struct stl_conv_data {
    int verbose;
    int is_success;
    int binary;		         	/* whether to produce binary or ascii */
    const char *output_file;	        /* output filename */
    char *output_directory;             /* directory name to hold output files */
    FILE *fp;                    	/* Output file pointer */
    int bfd;				/* Output binary file descriptor */
    struct db_i *dbip;
    struct model *the_model;
    struct bu_vls file_name;             /* file name built from region name */
    struct rt_tess_tol ttol;		/* tessellation tolerance in mm */
    struct bn_tol tol;		        /* calculation tolerance */
    struct db_tree_state tree_state;	/* includes tol & model */
    int inches;
    size_t tot_polygons;
};


/* Byte swaps a four byte value */
static void
lswap(unsigned int *v)
{
    unsigned int r;

    r = *v;
    *v = ((r & 0xff) << 24) | ((r & 0xff00) << 8) | ((r & 0xff0000) >> 8)
	 | ((r & 0xff000000) >> 24);
}


static void
nmg_to_stl(struct nmgregion *r, const struct db_full_path *pathp,
	   int UNUSED(region_id), int UNUSED(material_id), float UNUSED(color[3]),
	   void *client_data)
{
    struct model *m;
    struct shell *s;
    struct vertex *v;
    char *region_name;
    int region_polys = 0;
    int ret;
    struct stl_conv_data *conv_data = (struct stl_conv_data *)client_data;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string(pathp);

    if (conv_data->output_directory) {
	char *c;

	bu_vls_trunc(&conv_data->file_name, 0);
	bu_vls_strcpy(&conv_data->file_name, conv_data->output_directory);
	bu_vls_putc(&conv_data->file_name, '/');
	c = region_name;
	c++;

	while (*c != '\0') {
	    if (*c == '/') {
		bu_vls_putc(&conv_data->file_name, '@');
	    } else if (*c == '.' || isspace((int)*c)) {
		bu_vls_putc(&conv_data->file_name, '_');
	    } else {
		bu_vls_putc(&conv_data->file_name, *c);
	    }

	    c++;
	}

	bu_vls_strcat(&conv_data->file_name, ".stl");

	if (!conv_data->binary) {
	    /* Open ASCII output file */
	    if ((conv_data->fp = fopen(bu_vls_addr(&conv_data->file_name),
				       "wb+")) == NULL) {
		perror("g-stl");
		bu_log("Cannot open ASCII output file (%s) for writing\n",
		       bu_vls_addr(&conv_data->file_name));
		conv_data->is_success = 0;
		return;
	    }
	} else {
	    char buf[81] = {0};	/* need exactly 80 char for header */

	    /* Open binary output file */
	    if ((conv_data->bfd = open(bu_vls_addr(&conv_data->file_name),
				       O_WRONLY | O_CREAT | O_TRUNC | O_BINARY,
				       S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
		perror("g-stl");
		bu_log("ERROR: Cannot open binary output file (%s) for writing\n",
		       bu_vls_addr(&conv_data->file_name));
		conv_data->is_success = 0;
		return;
	    }

	    if (!region_name) {
		snprintf(buf, 80, "BRL-CAD generated STL FILE (Units=%s)",
			 conv_data->inches ? "inches" : "mm");
	    } else {
		if (region_name && strlen(region_name) > 0) {
		    snprintf(buf, 80, "BRL-CAD generated STL FILE (Units=%s) %s",
			     conv_data->inches ? "inches" : "mm", region_name);
		} else {
		    snprintf(buf, 80, "BRL-CAD generated STL FILE (Units=%s) %s",
			     conv_data->inches ? "inches" : "mm", region_name);
		}
	    }

	    ret = write(conv_data->bfd, &buf, 80);

	    if (ret < 0)
		perror("write");

	    /* write a place keeper for the number of triangles */
	    memset(buf, 0, 4);
	    ret = write(conv_data->bfd, &buf, 4);

	    if (ret < 0)
		perror("write");
	}
    }

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* Write pertinent info for this region */
    if (!conv_data->binary)
	fprintf(conv_data->fp, "solid %s\n", (region_name + 1));

    /* triangulate model */
    nmg_triangulate_model(m, &conv_data->tol);

    /* Check triangles */
    for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	struct faceuse *fu;

	NMG_CK_SHELL(s);

	for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	    struct loopuse *lu;
	    vect_t facet_normal;

	    NMG_CK_FACEUSE(fu);

	    if (fu->orientation != OT_SAME)
		continue;

	    /* Grab the face normal and save it for all the vertex loops */
	    NMG_GET_FU_NORMAL(facet_normal, fu);

	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		struct edgeuse *eu;
		int vert_count = 0;
		float flts[12];
		float *flt_ptr = NULL;
		unsigned char vert_buffer[50];

		NMG_CK_LOOPUSE(lu);

		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
		    continue;

		memset(vert_buffer, 0, sizeof(vert_buffer));

		if (!conv_data->binary) {
		    fprintf(conv_data->fp, "  facet normal %f %f %f\n",
			    V3ARGS(facet_normal));
		    fprintf(conv_data->fp, "    outer loop\n");
		} else {
		    flt_ptr = flts;
		    VMOVE(flt_ptr, facet_normal);
		    flt_ptr += 3;
		}

		/* check vertex numbers for each triangle */
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    NMG_CK_EDGEUSE(eu);

		    vert_count++;

		    v = eu->vu_p->v_p;
		    NMG_CK_VERTEX(v);

		    if (!conv_data->binary)
			fprintf(conv_data->fp, "      vertex ");

		    if (conv_data->inches)
			if (!conv_data->binary) {
			    fprintf(conv_data->fp, "%f %f %f\n", V3ARGSIN(v->vg_p->coord));
			} else {
			    VSETIN(flt_ptr, v->vg_p->coord);
			    flt_ptr += 3;
			}
		    else if (!conv_data->binary) {
			fprintf(conv_data->fp, "%f %f %f\n", V3ARGS(v->vg_p->coord));
		    } else {
			VMOVE(flt_ptr, v->vg_p->coord);
			flt_ptr += 3;
		    }
		}

		if (vert_count > 3) {
		    bu_free(region_name, "region name");
		    bu_log("lu %p has %d vertices!\n", (void *)lu, vert_count);
		    bu_log("ERROR: LU is not a triangle");
		    conv_data->is_success = 0;
		    return;
		} else if (vert_count < 3)
		    continue;

		if (!conv_data->binary) {
		    fprintf(conv_data->fp, "    endloop\n");
		    fprintf(conv_data->fp, "  endfacet\n");
		} else {
		    int i;

		    bu_cv_htonf(vert_buffer, (const unsigned char *)flts, 12);

		    for (i = 0; i < 12; i++) {
			lswap((unsigned int *)&vert_buffer[i * 4]);
		    }

		    ret = write(conv_data->bfd, vert_buffer, 50);

		    if (ret < 0)
			perror("write");
		}

		conv_data->tot_polygons++;
		region_polys++;
	    }
	}
    }

    if (!conv_data->binary)
	fprintf(conv_data->fp, "endsolid %s\n", (region_name + 1));

    if (conv_data->output_directory) {
	if (conv_data->binary) {
	    unsigned char tot_buffer[4];

	    /* Re-position pointer to 80th byte */
	    lseek(conv_data->bfd, 80, SEEK_SET);

	    /* Write out number of triangles */
	    *(uint32_t *)tot_buffer = htonl((unsigned long)region_polys);
	    lswap((unsigned int *)tot_buffer);
	    ret = write(conv_data->bfd, tot_buffer, 4);

	    if (ret < 0)
		perror("write");

	    close(conv_data->bfd);
	} else {
	    fclose(conv_data->fp);
	}
    }

    bu_free(region_name, "region name");
}


static int
gcv_stl_write(const char *path, struct db_i *vdbip,
	      const struct gcv_opts *UNUSED(options))
{
    int ret;
    int use_mc = 0;
    int mutex;

    size_t num_objects = 0;
    char **object_names = NULL;

    struct stl_conv_data conv_data;
    struct gcv_region_end_data gcvwriter = {nmg_to_stl, NULL};

    gcvwriter.client_data = (void *)&conv_data;
    conv_data.verbose = 0;
    conv_data.is_success = 1;
    conv_data.binary = 0;			/* Default output is ASCII */
    conv_data.output_file = NULL;
    conv_data.output_directory = NULL;
    BU_VLS_INIT(&conv_data.file_name);
    conv_data.inches = 0;
    conv_data.tot_polygons = 0;

    conv_data.dbip = vdbip;

    bu_setlinebuf(stderr);

    conv_data.tree_state = rt_initial_tree_state;	/* struct copy */
    conv_data.tree_state.ts_tol = &conv_data.tol;
    conv_data.tree_state.ts_ttol = &conv_data.ttol;
    conv_data.tree_state.ts_m = &conv_data.the_model;

    /* Set up tessellation tolerance defaults */
    conv_data.ttol.magic = RT_TESS_TOL_MAGIC;
    /* Defaults, updated by command line options. */
    conv_data.ttol.abs = 0.0;
    conv_data.ttol.rel = 0.01;
    conv_data.ttol.norm = 0.0;

    /* Set up calculation tolerance defaults */
    /* FIXME: These need to be improved */
    conv_data.tol.magic = BN_TOL_MAGIC;
    conv_data.tol.dist = 0.0005;
    conv_data.tol.dist_sq = conv_data.tol.dist * conv_data.tol.dist;
    conv_data.tol.perp = 1e-6;
    conv_data.tol.para = 1 - conv_data.tol.perp;

    /* make empty NMG model */
    conv_data.the_model = nmg_mm();
    BU_LIST_INIT(&RTG.rtg_vlfree);	/* for vlist macros */

    conv_data.output_file = path;

    mutex = (conv_data.output_file && conv_data.output_directory);

    if (mutex) {
	print_usage("g-stl");
	return 0;
    }

    if (!conv_data.output_file && !conv_data.output_directory) {
	if (conv_data.binary) {
	    bu_log("Can't output binary to stdout\n");
	    return 0;
	}

	conv_data.fp = stdout;
    } else if (conv_data.output_file) {
	if (!conv_data.binary) {
	    /* Open ASCII output file */
	    if ((conv_data.fp = fopen(conv_data.output_file, "wb+")) == NULL) {
		perror("g-stl");
		bu_log("%s: Cannot open ASCII output file (%s) for writing\n", "g-stl",
		       conv_data.output_file);
		return 0;
	    }
	} else {
	    /* Open binary output file */
	    if ((conv_data.bfd = open(conv_data.output_file,
				      O_WRONLY | O_CREAT | O_TRUNC | O_BINARY,
				      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
		perror("g-stl");
		bu_log("%s: Cannot open binary output file (%s) for writing\n",
		       "g-stl", conv_data.output_file);
		return 0;
	    }
	}
    }

    BN_CK_TOL(conv_data.tree_state.ts_tol);
    RT_CK_TESS_TOL(conv_data.tree_state.ts_ttol);

    if (conv_data.verbose) {
	bu_log("\nTessellation tolerances:\n\tabs = %g mm\n\trel = %g\n\tnorm = %g\n",
	       conv_data.tree_state.ts_ttol->abs, conv_data.tree_state.ts_ttol->rel,
	       conv_data.tree_state.ts_ttol->norm);
	bu_log("Calculational tolerances:\n\tdist = %g mm perp = %g\n",
	       conv_data.tree_state.ts_tol->dist, conv_data.tree_state.ts_tol->perp);
    }

    /* Write out STL header if output file is binary */
    if (conv_data.binary && conv_data.output_file) {
	char buf[81];	/* need exactly 80 char for header */

	memset(buf, 0, sizeof(buf));

	if (conv_data.inches) {
	    bu_strlcpy(buf, "BRL-CAD generated STL FILE (Units=inches)", sizeof(buf));
	} else {
	    bu_strlcpy(buf, "BRL-CAD generated STL FILE (Units=mm)", sizeof(buf));
	}

	ret = write(conv_data.bfd, &buf, 80);

	if (ret < 0)
	    perror("write");

	/* write a place keeper for the number of triangles */
	memset(buf, 0, 4);
	ret = write(conv_data.bfd, &buf, 4);

	if (ret < 0)
	    perror("write");
    }

    /* get toplevel objects */
    {
	struct directory **results;
	db_update_nref(conv_data.dbip, &rt_uniresource);
	num_objects = db_ls(conv_data.dbip, DB_LS_TOPS, NULL, &results);
	object_names = db_dpv_to_argv(results);
	bu_free(results, "tops");
    }

    /* Walk indicated tree(s).  Each region will be output separately */
    (void) db_walk_tree(conv_data.dbip, num_objects, (const char **)object_names,
			1,
			&conv_data.tree_state,
			0,			/* take all regions */
			use_mc ? gcv_region_end_mc : gcv_region_end,
			use_mc ? NULL : nmg_booltree_leaf_tess,
			(void *)&gcvwriter);

    bu_free(object_names, "object_names");

    bu_log("%zu triangles written\n", conv_data.tot_polygons);

    if (conv_data.output_file) {
	if (conv_data.binary) {
	    unsigned char tot_buffer[4];

	    /* Re-position pointer to 80th byte */
	    lseek(conv_data.bfd, 80, SEEK_SET);

	    /* Write out number of triangles */
	    *(uint32_t *)tot_buffer = htonl((unsigned long)conv_data.tot_polygons);
	    lswap((unsigned int *)tot_buffer);
	    ret = write(conv_data.bfd, tot_buffer, 4);

	    if (ret < 0)
		perror("write");

	    close(conv_data.bfd);
	} else {
	    fclose(conv_data.fp);
	}
    }

    /* Release dynamic storage */
    nmg_km(conv_data.the_model);
    rt_vlist_cleanup();

    return !!conv_data.is_success;
}


static const struct gcv_converter converters[] = {
    {"stl", NULL, gcv_stl_write},
    {NULL, NULL, NULL}
};

const struct gcv_plugin_info gcv_plugin_conv_stl_write = {converters};


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
