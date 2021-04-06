/*                       P O L Y G O N S . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2021 United States Government as represented by
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
/** @addtogroup libtclcad */
/** @{ */
/**
 *
 * A quasi-object-oriented database interface.
 *
 * A GED object contains the attributes and methods for controlling a
 * BRL-CAD geometry edit object.
 *
 */
/** @} */

#include "common.h"

#include "tclcad.h"

/* Private headers */
#include "./tclcad_private.h"
#include "./view/view.h"

/*
 * Weird upper limit from clipper ---> sqrt(2^63 -1)/2
 * Probably should be sqrt(2^63 -1)
 */
#define CLIPPER_MAX 1518500249


/* These functions should be macros */
static void
to_polygon_free(struct bg_polygon *gpp)
{
    register size_t j;

    if (gpp->num_contours == 0)
	return;

    for (j = 0; j < gpp->num_contours; ++j)
	if (gpp->contour[j].num_points > 0)
	    bu_free((void *)gpp->contour[j].point, "contour points");

    bu_free((void *)gpp->contour, "contour");
    bu_free((void *)gpp->hole, "hole");
    gpp->num_contours = 0;
}


static void
to_polygons_free(struct bg_polygons *gpp)
{
    register size_t i;

    if (gpp->num_polygons == 0)
	return;

    for (i = 0; i < gpp->num_polygons; ++i) {
	to_polygon_free(&gpp->polygon[i]);
    }

    bu_free((void *)gpp->polygon, "data polygons");
    gpp->polygon = (struct bg_polygon *)0;
    gpp->num_polygons = 0;
}


static int
to_extract_contours_av(Tcl_Interp *interp, struct ged *gedp, struct bview *gdvp, struct bg_polygon *gpp, size_t contour_ac, const char **contour_av, int mode, int vflag)
{
    register size_t j = 0, k = 0;

    gpp->num_contours = contour_ac;
    gpp->hole = NULL;
    gpp->contour = NULL;

    if (contour_ac == 0)
	return GED_OK;

    gpp->hole = (int *)bu_calloc(contour_ac, sizeof(int), "hole");
    gpp->contour = (struct bg_poly_contour *)bu_calloc(contour_ac, sizeof(struct bg_poly_contour), "contour");

    for (j = 0; j < contour_ac; ++j) {
	int ac;
	size_t point_ac;
	const char **point_av;
	int hole;

	/* Split contour j into points */
	if (Tcl_SplitList(interp, contour_av[j], &ac, &point_av) != TCL_OK) {
	    bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(interp));
	    return GED_ERROR;
	}
	point_ac = ac;

	/* point_ac includes a hole flag */
	if (mode != TCLCAD_POLY_CONTOUR_MODE && point_ac < 4) {
	    bu_vls_printf(gedp->ged_result_str, "There must be at least 3 points per contour");
	    Tcl_Free((char *)point_av);
	    return GED_ERROR;
	}

	gpp->contour[j].num_points = point_ac - 1;
	gpp->contour[j].point = (point_t *)bu_calloc(point_ac, sizeof(point_t), "point");

	if (bu_sscanf(point_av[0], "%d", &hole) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "contour %zu, point %zu: bad hole flag - %s\n",
			  j, k, point_av[k]);
	    Tcl_Free((char *)point_av);
	    return GED_ERROR;
	}
	gpp->hole[j] = hole;

	for (k = 1; k < point_ac; ++k) {
	    double pt[ELEMENTS_PER_POINT]; /* must be double for scanf */

	    if (bu_sscanf(point_av[k], "%lf %lf %lf", &pt[X], &pt[Y], &pt[Z]) != 3) {
		bu_vls_printf(gedp->ged_result_str, "contour %zu, point %zu: bad data point - %s\n",
			      j, k, point_av[k]);
		Tcl_Free((char *)point_av);
		return GED_ERROR;
	    }

	    if (vflag) {
		MAT4X3PNT(gpp->contour[j].point[k-1], gdvp->gv_view2model, pt);
	    } else {
		VMOVE(gpp->contour[j].point[k-1], pt);
	    }

	}

	Tcl_Free((char *)point_av);
    }

    return GED_OK;
}


static int
to_extract_polygons_av(Tcl_Interp *interp, struct ged *gedp, struct bview *gdvp, bview_data_polygon_state *gdpsp, size_t polygon_ac, const char **polygon_av, int mode, int vflag)
{
    register size_t i;
    int ac;

    gdpsp->gdps_polygons.num_polygons = polygon_ac;
    gdpsp->gdps_polygons.polygon = (struct bg_polygon *)bu_calloc(polygon_ac, sizeof(struct bg_polygon), "data polygons");
    for (i = 0; i < polygon_ac; ++i) {
	// TODO - allocate properties containers for each polygon
    }

    for (i = 0; i < polygon_ac; ++i) {
	size_t contour_ac;
	const char **contour_av;

	/* Split polygon i into contours */
	if (Tcl_SplitList(interp, polygon_av[i], &ac, &contour_av) != TCL_OK) {
	    bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(interp));
	    return GED_ERROR;
	}
	contour_ac = ac;

	if (to_extract_contours_av(interp, gedp, gdvp, &gdpsp->gdps_polygons.polygon[i], contour_ac, contour_av, mode, vflag) != GED_OK) {
	    Tcl_Free((char *)contour_av);
	    return GED_ERROR;
	}

	VMOVE(gdpsp->gdps_polygons.polygon[i].gp_color, gdpsp->gdps_color);
	if (contour_ac)
	    Tcl_Free((char *)contour_av);
    }

    return GED_OK;
}



