/*                     S T L _ W R I T E . C
 * BRL-CAD
 *
 * Copyright (c) 2003-2022 United States Government as represented by
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
 * Convert a BRL-CAD model (in a .g file) to an STL file by
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
#include "bu/units.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "gcv.h"


#define V3ARGS_SCALE(v, factor)       (v)[X] * (factor), (v)[Y] * (factor), (v)[Z] * (factor)

#define VSET_SCALE(a, b, factor) \
do { \
    (a)[X] = (b)[X] * (factor); \
    (a)[Y] = (b)[Y] * (factor); \
    (a)[Z] = (b)[Z] * (factor); \
} while (0)


struct stl_write_options
{
    int binary;
    int output_directory;
};


struct conversion_state
{
    const struct gcv_opts *gcv_options;
    const struct stl_write_options *stl_write_options;

    const char *output_file;	        /* output filename */

    struct db_i *dbip;
    FILE *fp;			/* Output file pointer */
    int bfd;		    	/* Output binary file descriptor */

    struct model *the_model;
    struct bu_vls file_name;		/* file name built from region name */

    int regions_tried;
    int regions_converted;
    int regions_written;
    size_t tot_polygons;
};


HIDDEN char *
stl_write_make_units_str(double scale_factor)
{
    const char *bu_units = bu_units_string(scale_factor);

    if (bu_units)
	return bu_strdup(bu_units);
    else {
	struct bu_vls temp = BU_VLS_INIT_ZERO;
	bu_vls_printf(&temp, "%g units per mm", scale_factor);
	return bu_vls_strgrab(&temp);
    }
}


/* Byte swaps a four byte value */
HIDDEN void
stl_write_lswap(unsigned int *v)
{
    unsigned int r;

    r =*v;
    *v = ((r & 0xff) << 24) | ((r & 0xff00) << 8) | ((r & 0xff0000) >> 8)
	| ((r & 0xff000000) >> 24);
}


