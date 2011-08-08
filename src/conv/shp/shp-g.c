/*                         S H P - G . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2011 United States Government as represented by
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
/** @file shp-g.c
 *
 * Convert Esri Shapefile format files to BRL-CAD .g binary format
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"

#include "./shapelib/shapefil.h"


static void
usage(const char *argv0)
{
    bu_log("Usage: %s [-v] [-d] [-x rt_debug_flag] shapefile [output.g]\n", argv0);
    bu_log("	where shapefile is a Shapefile prefix or file.shp shape format file\n");
    bu_log("	and output.g is the name of the BRL-CAD database file being output to.\n");
    bu_log("	The -v option prints verbose conversion progress information.\n");
    bu_log("	The -d option prints additional diagnostic debugging information.\n");
    bu_log("	The -x option specifies an RT debug flags (see raytrace.h).\n");
}


static int
make_shape(struct rt_wdb *fd, int verbose, int debug, size_t idx, size_t num, point2d_t *v)
{
    size_t i;

    point_t V = VINIT_ZERO;
    vect_t h = VINIT_ZERO;
    struct bu_vls str_sketch;
    struct bu_vls str_extrude;

    struct rt_sketch_internal *skt = NULL;
    struct line_seg *lsg = NULL;

    /* nothing to do? */
    if (num == 0 || !v)
	return 0;

    if (!fd) {
	if (debug)
	    bu_log("ERROR: unable to write out shape\n");
	return 1;
    }

    if (verbose || debug)
	bu_log("%zu vertices\n", num);

    skt = (struct rt_sketch_internal *)bu_calloc(1, sizeof(struct rt_sketch_internal), "sketch");
    skt->magic = RT_SKETCH_INTERNAL_MAGIC;

    VMOVE(skt->V, V);
    VSET(skt->u_vec, 1.0, 0.0, 0.0);
    VSET(skt->v_vec, 0.0, 1.0, 0.0);

    skt->vert_count = num;
    skt->verts = (point2d_t *)bu_calloc(skt->vert_count, sizeof( point2d_t ), "verts");
    for (i = 0; i < skt->vert_count; i++ ) {
	V2MOVE(skt->verts[i], v[i]);
    }

    /* Specify number of segments */
    skt->curve.count = num;
    /* FIXME: investigate allocation */
    skt->curve.reverse = (int *)bu_calloc(skt->curve.count, sizeof(int), "sketch: reverse");
    skt->curve.segment = (genptr_t *)bu_calloc(skt->curve.count, sizeof(genptr_t), "segs");

    /* Insert all line segments except the last one */
    for (i = 0; i < num-1; i++) {
	lsg = (struct line_seg *)bu_malloc(sizeof(struct line_seg), "sketch: lsg");
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = i;
	lsg->end = i + 1;
	skt->curve.segment[i] = (genptr_t)lsg;
    }

    /* Connect the last connected vertex to the first vertex */
    lsg = (struct line_seg *)bu_malloc(sizeof(struct line_seg), "sketch: lsg");
    lsg->magic = CURVE_LSEG_MAGIC;
    lsg->start = num - 1;
    lsg->end = 0;
    skt->curve.segment[num - 1] = (genptr_t)lsg;

    /* write out sketch shape */
    bu_vls_init(&str_sketch);
    bu_vls_sprintf(&str_sketch, "shape-%zu.sketch", idx);
    mk_sketch(fd, bu_vls_addr(&str_sketch), skt);

    /* extrude the shape */
    bu_vls_init(&str_extrude);
    bu_vls_sprintf(&str_extrude, "shape-%zu.extrude", idx);
    VSET(h, 0.0, 0.0, 10.0); /* FIXME: arbitrary height */
    mk_extrusion(fd, bu_vls_addr(&str_extrude), bu_vls_addr(&str_sketch), skt->V, h, skt->u_vec, skt->v_vec, 0);

    bu_vls_free(&str_sketch);
    bu_vls_free(&str_extrude);

    return 0;
}