int
to_data_polygons_func(Tcl_Interp *interp,
		      struct ged *gedp,
		      struct bview *gdvp,
		      int argc,
		      const char *argv[])
{
    bview_data_polygon_state *gdpsp;

    if (argv[0][0] == 's')
	gdpsp = &gdvp->gv_sdata_polygons;
    else
	gdpsp = &gdvp->gv_data_polygons;

    gdpsp->gdps_scale = gdvp->gv_scale;
    gdpsp->gdps_data_vZ = gdvp->gv_data_vZ;
    VMOVE(gdpsp->gdps_origin, gdvp->gv_center);
    MAT_COPY(gdpsp->gdps_rotation, gdvp->gv_rotation);
    MAT_COPY(gdpsp->gdps_model2view, gdvp->gv_model2view);
    MAT_COPY(gdpsp->gdps_view2model, gdvp->gv_view2model);

    if (BU_STR_EQUAL(argv[1], "target_poly")) {
	if (argc == 2) {
	    bu_vls_printf(gedp->ged_result_str, "%lu", gdpsp->gdps_target_polygon_i);
	    return GED_OK;
	}

	if (argc == 3) {
	    size_t i;

	    if (bu_sscanf(argv[2], "%zu", &i) != 1 || i > gdpsp->gdps_polygons.num_polygons)
		goto bad;

	    gdpsp->gdps_target_polygon_i = i;

	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[1], "clip_type")) {
	if (argc == 2) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdpsp->gdps_clip_type);
	    return GED_OK;
	}

	if (argc == 3) {
	    int op;

	    if (bu_sscanf(argv[2], "%d", &op) != 1 || op > bg_Xor)
		goto bad;

	    gdpsp->gdps_clip_type = (bg_clip_t)op;

	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[1], "draw")) {
	if (argc == 2) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdpsp->gdps_draw);
	    return GED_OK;
	}

	if (argc == 3) {
	    int i;

	    if (bu_sscanf(argv[2], "%d", &i) != 1)
		goto bad;

	    if (i)
		gdpsp->gdps_draw = 1;
	    else
		gdpsp->gdps_draw = 0;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[1], "moveall")) {
	if (argc == 2) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdpsp->gdps_moveAll);
	    return GED_OK;
	}

	if (argc == 3) {
	    int i;

	    if (bu_sscanf(argv[2], "%d", &i) != 1)
		goto bad;

	    if (i)
		gdpsp->gdps_moveAll = 1;
	    else
		gdpsp->gdps_moveAll = 0;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    /* Usage: poly_color i [r g b]
     *
     * Set/get the color of polygon i.
     */
    if (BU_STR_EQUAL(argv[1], "poly_color")) {
	size_t i;

	if (argc == 3) {
	    /* Get the color for polygon i */
	    if (bu_sscanf(argv[2], "%zu", &i) != 1 ||
		i >= gdpsp->gdps_polygons.num_polygons)
		goto bad;

	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gdpsp->gdps_polygons.polygon[i].gp_color));

	    return GED_OK;
	}

	if (argc == 6) {
	    int r, g, b;


	    if (bu_sscanf(argv[2], "%zu", &i) != 1 ||
		i >= gdpsp->gdps_polygons.num_polygons)
		goto bad;

	    /* set background color */
	    if (bu_sscanf(argv[3], "%d", &r) != 1 ||
		bu_sscanf(argv[4], "%d", &g) != 1 ||
		bu_sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    /* Set the color for polygon i */
	    VSET(gdpsp->gdps_polygons.polygon[i].gp_color, r, g, b);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    /* Usage: color [r g b]
     *
     * Set the color of all polygons, or get the default polygon color.
     */
    if (BU_STR_EQUAL(argv[1], "color")) {
	size_t i;

	if (argc == 2) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gdpsp->gdps_color));

	    return GED_OK;
	}

	if (argc == 5) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[2], "%d", &r) != 1 ||
		bu_sscanf(argv[3], "%d", &g) != 1 ||
		bu_sscanf(argv[4], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    /* Set the color for all polygons */
	    for (i = 0; i < gdpsp->gdps_polygons.num_polygons; ++i) {
		VSET(gdpsp->gdps_polygons.polygon[i].gp_color, r, g, b);
	    }

	    /* Set the default polygon color */
	    VSET(gdpsp->gdps_color, r, g, b);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    /* Usage: poly_line_width i [w]
     *
     * Set/get the line width of polygon i.
     */
    if (BU_STR_EQUAL(argv[1], "poly_line_width")) {
	size_t i;

	if (argc == 3) {
	    /* Get the line width for polygon i */
	    if (bu_sscanf(argv[2], "%zu", &i) != 1 ||
		i >= gdpsp->gdps_polygons.num_polygons)
		goto bad;

	    bu_vls_printf(gedp->ged_result_str, "%d", gdpsp->gdps_polygons.polygon[i].gp_line_width);

	    return GED_OK;
	}

	if (argc == 4) {
	    int line_width;

	    if (bu_sscanf(argv[2], "%zu", &i) != 1 ||
		i >= gdpsp->gdps_polygons.num_polygons)
		goto bad;

	    if (bu_sscanf(argv[3], "%d", &line_width) != 1)
		goto bad;

	    if (line_width < 0)
		line_width = 0;

	    gdpsp->gdps_polygons.polygon[i].gp_line_width = line_width;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    /* Usage: line_width [w]
     *
     * Set the line width of all polygons, or get the default polygon line width.
     */
    if (BU_STR_EQUAL(argv[1], "line_width")) {
	size_t i;

	if (argc == 2) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdpsp->gdps_line_width);
	    return GED_OK;
	}

	if (argc == 3) {
	    int line_width;

	    if (bu_sscanf(argv[2], "%d", &line_width) != 1)
		goto bad;

	    if (line_width < 0)
		line_width = 0;

	    /* Set the line width for all polygons */
	    for (i = 0; i < gdpsp->gdps_polygons.num_polygons; ++i) {
		gdpsp->gdps_polygons.polygon[i].gp_line_width = line_width;
	    }

	    /* Set the default line width */
	    gdpsp->gdps_line_width = line_width;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    /* Usage: poly_line_style i [w]
     *
     * Set/get the line style of polygon i.
     */
    if (BU_STR_EQUAL(argv[1], "poly_line_style")) {
	size_t i;

	if (argc == 3) {
	    /* Get the line style for polygon i */
	    if (bu_sscanf(argv[2], "%zu", &i) != 1 ||
		i >= gdpsp->gdps_polygons.num_polygons)
		goto bad;

	    bu_vls_printf(gedp->ged_result_str, "%d", gdpsp->gdps_polygons.polygon[i].gp_line_style);

	    return GED_OK;
	}

	if (argc == 4) {
	    int line_style;

	    if (bu_sscanf(argv[2], "%zu", &i) != 1 ||
		i >= gdpsp->gdps_polygons.num_polygons)
		goto bad;

	    if (bu_sscanf(argv[3], "%d", &line_style) != 1)
		goto bad;


	    if (line_style <= 0)
		gdpsp->gdps_polygons.polygon[i].gp_line_style = 0;
	    else
		gdpsp->gdps_polygons.polygon[i].gp_line_style = 1;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    /* Usage: line_style [w]
     *
     * Set the line style of all polygons, or get the default polygon line style.
     */
    if (BU_STR_EQUAL(argv[1], "line_style")) {
	size_t i;

	if (argc == 2) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gdpsp->gdps_line_style);
	    return GED_OK;
	}

	if (argc == 3) {
	    int line_style;

	    if (bu_sscanf(argv[2], "%d", &line_style) != 1)
		goto bad;

	    if (line_style <= 0)
		line_style = 0;
	    else
		line_style = 1;

	    /* Set the line width for all polygons */
	    for (i = 0; i < gdpsp->gdps_polygons.num_polygons; ++i) {
		gdpsp->gdps_polygons.polygon[i].gp_line_style = line_style;
	    }

	    /* Set the default line style */
	    gdpsp->gdps_line_style = line_style;

	    to_refresh_view(gdvp);
	    return GED_OK;
	}

	goto bad;
    }

    /* Usage: append_poly plist
     *
     * Append the polygon specified by plist.
     */
    if (BU_STR_EQUAL(argv[1], "append_poly")) {
	if (argc != 3)
	    goto bad;

	if (argc == 3) {
	    register size_t i;
	    int ac;
	    size_t contour_ac;
	    const char **contour_av;

	    /* Split the polygon in argv[2] into contours */
	    if (Tcl_SplitList(interp, argv[2], &ac, &contour_av) != TCL_OK ||
		ac < 1) {
		bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(interp));
		return GED_ERROR;
	    }
	    contour_ac = ac;

	    i = gdpsp->gdps_polygons.num_polygons;
	    ++gdpsp->gdps_polygons.num_polygons;
	    gdpsp->gdps_polygons.polygon = (struct bg_polygon *)bu_realloc(gdpsp->gdps_polygons.polygon,
									  gdpsp->gdps_polygons.num_polygons * sizeof(struct bg_polygon),
									  "realloc polygon");

	    if (to_extract_contours_av(interp, gedp, gdvp, &gdpsp->gdps_polygons.polygon[i],
				       contour_ac, contour_av, gdvp->gv_mode, 0) != GED_OK) {
		Tcl_Free((char *)contour_av);
		return GED_ERROR;
	    }

	    VMOVE(gdpsp->gdps_polygons.polygon[i].gp_color, gdpsp->gdps_color);
	    gdpsp->gdps_polygons.polygon[i].gp_line_style = gdpsp->gdps_line_style;
	    gdpsp->gdps_polygons.polygon[i].gp_line_width = gdpsp->gdps_line_width;

	    Tcl_Free((char *)contour_av);

	    to_refresh_view(gdvp);
	    return GED_OK;
	}
    }


    /* Usage: clip [i j [op]]
     *
     * Clip polygon i using polygon j and op.
     */
    if (BU_STR_EQUAL(argv[1], "clip")) {
	size_t i, j;
	int op;
	struct bg_polygon *gpp;

	if (argc > 5)
	    goto bad;

	if (argc > 2) {
	    if (bu_sscanf(argv[2], "%zu", &i) != 1 ||
		i >= gdpsp->gdps_polygons.num_polygons)
		goto bad;
	} else
	    i = gdpsp->gdps_target_polygon_i;

	if (argc > 3) {
	    if (bu_sscanf(argv[3], "%zu", &j) != 1 ||
		j >= gdpsp->gdps_polygons.num_polygons)
		goto bad;
	} else
	    j = gdpsp->gdps_polygons.num_polygons - 1; /* Default - use last polygon as the clip polygon */

	/* Nothing to do */
	if (i == j)
	    return GED_OK;

	if (argc != 5)
	    op = gdpsp->gdps_clip_type;
	else if (bu_sscanf(argv[4], "%d", &op) != 1 || op > bg_Xor)
	    goto bad;

	gpp = bg_clip_polygon((bg_clip_t)op,
			       &gdpsp->gdps_polygons.polygon[i],
			       &gdpsp->gdps_polygons.polygon[j],
			       CLIPPER_MAX,
			       gdpsp->gdps_model2view,
			       gdpsp->gdps_view2model);

	/* Free the target polygon */
	to_polygon_free(&gdpsp->gdps_polygons.polygon[i]);

	/* When using defaults, the clip polygon is assumed to be temporary and is removed after clipping */
	if (argc == 2) {
	    /* Free the clip polygon */
	    to_polygon_free(&gdpsp->gdps_polygons.polygon[j]);

	    /* No longer need space for the clip polygon */
	    --gdpsp->gdps_polygons.num_polygons;
	    gdpsp->gdps_polygons.polygon = (struct bg_polygon *)bu_realloc(gdpsp->gdps_polygons.polygon,
									  gdpsp->gdps_polygons.num_polygons * sizeof(struct bg_polygon),
									  "realloc polygon");
	}

	/* Replace the target polygon with the newly clipped polygon. */
	/* Not doing a struct copy to avoid overwriting the color, line width and line style. */
	gdpsp->gdps_polygons.polygon[i].num_contours = gpp->num_contours;
	gdpsp->gdps_polygons.polygon[i].hole = gpp->hole;
	gdpsp->gdps_polygons.polygon[i].contour = gpp->contour;

	/* Free the clipped polygon container */
	bu_free((void *)gpp, "clip gpp");

	to_refresh_view(gdvp);
	return GED_OK;
    }

    /* Usage: export i sketch_name
     *
     * Export polygon i to sketch_name
     */
    if (BU_STR_EQUAL(argv[1], "export")) {
	size_t i;
	int ret;

	if (argc != 4)
	    goto bad;

	if (bu_sscanf(argv[2], "%zu", &i) != 1 ||
	    i >= gdpsp->gdps_polygons.num_polygons)
	    goto bad;

	if ((ret = ged_export_polygon(gedp, gdpsp, i, argv[3])) != GED_OK)
	    bu_vls_printf(gedp->ged_result_str, "%s: failed to export polygon %lu to %s", argv[0], i, argv[3]);

	return ret;
    }

    /* Usage: import sketch_name
     *
     * Import sketch_name and append
     */
    if (BU_STR_EQUAL(argv[1], "import")) {
	struct bg_polygon *gpp;
	size_t i;

	if (argc != 3)
	    goto bad;

	if ((gpp = ged_import_polygon(gedp, argv[2])) == (struct bg_polygon *)0) {
	    bu_vls_printf(gedp->ged_result_str, "%s: failed to import sketch %s", argv[0], argv[2]);
	    return GED_ERROR;
	}

	i = gdpsp->gdps_polygons.num_polygons;
	++gdpsp->gdps_polygons.num_polygons;
	gdpsp->gdps_polygons.polygon = (struct bg_polygon *)bu_realloc(gdpsp->gdps_polygons.polygon,
								      gdpsp->gdps_polygons.num_polygons * sizeof(struct bg_polygon),
								      "realloc polygon");

	gdpsp->gdps_polygons.polygon[i] = *gpp;  /* struct copy */
	VMOVE(gdpsp->gdps_polygons.polygon[i].gp_color, gdpsp->gdps_color);
	gdpsp->gdps_polygons.polygon[i].gp_line_style = gdpsp->gdps_line_style;
	gdpsp->gdps_polygons.polygon[i].gp_line_width = gdpsp->gdps_line_width;

	to_refresh_view(gdvp);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[1], "fill")) {
	size_t i;
	vect2d_t vdir;
	fastf_t vdelta;

	if (argc != 5)
	    goto bad;

	if (bu_sscanf(argv[2], "%zu", &i) != 1 ||
	    i >= gdpsp->gdps_polygons.num_polygons)
	    goto bad;

	if (bu_sscanf(argv[3], "%lf %lf", &vdir[X], &vdir[Y]) != 2) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad dir: %s", argv[0], argv[3]);
	    goto bad;
	}

	if (bu_sscanf(argv[4], "%lf", &vdelta) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad delta: %s", argv[0], argv[4]);
	    goto bad;
	}

	ged_polygon_fill_segments(gedp, &gdpsp->gdps_polygons.polygon[i], vdir, vdelta);

	return GED_OK;
    }

    /* Usage: area i
     *
     * Find area of polygon i
     */
    if (BU_STR_EQUAL(argv[1], "area")) {
	size_t i;
	double area;

	if (argc != 3)
	    goto bad;

	if (bu_sscanf(argv[2], "%zu", &i) != 1 ||
	    i >= gdpsp->gdps_polygons.num_polygons)
	    goto bad;

	area = bg_find_polygon_area(&gdpsp->gdps_polygons.polygon[i], CLIPPER_MAX,
				     gdpsp->gdps_model2view, gdpsp->gdps_scale);
	bu_vls_printf(gedp->ged_result_str, "%lf", area);

	return GED_OK;
    }

    /* Usage: polygons_overlap i j
     *
     * Do polygons i and j overlap?
     */
    if (BU_STR_EQUAL(argv[1], "polygons_overlap")) {
	size_t i, j;
	int ret;

	if (argc != 4)
	    goto bad;

	if (bu_sscanf(argv[2], "%zu", &i) != 1 ||
	    i >= gdpsp->gdps_polygons.num_polygons)
	    goto bad;

	if (bu_sscanf(argv[3], "%zu", &j) != 1 ||
	    j >= gdpsp->gdps_polygons.num_polygons)
	    goto bad;

	ret = ged_polygons_overlap(gedp, &gdpsp->gdps_polygons.polygon[i], &gdpsp->gdps_polygons.polygon[j]);
	bu_vls_printf(gedp->ged_result_str, "%d", ret);

	return GED_OK;
    }

    /* Usage: [v]polygons [poly_list]
     *
     * Set/get the polygon list. If vpolygons is specified then
     * the polygon list is in view coordinates.
     */
    if (BU_STR_EQUAL(argv[1], "polygons") || BU_STR_EQUAL(argv[1], "vpolygons")) {
	register size_t i, j, k;
	int ac;
	int vflag;

	if (BU_STR_EQUAL(argv[1], "polygons"))
	    vflag = 0;
	else
	    vflag = 1;
	if (argc == 2) {
	    for (i = 0; i < gdpsp->gdps_polygons.num_polygons; ++i) {
		bu_vls_printf(gedp->ged_result_str, " {");

		for (j = 0; j < gdpsp->gdps_polygons.polygon[i].num_contours; ++j) {
		    bu_vls_printf(gedp->ged_result_str, " {%d ", gdpsp->gdps_polygons.polygon[i].hole[j]);

		    for (k = 0; k < gdpsp->gdps_polygons.polygon[i].contour[j].num_points; ++k) {
			point_t pt;

			if (vflag) {
			    MAT4X3PNT(pt, gdvp->gv_model2view, gdpsp->gdps_polygons.polygon[i].contour[j].point[k]);
			} else {
			    VMOVE(pt, gdpsp->gdps_polygons.polygon[i].contour[j].point[k]);
			}

			bu_vls_printf(gedp->ged_result_str, " {%lf %lf %lf} ", V3ARGS(pt));
		    }

		    bu_vls_printf(gedp->ged_result_str, "} ");
		}

		bu_vls_printf(gedp->ged_result_str, "} ");
	    }

	    return GED_OK;
	}

	if (argc == 3) {
	    size_t polygon_ac;
	    const char **polygon_av;

	    /* Split into polygons */
	    if (Tcl_SplitList(interp, argv[2], &ac, &polygon_av) != TCL_OK) {
		bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(interp));
		return GED_ERROR;
	    }
	    polygon_ac = ac;

	    to_polygons_free(&gdpsp->gdps_polygons);
	    gdpsp->gdps_target_polygon_i = 0;

	    if (polygon_ac < 1) {
		to_refresh_view(gdvp);
		return GED_OK;
	    }

	    if (to_extract_polygons_av(interp, gedp, gdvp, gdpsp, polygon_ac, polygon_av, gdvp->gv_mode, vflag) != GED_OK) {
		Tcl_Free((char *)polygon_av);
		return GED_ERROR;
	    }

	    Tcl_Free((char *)polygon_av);
	    to_refresh_view(gdvp);
	    return GED_OK;
	}
    }

    /* Usage: replace_poly i plist
     *
     * Replace polygon i with plist.
     */
    if (BU_STR_EQUAL(argv[1], "replace_poly")) {
	size_t i;
	int ac;
	size_t contour_ac;
	const char **contour_av;
	struct bg_polygon gp;

	if (argc != 4)
	    goto bad;

	if (bu_sscanf(argv[2], "%zu", &i) != 1 ||
	    i >= gdpsp->gdps_polygons.num_polygons)
	    goto bad;

	/* Split the polygon in argv[3] into contours */
	if (Tcl_SplitList(interp, argv[3], &ac, &contour_av) != TCL_OK ||
	    ac < 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(interp));
	    return GED_ERROR;
	}
	contour_ac = ac;

	if (to_extract_contours_av(interp, gedp, gdvp, &gp, contour_ac, contour_av, gdvp->gv_mode, 0) != GED_OK) {
	    Tcl_Free((char *)contour_av);
	    return GED_ERROR;
	}

	to_polygon_free(&gdpsp->gdps_polygons.polygon[i]);

	/* Not doing a struct copy to avoid overwriting the color, line width and line style. */
	gdpsp->gdps_polygons.polygon[i].num_contours = gp.num_contours;
	gdpsp->gdps_polygons.polygon[i].hole = gp.hole;
	gdpsp->gdps_polygons.polygon[i].contour = gp.contour;

	to_refresh_view(gdvp);
	return GED_OK;
    }

    /* Usage: append_point i j pt
     *
     * Append pt to polygon_i/contour_j
     */
    if (BU_STR_EQUAL(argv[1], "append_point")) {
	size_t i, j, k;
	double pt[ELEMENTS_PER_POINT]; /* must be double for scan */

	if (argc != 5)
	    goto bad;

	if (bu_sscanf(argv[2], "%zu", &i) != 1 ||
	    i >= gdpsp->gdps_polygons.num_polygons)
	    goto bad;

	if (bu_sscanf(argv[3], "%zu", &j) != 1 ||
	    j >= gdpsp->gdps_polygons.polygon[i].num_contours)
	    goto bad;

	if (bu_sscanf(argv[4], "%lf %lf %lf", &pt[X], &pt[Y], &pt[Z]) != 3)
	    goto bad;

	k = gdpsp->gdps_polygons.polygon[i].contour[j].num_points;
	++gdpsp->gdps_polygons.polygon[i].contour[j].num_points;
	gdpsp->gdps_polygons.polygon[i].contour[j].point = (point_t *)bu_realloc(gdpsp->gdps_polygons.polygon[i].contour[j].point,
											   gdpsp->gdps_polygons.polygon[i].contour[j].num_points * sizeof(point_t),
											   "realloc point");
	VMOVE(gdpsp->gdps_polygons.polygon[i].contour[j].point[k], pt);
	to_refresh_view(gdvp);
	return GED_OK;
    }

    /* Usage: get_point i j k
     *
     * Get polygon_i/contour_j/point_k
     */
    if (BU_STR_EQUAL(argv[1], "get_point")) {
	size_t i, j, k;

	if (argc != 5)
	    goto bad;

	if (bu_sscanf(argv[2], "%zu", &i) != 1 ||
	    i >= gdpsp->gdps_polygons.num_polygons)
	    goto bad;

	if (bu_sscanf(argv[3], "%zu", &j) != 1 ||
	    j >= gdpsp->gdps_polygons.polygon[i].num_contours)
	    goto bad;

	if (bu_sscanf(argv[4], "%zu", &k) != 1 ||
	    k >= gdpsp->gdps_polygons.polygon[i].contour[j].num_points)
	    goto bad;

	bu_vls_printf(gedp->ged_result_str, "%lf %lf %lf", V3ARGS(gdpsp->gdps_polygons.polygon[i].contour[j].point[k]));
	return GED_OK;
    }

    /* Usage: replace_point i j k pt
     *
     * Replace polygon_i/contour_j/point_k with pt
     */
    if (BU_STR_EQUAL(argv[1], "replace_point")) {
	size_t i, j, k;
	double pt[ELEMENTS_PER_POINT];

	if (argc != 6)
	    goto bad;

	if (bu_sscanf(argv[2], "%zu", &i) != 1 ||
	    i >= gdpsp->gdps_polygons.num_polygons)
	    goto bad;

	if (bu_sscanf(argv[3], "%zu", &j) != 1 ||
	    j >= gdpsp->gdps_polygons.polygon[i].num_contours)
	    goto bad;

	if (bu_sscanf(argv[4], "%zu", &k) != 1 ||
	    k >= gdpsp->gdps_polygons.polygon[i].contour[j].num_points)
	    goto bad;

	if (bu_sscanf(argv[5], "%lf %lf %lf", &pt[X], &pt[Y], &pt[Z]) != 3)
	    goto bad;

	VMOVE(gdpsp->gdps_polygons.polygon[i].contour[j].point[k], pt);
	to_refresh_view(gdvp);
	return GED_OK;
    }