HIDDEN void
nmg_to_stl(struct nmgregion *r, const struct db_full_path *pathp, struct db_tree_state* UNUSED(tsp), void *client_data)
{
    struct model *m;
    struct shell *s;
    struct vertex *v;
    char *region_name;
    int region_polys=0;
    int ret;
    struct conversion_state * const pstate = (struct conversion_state *)client_data;

    NMG_CK_REGION(r);
    RT_CK_FULL_PATH(pathp);

    region_name = db_path_to_string(pathp);

    if (pstate->stl_write_options->output_directory) {
	char *c;

	bu_vls_trunc(&pstate->file_name, 0);
	bu_vls_strcpy(&pstate->file_name, pstate->output_file);
	bu_vls_putc(&pstate->file_name, '/');
	c = region_name;
	c++;
	while (*c != '\0') {
	    if (*c == '/') {
		bu_vls_putc(&pstate->file_name, '@');
	    } else if (*c == '.' || isspace((int)*c)) {
		bu_vls_putc(&pstate->file_name, '_');
	    } else {
		bu_vls_putc(&pstate->file_name, *c);
	    }
	    c++;
	}
	bu_vls_strcat(&pstate->file_name, ".stl");
	if (!pstate->stl_write_options->binary) {
	    /* Open ASCII output file */
	    if ((pstate->fp=fopen(bu_vls_addr(&pstate->file_name), "wb+")) == NULL)
	    {
		perror("g-stl");
		bu_exit(1, "Cannot open ASCII output file (%s) for writing\n", bu_vls_addr(&pstate->file_name));
	    }
	} else {
	    char buf[81] = {0};	/* need exactly 80 char for header */

	    /* Open binary output file */
	    if ((pstate->bfd=open(bu_vls_addr(&pstate->file_name), O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0)
	    {
		perror("g-stl");
		bu_exit(1, "ERROR: Cannot open binary output file (%s) for writing\n", bu_vls_addr(&pstate->file_name));
	    }

	    if (!region_name) {
		char *units_str = stl_write_make_units_str(pstate->gcv_options->scale_factor);
		snprintf(buf, 80, "BRL-CAD generated STL FILE (Units=%s)", units_str);
		bu_free(units_str, "units_str");
	    } else {
		char *units_str = stl_write_make_units_str(pstate->gcv_options->scale_factor);

		if (region_name && strlen(region_name) > 0) {
		    snprintf(buf, 80, "BRL-CAD generated STL FILE (Units=%s) %s", units_str, region_name);
		} else {
		    snprintf(buf, 80, "BRL-CAD generated STL FILE (Units=%s)", units_str);
		}

		bu_free(units_str, "units_str");
	    }
	    ret = write(pstate->bfd, &buf, 80);
	    if (ret < 0)
		perror("write");

	    /* write a place keeper for the number of triangles */
	    memset(buf, 0, 4);
	    ret = write(pstate->bfd, &buf, 4);
	    if (ret < 0)
		perror("write");
	}
    }

    m = r->m_p;
    NMG_CK_MODEL(m);

    /* Write pertinent info for this region */
    if (!pstate->stl_write_options->binary)
	fprintf(pstate->fp, "solid %s\n", (region_name+1));

    /* triangulate model */
    nmg_triangulate_model(m, &RTG.rtg_vlfree, &pstate->gcv_options->calculational_tolerance);

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

		if (!pstate->stl_write_options->binary) {
		    fprintf(pstate->fp, "  facet normal %f %f %f\n", V3ARGS(facet_normal));
		    fprintf(pstate->fp, "    outer loop\n");
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
		    if (!pstate->stl_write_options->binary) {
			fprintf(pstate->fp, "      vertex ");
			fprintf(pstate->fp, "%f %f %f\n", V3ARGS_SCALE(v->vg_p->coord, pstate->gcv_options->scale_factor));
		    } else {
			VSET_SCALE(flt_ptr, v->vg_p->coord, pstate->gcv_options->scale_factor);
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
		if (!pstate->stl_write_options->binary) {
		    fprintf(pstate->fp, "    endloop\n");
		    fprintf(pstate->fp, "  endfacet\n");
		} else {
		    int i;

		    bu_cv_htonf(vert_buffer, (const unsigned char *)flts, 12);
		    for (i=0; i<12; i++) {
			stl_write_lswap((unsigned int *)&vert_buffer[i*4]);
		    }
		    ret = write(pstate->bfd, vert_buffer, 50);
		    if (ret < 0)
			perror("write");
		}
		pstate->tot_polygons++;
		region_polys++;
	    }
	}
    }
    if (!pstate->stl_write_options->binary)
	fprintf(pstate->fp, "endsolid %s\n", (region_name+1));

    if (pstate->stl_write_options->output_directory) {
	if (pstate->stl_write_options->binary) {
	    unsigned char tot_buffer[4];

	    /* Re-position pointer to 80th byte */
	    bu_lseek(pstate->bfd, 80, SEEK_SET);

	    /* Write out number of triangles */
	    *(uint32_t *)tot_buffer = htonl((unsigned long)region_polys);
	    stl_write_lswap((unsigned int *)tot_buffer);
	    ret = write(pstate->bfd, tot_buffer, 4);
	    if (ret < 0)
		perror("write");
	    close(pstate->bfd);
	} else {
	    fclose(pstate->fp);
	}
    }
    bu_free(region_name, "region name");
}


HIDDEN void
stl_write_create_opts(struct bu_opt_desc **options_desc, void **dest_options_data)
{
    struct stl_write_options *options_data;

    BU_ALLOC(options_data, struct stl_write_options);
    *dest_options_data = options_data;
    *options_desc = (struct bu_opt_desc *)bu_malloc(3 * sizeof(struct bu_opt_desc), "options_desc");

    BU_OPT((*options_desc)[0], NULL, "binary", NULL,
	    NULL, &options_data->binary,
	    "specify that the input is in binary STL format");

    BU_OPT((*options_desc)[1], NULL, "output-dir", NULL,
	    NULL, &options_data->output_directory,
	    "specify that the output path should be a directory");

    BU_OPT_NULL((*options_desc)[2]);
}


HIDDEN void
stl_write_free_opts(void *options_data)
{
    bu_free(options_data, "options_data");
}


HIDDEN int
stl_write(struct gcv_context *context, const struct gcv_opts *gcv_options, const void *options_data, const char *dest_path)
{
    int ret;
    double percent = 0.0;
    struct db_tree_state tree_state;
    struct conversion_state state;
    struct gcv_region_end_data gcvwriter;

    gcvwriter.write_region = nmg_to_stl;
    gcvwriter.client_data = &state;

    memset(&state, 0, sizeof(state));
    state.gcv_options = gcv_options;
    state.stl_write_options = (struct stl_write_options *)options_data;
    state.output_file = dest_path;
    state.dbip = context->dbip;
    BU_VLS_INIT(&state.file_name);

    if (!state.stl_write_options->output_directory) {
	if (!state.stl_write_options->binary) {
	    /* Open ASCII output file */
	    if ((state.fp = fopen(state.output_file, "wb+")) == NULL) {
		perror("libgcv");
		bu_log("cannot open ASCII output file (%s) for writing\n", state.output_file);
		return 0;
	    }
	} else {
	    /* Open binary output file */
	    if ((state.bfd = open(state.output_file, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0) {
		perror("libgcv");
		bu_log("cannot open binary output file (%s) for writing\n", state.output_file);
		return 0;
	    }
	}
    }

    /* Write out STL header if output file is binary */
    if (state.stl_write_options->binary && !state.stl_write_options->output_directory) {
	char buf[81];	/* need exactly 80 char for header */
	char *units_str = stl_write_make_units_str(state.gcv_options->scale_factor);

	memset(buf, 0, sizeof(buf));
	sprintf(buf, "BRL-CAD generated STL FILE (Units=%s)", units_str);
	bu_free(units_str, "units_str");

	ret = write(state.bfd, &buf, 80);
	if (ret < 0)
	    perror("write");

	/* write a place keeper for the number of triangles */
	memset(buf, 0, 4);
	ret = write(state.bfd, &buf, 4);
	if (ret < 0)
	    perror("write");
    }

    tree_state = rt_initial_tree_state;	/* struct copy */
    tree_state.ts_tol = &state.gcv_options->calculational_tolerance;
    tree_state.ts_ttol = &state.gcv_options->tessellation_tolerance;
    tree_state.ts_m = &state.the_model;

    /* make empty NMG model */
    state.the_model = nmg_mm();
    BU_LIST_INIT(&RTG.rtg_vlfree);	/* for vlist macros */

    /* Walk indicated tree(s).  Each region will be output separately */
    (void) db_walk_tree(state.dbip, gcv_options->num_objects, (const char **)gcv_options->object_names,
			1,
			&tree_state,
			0,			/* take all regions */
			(gcv_options->tessellation_algorithm == GCV_TESS_MARCHING_CUBES)?gcv_region_end_mc:gcv_region_end,
			(gcv_options->tessellation_algorithm == GCV_TESS_MARCHING_CUBES)?NULL:nmg_booltree_leaf_tess,
			(void *)&gcvwriter);

    if (state.regions_tried>0) {
	percent = ((double)state.regions_converted * 100) / state.regions_tried;
	if (state.gcv_options->verbosity_level)
	    bu_log("Tried %d regions, %d converted to NMG's successfully.  %g%%\n",
		   state.regions_tried, state.regions_converted, percent);
    }

    if (state.regions_tried > 0) {
	percent = ((double)state.regions_written * 100) / state.regions_tried;
	if (state.gcv_options->verbosity_level)
	    bu_log("                  %d triangulated successfully. %g%%\n",
		   state.regions_written, percent);
    }

    bu_log("%zu triangles written\n", state.tot_polygons);

    if (!state.stl_write_options->output_directory) {
	if (state.stl_write_options->binary) {
	    unsigned char tot_buffer[4];

	    /* Re-position pointer to 80th byte */
	    bu_lseek(state.bfd, 80, SEEK_SET);

	    /* Write out number of triangles */
	    *(uint32_t *)tot_buffer = htonl((unsigned long)state.tot_polygons);
	    stl_write_lswap((unsigned int *)tot_buffer);
	    ret = write(state.bfd, tot_buffer, 4);
	    if (ret < 0)
		perror("write");
	    close(state.bfd);
	} else {
	    fclose(state.fp);
	}
    }

    /* Release dynamic storage */
    nmg_km(state.the_model);
    rt_vlist_cleanup();

    return 1;
}

const struct gcv_filter gcv_conv_stl_write = {
    "STL Writer", GCV_FILTER_WRITE, BU_MIME_MODEL_STL, NULL,
    stl_write_create_opts, stl_write_free_opts, stl_write
};


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
