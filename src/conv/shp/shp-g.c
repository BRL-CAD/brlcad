/*                         S H P - G . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2026 United States Government as represented by
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"

#include "./shapelib/shapefil.h"


struct shp_g_debug_mask {
    unsigned int value;
    int set;
};

struct shp_g_args {
    int help;
    int debug;
    int verbose;
    struct shp_g_debug_mask rt_debug;
};

static int
shp_g_debug_mask_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    struct shp_g_debug_mask *mask = (struct shp_g_debug_mask *)storage;
    unsigned int value;

    if (!bu_arg_unsigned_integer_from_str(msg, &value, arg, 16))
	return -1;
    if (mask) {
	mask->value = value;
	mask->set = 1;
    }
    return 0;
}

static const struct bu_arg_option shp_g_options[] = {
    BU_ARG_FLAG("h", "help", struct shp_g_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "verbose", struct shp_g_args, verbose,
	"Print shapefile type, shape count, bounds, per-shape details, "
	"and loop-orientation summary"),
    BU_ARG_FLAG("d", "debug", struct shp_g_args, debug,
	"Print diagnostics for missing shapes and corrected loop winding"),
    BU_ARG_CUSTOM("x", "rt-debug", struct shp_g_args, rt_debug,
	shp_g_debug_mask_parse, "mask",
	"Set librt hexadecimal debug flags (see RT_DEBUG in raytrace.h)"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand shp_g_operands[] = {
    BU_ARG_OPERAND("shapefile", BU_ARG_VALUE_FILE, 1, 1,
	"Input Shapefile prefix or .shp file", NULL),
    BU_ARG_OPERAND("output.g", BU_ARG_VALUE_FILE, 0, 1,
	"Output BRL-CAD database", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema shp_g_schema = BU_ARG_SCHEMA(
    "shp-g", "Convert an Esri Shapefile to a BRL-CAD database",
    shp_g_options, shp_g_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar shp_g_grammar = {
    &shp_g_schema, NULL, NULL
};

static int
shp_g_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&shp_g_schema, "shp-g");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "shp-g help");
    return status;
}


static int
make_shape(struct rt_wdb *fd, int verbose, int debug, size_t idx, size_t num, point2d_t *v)
{
    size_t i;

    point_t V = VINIT_ZERO;
    vect_t h = VINIT_ZERO;
    struct bu_vls str_sketch = BU_VLS_INIT_ZERO;
    struct bu_vls str_extrude = BU_VLS_INIT_ZERO;

    struct rt_sketch_internal skt;
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

    skt.magic = RT_SKETCH_INTERNAL_MAGIC;

    VMOVE(skt.V, V);
    VSET(skt.u_vec, 1.0, 0.0, 0.0);
    VSET(skt.v_vec, 0.0, 1.0, 0.0);

    skt.vert_count = num;
    skt.verts = v;

    /* Specify number of segments */
    skt.curve.count = num;
    /* FIXME: investigate allocation */
    skt.curve.reverse = (int *)bu_calloc(skt.curve.count, sizeof(int), "sketch: reverse");
    skt.curve.segment = (void **)bu_calloc(skt.curve.count, sizeof(void *), "segs");

    /* Insert all line segments except the last one */
    for (i = 0; i < num-1; i++) {
	BU_ALLOC(lsg, struct line_seg);
	lsg->magic = CURVE_LSEG_MAGIC;
	lsg->start = (int)i;
	lsg->end = (int)i + 1;
	skt.curve.segment[i] = (void *)lsg;
    }

    /* Connect the last connected vertex to the first vertex */
    BU_ALLOC(lsg, struct line_seg);
    lsg->magic = CURVE_LSEG_MAGIC;
    lsg->start = (int)num - 1;
    lsg->end = 0;
    skt.curve.segment[num - 1] = (void *)lsg;

    /* write out sketch shape */
    bu_vls_sprintf(&str_sketch, "shape-%zu.sketch", idx);
    mk_sketch(fd, bu_vls_addr(&str_sketch), &skt);

    /* extrude the shape */
    bu_vls_sprintf(&str_extrude, "shape-%zu.extrude", idx);
    VSET(h, 0.0, 0.0, 10.0); /* FIXME: arbitrary height */
    mk_extrusion(fd, bu_vls_addr(&str_extrude), bu_vls_addr(&str_sketch), skt.V, h, skt.u_vec, skt.v_vec, 0);

    /* clean up  */
    bu_vls_free(&str_sketch);
    bu_vls_free(&str_extrude);
    rt_curve_free(&skt.curve);

    return 0;
}


int
main(int argc, char *argv[])
{
    struct shp_g_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    const char * const *operands;
    int introspection;
    int operand_index;
    size_t i;
    const char *argv0 = argv[0];
    struct rt_wdb *fd_out;
    struct bu_vls vls_in = BU_VLS_INIT_ZERO;
    struct bu_vls vls_out = BU_VLS_INIT_ZERO;

    int opt_debug = 0;
    int opt_verbose = 0;

    /* shapelib vars */
    SHPHandle shapefile;
    size_t shp_num_invalid = 0;
    int shp_num_entities = 0;
    int shp_type = 0;

    /* intentionally double for scan */
    double shp_min[4] = HINIT_ZERO;
    double shp_max[4] = HINIT_ZERO;

    /* geometry */
    point2d_t *verts = NULL;
    size_t num_verts = 0;

    bu_setprogname(argv[0]);

    introspection = bu_arg_introspect(&msg, &shp_g_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    operand_index = bu_arg_schema_parse_complete(&shp_g_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return shp_g_help(stdout, 0);

    opt_debug = args.debug;
    opt_verbose = args.verbose;
    if (args.rt_debug.set) {
	rt_debug = args.rt_debug.value;
	bu_printb("librt RT_G_DEBUG", RT_G_DEBUG, RT_DEBUG_FORMAT);
	bu_log("\n");
    }
    operands = (const char * const *)(argv + operand_index + 1);

    if (opt_verbose)
	bu_log("Verbose output enabled.\n");

    if (opt_debug)
	bu_log("Debugging output enabled.\n");

    bu_vls_strcat(&vls_in, operands[0]);

    if (argc - operand_index - 1 < 2) {
	bu_vls_printf(&vls_out, "%s.g", operands[0]);
    } else {
	bu_vls_strcat(&vls_out, operands[1]);
    }

    if (opt_verbose) {
	bu_log("Reading from [%s]\n", bu_vls_addr(&vls_in));
	bu_log("Writing to [%s]\n\n", bu_vls_addr(&vls_out));
    }

    /* open the input */
    shapefile = SHPOpen(bu_vls_addr(&vls_in), "rb");
    if (!shapefile) {
	bu_log("ERROR: Unable to open shapefile [%s]\n", bu_vls_addr(&vls_in));
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
	bu_log("ERROR: Unable to open shapefile [%s]\n", bu_vls_addr(&vls_out));
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

	object = SHPReadObject(shapefile, (int)i);
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
	verts = (point2d_t *)bu_calloc((size_t)object->nVertices, sizeof(point2d_t), "alloc point array");

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
    db_close(fd_out->dbip);

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