bad:

    return GED_ERROR;
}

int
go_data_polygons(Tcl_Interp *interp,
		 struct ged *gedp,
		 struct bview *gdvp,
		 int argc,
		 const char *argv[],
		 const char *usage)
{
    int ret;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 2 || 6 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Don't allow go_refresh() to be called */
    if (current_top != NULL) {
	struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
	tgd->go_dmv.refresh_on = 0;
    }

    ret = to_data_polygons_func(interp, gedp, gdvp, argc, argv);
    if (ret & GED_ERROR)
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);

    return ret;
}


int
to_data_polygons(struct ged *gedp,
		 int argc,
		 const char *argv[],
		 ged_func_ptr UNUSED(func),
		 const char *usage,
		 int UNUSED(maxargs))
{
    struct bview *gdvp;
    int ret;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 7 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* shift the command name to argv[1] before calling to_data_polygons_func */
    argv[1] = argv[0];
    ret = to_data_polygons_func(current_top->to_interp, gedp, gdvp, argc-1, argv+1);
    if (ret == GED_ERROR) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    }

    return ret;
}



int
go_poly_circ_mode(Tcl_Interp *interp,
		  struct ged *gedp,
		  struct bview *gdvp,
		  int argc,
		  const char *argv[],
		  const char *usage)
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Don't allow go_refresh() to be called */
    if (current_top != NULL) {
	struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
	tgd->go_dmv.refresh_on = 0;
    }

    return to_poly_circ_mode_func(interp, gedp, gdvp, argc, argv, usage);
}