int
main(int argc, char *argv[])
{
    int opt;
    size_t i;
    const char *argv0 = argv[0];
    struct rt_wdb *fd_out;
    struct bu_vls vls_in;
    struct bu_vls vls_out;

    int opt_debug = 0;
    int opt_verbose = 0;

    /* shapelib vars */
    SHPHandle shapefile;
    size_t shp_num_invalid = 0;
    int shp_num_entities = 0;
    int shp_type = 0;
    hvect_t shp_min = HINIT_ZERO;
    hvect_t shp_max = HINIT_ZERO;

    /* geometry */
    point2d_t *verts = NULL;
    size_t num_verts = 0;

    if (argc < 2) {
	usage(argv0);
	bu_exit(1, NULL);
    }

    while ((opt = bu_getopt(argc, argv, "dxv")) != -1) {
	switch (opt) {
	    case 'd':
		opt_debug = 1;
		break;
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *) &rt_g.debug);
		bu_printb("librt RT_G_DEBUG", RT_G_DEBUG, DEBUG_FORMAT);
		bu_log("\n");
		break;
	    case 'v':
		opt_verbose++;
		break;
	    default:
		usage(argv0);
		bu_exit(1, NULL);
		break;
	}
    }
    argv += bu_optind;
    argc -= bu_optind;

    if (opt_verbose)
	bu_log("Verbose output enabled.\n");

    if (opt_debug)
	bu_log("Debugging output enabled.\n");

    /* validate input/output file specifiers */
    if (argc < 1) {
	usage(argv0);
	bu_exit(1, "ERROR: Missing input and output file names\n");
    }

    bu_vls_init(&vls_in);
    bu_vls_strcat(&vls_in, argv[0]);

    bu_vls_init(&vls_out);
    if (argc < 2) {
	bu_vls_printf(&vls_out, "%s.g", argv[0]);
    } else {
	bu_vls_strcat(&vls_out, argv[1]);
    }

    if (opt_verbose) {
	bu_log("Reading from [%V]\n", &vls_in);
	bu_log("Writing to [%V]\n\n", &vls_out);
    }

    /* initialize single threaded resource */
    rt_init_resource(&rt_uniresource, 0, NULL);

    /* open the input */
    shapefile = SHPOpen(bu_vls_addr(&vls_in), "rb");
    if (!shapefile) {
	bu_log("ERROR: Unable to open shapefile [%V]\n", &vls_in);
	bu_vls_free(&vls_in);
	bu_vls_free(&vls_out);
	bu_exit(4, NULL);    }

    /* print shapefile details */
    if (opt_verbose) {
	SHPGetInfo(shapefile, &shp_num_entities, &shp_type, shp_min, shp_max);

	bu_log("Shapefile Type: %s\n", SHPTypeName(shp_type));
	bu_log("# of Shapes: %d\n\n", shp_num_entities);
	bu_log("File Bounds: (%12.3f,%12.3f, %.3g, %.3g)\n"
	       "         to  (%12.3f,%12.3f, %.3g, %.3g)\n",
	       shp_min[0], shp_min[1], shp_min[2], shp_min[3],
	       shp_max[0], shp_max[1], shp_max[2], shp_max[3]);
    }

    /* open the .g for writing */
    if ((fd_out = wdb_fopen(bu_vls_addr(&vls_out))) == NULL) {
	bu_log("ERROR: Unable to open shapefile [%V]\n", &vls_out);
	bu_vls_free(&vls_in);
	bu_vls_free(&vls_out);
	perror(argv0);
	bu_exit(5, NULL);
    }

    /* iterate over all entities */
    for (i=0; i < (size_t)shp_num_entities; i++) {
	SHPObject *object;
	int shp_part;
	size_t j;

	object = SHPReadObject(shapefile, i);
	if (!object) {
	    if (opt_debug)
		bu_log("Shape %zu of %zu is missing, skipping.\n", i+1, (size_t)shp_num_entities);
	    continue;
	}

	/* validate the object */
	if (opt_debug) {
	    int shp_altered = SHPRewindObject(shapefile, object);
	    if (shp_altered > 0) {
		bu_log("WARNING: Shape %zu of %zu has [%d] bad loop orientations.\n", i+1, (size_t)shp_num_entities, shp_altered);
		shp_num_invalid++;
	    }
	}

	/* print detail header */
	if (opt_verbose) {
	    if (object->bMeasureIsUsed) {
		bu_log("\nShape:%zu (%s)  nVertices=%d, nParts=%d\n"
		       "  Bounds:(%12.3f,%12.3f, %g, %g)\n"
		       "      to (%12.3f,%12.3f, %g, %g)\n",
		       i+1, SHPTypeName(object->nSHPType), object->nVertices, object->nParts,
		       object->dfXMin, object->dfYMin, object->dfZMin, object->dfMMin,
		       object->dfXMax, object->dfYMax, object->dfZMax, object->dfMMax);
	    } else {
		bu_log("\nShape:%zu (%s)  nVertices=%d, nParts=%d\n"
		       "  Bounds:(%12.3f,%12.3f, %g)\n"
		       "      to (%12.3f,%12.3f, %g)\n",
		       i+1, SHPTypeName(object->nSHPType), object->nVertices, object->nParts,
		       object->dfXMin, object->dfYMin, object->dfZMin,
		       object->dfXMax, object->dfYMax, object->dfZMax);
	    }

	    if (object->nParts > 0 && object->panPartStart[0] != 0) {
		if (opt_debug)
		    bu_log("Shape %zu of %zu: panPartStart[0] = %d, not zero as expected.\n", i+1, (size_t)shp_num_entities, object->panPartStart[0]);
		continue;
	    }
	}

	num_verts = 0;
	verts = bu_calloc((size_t)object->nVertices, sizeof(point2d_t), "alloc point array");

	for (j = 0, shp_part = 1; j < (size_t)object->nVertices; j++) {
	    if (shp_part < object->nParts
		&& j == (size_t)object->panPartStart[shp_part]) {
		shp_part++;
		bu_log("Shape %zu of %zu: End of Loop\n", i+1, (size_t)shp_num_entities);
		make_shape(fd_out, opt_verbose, opt_debug, i, num_verts, verts);

		/* reset for next loop */
		memset(verts, 0, sizeof(point2d_t) * object->nVertices);
		num_verts = 0;
	    }
	    bu_log("%zu/%zu:%zu/%zu\t\t", i+1, (size_t)shp_num_entities, j+1, (size_t)object->nVertices);
	    bu_log("(%12.4f, %12.4f, %12.4f, %g)\n", object->padfX[j], object->padfY[j], object->padfZ[j], object->padfM[j]);

	    V2SET(verts[num_verts], object->padfX[j], object->padfY[j]);
	    num_verts++;
	}
	bu_log("Shape %zu of %zu: End of Loop\n", i+1, (size_t)shp_num_entities);
	make_shape(fd_out, opt_verbose, opt_debug, i, num_verts, verts);

	bu_free(verts, "free point array");
	verts = NULL;
	num_verts = 0;

	SHPDestroyObject(object);
	object = NULL;
    }

    if (opt_verbose) {
	if (shp_num_invalid > 0) {
	    bu_log("WARNING: %zu of %zu shape(s) had bad loop orientations.\n", shp_num_invalid, (size_t)shp_num_entities);
	}
	bu_log("\nDone.\n");
    }

    /* close up our files */
    SHPClose(shapefile);
    wdb_close(fd_out);

    /* free up allocated resources */
    bu_vls_free(&vls_in);
    bu_vls_free(&vls_out);

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