int
to_poly_circ_mode(struct ged *gedp,
		  int argc,
		  const char *argv[],
		  ged_func_ptr UNUSED(func),
		  const char *usage,
		  int UNUSED(maxargs))
{
    struct bview *gdvp;
    struct bu_vls bindings = BU_VLS_INIT_ZERO;
    int ret;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* shift the command name to argv[1] before calling to_poly_circ_mode_func */
    argv[1] = argv[0];
    ret = to_poly_circ_mode_func(current_top->to_interp, gedp, gdvp, argc-1, argv+1, usage);

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_poly_circ %s %%x %%y}",
		      bu_vls_cstr(pathname),
		      bu_vls_cstr(&current_top->to_gedp->go_name),
		      bu_vls_cstr(&gdvp->gv_name));
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    to_refresh_view(gdvp);

    return ret;
}


int
to_poly_circ_mode_func(Tcl_Interp *interp,
		       struct ged *gedp,
		       struct bview *gdvp,
		       int UNUSED(argc),
		       const char *argv[],
		       const char *usage)
{
    int ac;
    char *av[4];
    int x, y;
    fastf_t fx, fy;
    point_t v_pt, m_pt;
    struct bu_vls plist = BU_VLS_INIT_ZERO;
    bview_data_polygon_state *gdpsp;

    if (argv[0][0] == 's')
	gdpsp = &gdvp->gv_sdata_polygons;
    else
	gdpsp = &gdvp->gv_data_polygons;

    gdpsp->gdps_scale = gdvp->gv_scale;
    VMOVE(gdpsp->gdps_origin, gdvp->gv_center);
    MAT_COPY(gdpsp->gdps_rotation, gdvp->gv_rotation);
    MAT_COPY(gdpsp->gdps_model2view, gdvp->gv_model2view);
    MAT_COPY(gdpsp->gdps_view2model, gdvp->gv_view2model);

    gedp->ged_gvp = gdvp;

    if (bu_sscanf(argv[1], "%d", &x) != 1 ||
	bu_sscanf(argv[2], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_POLY_CIRCLE_MODE;

    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);

    fx = screen_to_view_x((struct dm *)gdvp->dmp, x);
    fy = screen_to_view_y((struct dm *)gdvp->dmp, y);
    VSET(v_pt, fx, fy, gdvp->gv_data_vZ);
    int snapped = 0;
    if (gedp->ged_gvp->gv_snap_lines) {
	snapped = ged_snap_to_lines(gedp, &v_pt[X], &v_pt[Y]);
    }
    if (!snapped && gedp->ged_gvp->gv_grid.snap) {
	ged_snap_to_grid(gedp, &v_pt[X], &v_pt[Y]);
    }

    MAT4X3PNT(m_pt, gdvp->gv_view2model, v_pt);
    VMOVE(gdpsp->gdps_prev_point, v_pt);

    bu_vls_printf(&plist, "{ {%lf %lf %lf} {%lf %lf %lf} {%lf %lf %lf} {%lf %lf %lf} }",
		  V3ARGS(m_pt), V3ARGS(m_pt), V3ARGS(m_pt), V3ARGS(m_pt));
    ac = 3;
    av[0] = "data_polygons";
    av[1] = "append_poly";
    av[2] = bu_vls_addr(&plist);
    av[3] = (char *)0;

    if (gdpsp->gdps_polygons.num_polygons == gdpsp->gdps_target_polygon_i)
	gdpsp->gdps_curr_polygon_i = gdpsp->gdps_target_polygon_i;
    else
	gdpsp->gdps_curr_polygon_i = gdpsp->gdps_polygons.num_polygons;

    (void)to_data_polygons_func(interp, gedp, gdvp, ac, (const char **)av);
    bu_vls_free(&plist);

    return GED_OK;
}


static int
to_poly_cont_build_func(Tcl_Interp *interp,
			struct ged *gedp,
			struct bview *gdvp,
			int UNUSED(argc),
			const char *argv[],
			const char *usage,
			int doBind)
{
    int ac;
    char *av[7];
    int x, y;
    fastf_t fx, fy;
    point_t v_pt, m_pt;
    bview_data_polygon_state *gdpsp;

    if (argv[0][0] == 's')
	gdpsp = &gdvp->gv_sdata_polygons;
    else
	gdpsp = &gdvp->gv_data_polygons;

    gdpsp->gdps_scale = gdvp->gv_scale;
    VMOVE(gdpsp->gdps_origin, gdvp->gv_center);
    MAT_COPY(gdpsp->gdps_rotation, gdvp->gv_rotation);
    MAT_COPY(gdpsp->gdps_model2view, gdvp->gv_model2view);
    MAT_COPY(gdpsp->gdps_view2model, gdvp->gv_view2model);

    gedp->ged_gvp = gdvp;

    if (bu_sscanf(argv[1], "%d", &x) != 1 ||
	bu_sscanf(argv[2], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_POLY_CONTOUR_MODE;

    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);

    fx = screen_to_view_x((struct dm *)gdvp->dmp, x);
    fy = screen_to_view_y((struct dm *)gdvp->dmp, y);
    VSET(v_pt, fx, fy, gdvp->gv_data_vZ);
    int snapped = 0;
    if (gedp->ged_gvp->gv_snap_lines) {
	snapped = ged_snap_to_lines(gedp, &v_pt[X], &v_pt[Y]);
    }
    if (!snapped && gedp->ged_gvp->gv_grid.snap) {
	ged_snap_to_grid(gedp, &v_pt[X], &v_pt[Y]);
    }

    MAT4X3PNT(m_pt, gdvp->gv_view2model, v_pt);

    av[0] = "data_polygons";
    if (gdpsp->gdps_cflag == 0) {
	struct bu_vls plist = BU_VLS_INIT_ZERO;
	struct bu_vls bindings = BU_VLS_INIT_ZERO;

	if (gdpsp->gdps_polygons.num_polygons == gdpsp->gdps_target_polygon_i)
	    gdpsp->gdps_curr_polygon_i = gdpsp->gdps_target_polygon_i;
	else
	    gdpsp->gdps_curr_polygon_i = gdpsp->gdps_polygons.num_polygons;

	gdpsp->gdps_cflag = 1;
	gdpsp->gdps_curr_point_i = 1;

	bu_vls_printf(&plist, "{ {%lf %lf %lf} {%lf %lf %lf} }", V3ARGS(m_pt), V3ARGS(m_pt));
	ac = 3;
	av[1] = "append_poly";
	av[2] = bu_vls_addr(&plist);
	av[3] = (char *)0;

	(void)to_data_polygons_func(interp, gedp, gdvp, ac, (const char **)av);
	bu_vls_free(&plist);

	struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
	if (doBind && pathname && bu_vls_strlen(pathname)) {
	    bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_poly_cont %s %%x %%y}",
			  bu_vls_cstr(pathname),
			  bu_vls_cstr(&current_top->to_gedp->go_name),
			  bu_vls_cstr(&gdvp->gv_name));
	    Tcl_Eval(interp, bu_vls_cstr(&bindings));
	}
	bu_vls_free(&bindings);
    } else {
	struct bu_vls i_vls = BU_VLS_INIT_ZERO;
	struct bu_vls k_vls = BU_VLS_INIT_ZERO;
	struct bu_vls plist = BU_VLS_INIT_ZERO;

	bu_vls_printf(&i_vls, "%zu", gdpsp->gdps_curr_polygon_i);
	bu_vls_printf(&k_vls, "%zu", gdpsp->gdps_curr_point_i);
	bu_vls_printf(&plist, "%lf %lf %lf", V3ARGS(m_pt));

	ac = 6;
	av[1] = "replace_point";
	av[2] = bu_vls_addr(&i_vls);
	av[3] = "0";
	av[4] = bu_vls_addr(&k_vls);
	av[5] = bu_vls_addr(&plist);
	av[6] = (char *)0;
	(void)to_data_polygons_func(interp, gedp, gdvp, ac, (const char **)av);

	ac = 5;
	av[1] = "append_point";
	av[4] = bu_vls_addr(&plist);
	av[5] = (char *)0;
	(void)to_data_polygons_func(interp, gedp, gdvp, ac, (const char **)av);
	bu_vls_free(&i_vls);
	bu_vls_free(&k_vls);
	bu_vls_free(&plist);

	++gdpsp->gdps_curr_point_i;
    }

    return GED_OK;
}

int
go_poly_cont_build(Tcl_Interp *interp,
		   struct ged *gedp,
		   struct bview *gdvp,
		   int argc,
		   const char *argv[],
		   const char *usage)
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Don't allow go_refresh() to be called */
    if (current_top != NULL) {
	struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
	tgd->go_dmv.refresh_on = 0;
    }

    return to_poly_cont_build_func(interp, gedp, gdvp, argc, argv, usage, 0);
}


int
to_poly_cont_build(struct ged *gedp,
		   int argc,
		   const char *argv[],
		   ged_func_ptr UNUSED(func),
		   const char *usage,
		   int UNUSED(maxargs))
{
    struct bview *gdvp;
    int ret;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* shift the command name to argv[1] before calling to_mouse_poly_ell_func */
    argv[1] = argv[0];
    ret = to_poly_cont_build_func(current_top->to_interp, gedp, gdvp, argc-1, argv+1, usage, 1);

    to_refresh_view(gdvp);

    return ret;
}



int
go_poly_cont_build_end(Tcl_Interp *UNUSED(interp),
		       struct ged *gedp,
		       struct bview *gdvp,
		       int argc,
		       const char *argv[],
		       const char *usage)
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Don't allow go_refresh() to be called */
    if (current_top != NULL) {
	struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
	tgd->go_dmv.refresh_on = 0;
    }

    return to_poly_cont_build_end_func(gdvp, argc, argv);
}


int
to_poly_cont_build_end(struct ged *gedp,
		       int argc,
		       const char *argv[],
		       ged_func_ptr UNUSED(func),
		       const char *usage,
		       int UNUSED(maxargs))
{
    struct bview *gdvp;
    int ret;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* shift the command name to argv[1] before calling to_poly_ell_mode_func */
    argv[1] = argv[0];
    ret = to_poly_cont_build_end_func(gdvp, argc-1, argv+1);

    to_refresh_view(gdvp);
    return ret;
}


int
to_poly_cont_build_end_func(struct bview *gdvp,
			    int UNUSED(argc),
			    const char *argv[])
{
    if (argv[0][0] == 's')
	gdvp->gv_sdata_polygons.gdps_cflag = 0;
    else
	gdvp->gv_data_polygons.gdps_cflag = 0;

    return GED_OK;
}


int
go_poly_ell_mode(Tcl_Interp *interp,
		 struct ged *gedp,
		 struct bview *gdvp,
		 int argc,
		 const char *argv[],
		 const char *usage)
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Don't allow go_refresh() to be called */
    if (current_top != NULL) {
	struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
	tgd->go_dmv.refresh_on = 0;
    }

    return to_poly_ell_mode_func(interp, gedp, gdvp, argc, argv, usage);
}


int
to_poly_ell_mode(struct ged *gedp,
		 int argc,
		 const char *argv[],
		 ged_func_ptr UNUSED(func),
		 const char *usage,
		 int UNUSED(maxargs))
{
    struct bview *gdvp;
    struct bu_vls bindings = BU_VLS_INIT_ZERO;
    int ret;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* shift the command name to argv[1] before calling to_poly_ell_mode_func */
    argv[1] = argv[0];
    ret = to_poly_ell_mode_func(current_top->to_interp, gedp, gdvp, argc-1, argv+1, usage);

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_poly_ell %s %%x %%y}",
		      bu_vls_cstr(pathname),
		      bu_vls_cstr(&current_top->to_gedp->go_name),
		      bu_vls_cstr(&gdvp->gv_name));
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    to_refresh_view(gdvp);

    return ret;
}


int
to_poly_ell_mode_func(Tcl_Interp *interp,
		      struct ged *gedp,
		      struct bview *gdvp,
		      int UNUSED(argc),
		      const char *argv[],
		      const char *usage)
{
    int ac;
    char *av[4];
    int x, y;
    fastf_t fx, fy;
    point_t v_pt, m_pt;
    struct bu_vls plist = BU_VLS_INIT_ZERO;
    bview_data_polygon_state *gdpsp;

    if (argv[0][0] == 's')
	gdpsp = &gdvp->gv_sdata_polygons;
    else
	gdpsp = &gdvp->gv_data_polygons;

    gdpsp->gdps_scale = gdvp->gv_scale;
    VMOVE(gdpsp->gdps_origin, gdvp->gv_center);
    MAT_COPY(gdpsp->gdps_rotation, gdvp->gv_rotation);
    MAT_COPY(gdpsp->gdps_model2view, gdvp->gv_model2view);
    MAT_COPY(gdpsp->gdps_view2model, gdvp->gv_view2model);

    gedp->ged_gvp = gdvp;

    if (bu_sscanf(argv[1], "%d", &x) != 1 ||
	bu_sscanf(argv[2], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;
    gdvp->gv_mode = TCLCAD_POLY_ELLIPSE_MODE;

    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);

    fx = screen_to_view_x((struct dm *)gdvp->dmp, x);
    fy = screen_to_view_y((struct dm *)gdvp->dmp, y);
    VSET(v_pt, fx, fy, gdvp->gv_data_vZ);
    int snapped = 0;
    if (gedp->ged_gvp->gv_snap_lines) {
	snapped = ged_snap_to_lines(gedp, &v_pt[X], &v_pt[Y]);
    }
    if (!snapped && gedp->ged_gvp->gv_grid.snap) {
	ged_snap_to_grid(gedp, &v_pt[X], &v_pt[Y]);
    }

    MAT4X3PNT(m_pt, gdvp->gv_view2model, v_pt);
    VMOVE(gdpsp->gdps_prev_point, v_pt);

    bu_vls_printf(&plist, "{ {%lf %lf %lf} {%lf %lf %lf} {%lf %lf %lf} {%lf %lf %lf} }",
		  V3ARGS(m_pt), V3ARGS(m_pt), V3ARGS(m_pt), V3ARGS(m_pt));
    ac = 3;
    av[0] = "data_polygons";
    av[1] = "append_poly";
    av[2] = bu_vls_addr(&plist);
    av[3] = (char *)0;

    if (gdpsp->gdps_polygons.num_polygons == gdpsp->gdps_target_polygon_i)
	gdpsp->gdps_curr_polygon_i = gdpsp->gdps_target_polygon_i;
    else
	gdpsp->gdps_curr_polygon_i = gdpsp->gdps_polygons.num_polygons;

    (void)to_data_polygons_func(interp, gedp, gdvp, ac, (const char **)av);
    bu_vls_free(&plist);

    return GED_OK;
}


int
go_poly_rect_mode(Tcl_Interp *interp,
		  struct ged *gedp,
		  struct bview *gdvp,
		  int argc,
		  const char *argv[],
		  const char *usage)
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 4 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Don't allow go_refresh() to be called */
    if (current_top != NULL) {
	struct tclcad_ged_data *tgd = (struct tclcad_ged_data *)current_top->to_gedp->u_data;
	tgd->go_dmv.refresh_on = 0;
    }

    return to_poly_rect_mode_func(interp, gedp, gdvp, argc, argv, usage);
}


int
to_poly_rect_mode(struct ged *gedp,
		  int argc,
		  const char *argv[],
		  ged_func_ptr UNUSED(func),
		  const char *usage,
		  int UNUSED(maxargs))
{
    struct bview *gdvp;
    struct bu_vls bindings = BU_VLS_INIT_ZERO;
    int ret;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 4 || 5 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    gdvp = ged_find_view(gedp, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return GED_ERROR;
    }

    /* shift the command name to argv[1] before calling to_poly_rect_mode_func */
    argv[1] = argv[0];
    ret = to_poly_rect_mode_func(current_top->to_interp, gedp, gdvp, argc-1, argv+1, usage);

    struct bu_vls *pathname = dm_get_pathname((struct dm *)gdvp->dmp);
    if (pathname && bu_vls_strlen(pathname)) {
	bu_vls_printf(&bindings, "bind %s <Motion> {%s mouse_poly_rect %s %%x %%y}",
		      bu_vls_cstr(pathname),
		      bu_vls_cstr(&current_top->to_gedp->go_name),
		      bu_vls_cstr(&gdvp->gv_name));
	Tcl_Eval(current_top->to_interp, bu_vls_cstr(&bindings));
    }
    bu_vls_free(&bindings);

    to_refresh_view(gdvp);

    return ret;
}


int
to_poly_rect_mode_func(Tcl_Interp *interp,
		       struct ged *gedp,
		       struct bview *gdvp,
		       int argc,
		       const char *argv[],
		       const char *usage)
{
    int ac;
    char *av[4];
    int x, y;
    int sflag;
    fastf_t fx, fy;
    point_t v_pt, m_pt;
    struct bu_vls plist = BU_VLS_INIT_ZERO;
    bview_data_polygon_state *gdpsp;

    if (argv[0][0] == 's')
	gdpsp = &gdvp->gv_sdata_polygons;
    else
	gdpsp = &gdvp->gv_data_polygons;

    gdpsp->gdps_scale = gdvp->gv_scale;
    VMOVE(gdpsp->gdps_origin, gdvp->gv_center);
    MAT_COPY(gdpsp->gdps_rotation, gdvp->gv_rotation);
    MAT_COPY(gdpsp->gdps_model2view, gdvp->gv_model2view);
    MAT_COPY(gdpsp->gdps_view2model, gdvp->gv_view2model);

    gedp->ged_gvp = gdvp;

    if (bu_sscanf(argv[1], "%d", &x) != 1 ||
	bu_sscanf(argv[2], "%d", &y) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (argc == 3)
	sflag = 0;
    else {
	if (bu_sscanf(argv[3], "%d", &sflag) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}
    }
    gdvp->gv_prevMouseX = x;
    gdvp->gv_prevMouseY = y;

    if (sflag)
	gdvp->gv_mode = TCLCAD_POLY_SQUARE_MODE;
    else
	gdvp->gv_mode = TCLCAD_POLY_RECTANGLE_MODE;

    gdvp->gv_width = dm_get_width((struct dm *)gdvp->dmp);
    gdvp->gv_height = dm_get_height((struct dm *)gdvp->dmp);

    fx = screen_to_view_x((struct dm *)gdvp->dmp, x);
    fy = screen_to_view_y((struct dm *)gdvp->dmp, y);
    VSET(v_pt, fx, fy, gdvp->gv_data_vZ);
    int snapped = 0;
    if (gedp->ged_gvp->gv_snap_lines) {
	snapped = ged_snap_to_lines(gedp, &v_pt[X], &v_pt[Y]);
    }
    if (!snapped && gedp->ged_gvp->gv_grid.snap) {
	ged_snap_to_grid(gedp, &v_pt[X], &v_pt[Y]);
    }

    MAT4X3PNT(m_pt, gdvp->gv_view2model, v_pt);
    VMOVE(gdpsp->gdps_prev_point, v_pt);

    bu_vls_printf(&plist, "{ {%lf %lf %lf} {%lf %lf %lf} {%lf %lf %lf} {%lf %lf %lf} }",
		  V3ARGS(m_pt), V3ARGS(m_pt), V3ARGS(m_pt), V3ARGS(m_pt));
    ac = 3;
    av[0] = "data_polygons";
    av[1] = "append_poly";
    av[2] = bu_vls_addr(&plist);
    av[3] = (char *)0;

    if (gdpsp->gdps_polygons.num_polygons == gdpsp->gdps_target_polygon_i)
	gdpsp->gdps_curr_polygon_i = gdpsp->gdps_target_polygon_i;
    else
	gdpsp->gdps_curr_polygon_i = gdpsp->gdps_polygons.num_polygons;

    (void)to_data_polygons_func(interp, gedp, gdvp, ac, (const char **)av);
    bu_vls_free(&plist);

    return GED_OK;
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
