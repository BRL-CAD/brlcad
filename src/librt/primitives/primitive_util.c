/*               P R I M I T I V E _ U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 2012-2021 United States Government as represented by
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
/** @file primitive_util.c
 *
 * General utility routines for librt primitives that are not part of
 * the librt API. Prototypes for these routines are located in
 * librt_private.h.
 */

#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/app.h"
#include "../librt_private.h"


/**
 * Sort an array of hits into ascending order.
 */
void
primitive_hitsort(register struct hit *h, register int nh)
{
    register int i, j;
    struct hit temp;

    for (i = 0; i < nh-1; i++) {
	for (j = i + 1; j < nh; j++) {
	    if (h[i].hit_dist <= h[j].hit_dist)
		continue;
	    temp = h[j];                /* struct copy */
	    h[j] = h[i];                /* struct copy */
	    h[i] = temp;                /* struct copy */
	}
    }
}


/**
 * If only the absolute tolerance is valid (positive), it is returned.
 *
 * If only the relative tolerance is valid (in (0.0, 1.0)), the return value
 * is the relative tolerance multiplied by rel_to_abs.
 *
 * If both tolerances are valid, the smaller is used.
 * If neither is valid, a default of .1 * rel_to_abs is used.
 */
#define DEFAULT_REL_TOL .1
fastf_t
primitive_get_absolute_tolerance(
	const struct bg_tess_tol *ttol,
	fastf_t rel_to_abs)
{
    fastf_t tol, rel_tol, abs_tol;
    int rel_tol_is_valid;

    rel_tol = ttol->rel;
    abs_tol = ttol->abs;

    rel_tol_is_valid = 0;
    if (rel_tol > 0.0 && rel_tol < 1.0) {
	rel_tol_is_valid = 1;
    }

    if (abs_tol > 0.0 && (!rel_tol_is_valid || rel_tol > abs_tol)) {
	tol = abs_tol;
    } else {
	if (!rel_tol_is_valid) {
	    rel_tol = DEFAULT_REL_TOL;
	}
	tol = rel_tol * rel_to_abs;
    }

    return tol;
}

/**
 * Gives a rough estimate of the maximum number of times a primitive's bounding
 * box diagonal will be sampled based on the sample density of the view.
 *
 * Practically, it is an estimate of the maximum number of pixels that would be
 * used if the diagonal line were drawn in the current view window.
 *
 * It is currently used in adaptive plot routines to help choose how many
 * sample points should be used for plotted curves.
 */
fastf_t
primitive_diagonal_samples(
	struct rt_db_internal *ip,
	const struct rt_view_info *info)
{
    point_t bbox_min, bbox_max;
    fastf_t primitive_diagonal_mm, samples_per_mm;
    fastf_t diagonal_samples;

    ip->idb_meth->ft_bbox(ip, &bbox_min, &bbox_max, info->tol);
    primitive_diagonal_mm = DIST_PNT_PNT(bbox_min, bbox_max);

    samples_per_mm = 1.0 / info->point_spacing;
    diagonal_samples = samples_per_mm * primitive_diagonal_mm;

    return diagonal_samples;
}

/**
 * Estimate the number of evenly spaced cross-section curves needed to meet a
 * target curve spacing.
 */
fastf_t
primitive_curve_count(
	struct rt_db_internal *ip,
	const struct rt_view_info *info)
{
    point_t bbox_min, bbox_max;
    fastf_t x_len, y_len, z_len, avg_len;

    if (!ip->idb_meth->ft_bbox) {
	return 0.0;
    }

    ip->idb_meth->ft_bbox(ip, &bbox_min, &bbox_max, info->tol);

    x_len = fabs(bbox_max[X] - bbox_min[X]);
    y_len = fabs(bbox_max[Y] - bbox_min[Y]);
    z_len = fabs(bbox_max[Z] - bbox_min[Z]);

    avg_len = (x_len + y_len + z_len) / 3.0;

    return avg_len / info->curve_spacing;
}

/* Calculate the length of the shortest distance between a point and a line in
 * the y-z plane.
 */
static fastf_t
distance_point_to_line(point_t p, fastf_t slope, fastf_t intercept)
{
    /* input line:
     *   z = slope * y + intercept
     *
     * implicit form is:
     *   az + by + c = 0,  where a = -slope, b = 1, c = -intercept
     *
     * standard 2D point-line distance calculation:
     *   d = |aPy + bPz + c| / sqrt(a^2 + b^2)
     */
    return fabs(-slope * p[Y] + p[Z] - intercept) / sqrt(slope * slope + 1);
}

/* Assume the existence of a line and a parabola at the origin, both in the y-z
 * plane (x = 0.0). The parabola and line are described by arguments p and m,
 * from the respective equations (z = y^2 / 4p) and (z = my + b).
 *
 * The line is assumed to intersect the parabola at two points.
 *
 * The portion of the parabola between these two intersection points takes on a
 * single maximum value with respect to the intersecting line when its slope is
 * the same as the line's (i.e. when the tangent line and intersecting line are
 * parallel).
 *
 * The first derivate slope equation z' = y / 2p implies that the parabola has
 * slope m when y = 2pm. So we calculate y from p and m, and then z from y.
 */
static void
parabola_point_farthest_from_line(point_t max_point, fastf_t p, fastf_t m)
{
    fastf_t y = 2.0 * p * m;

    max_point[X] = 0.0;
    max_point[Y] = y;
    max_point[Z] = (y * y) / ( 4.0 * p);
}

/* Approximate part of the parabola (y - h)^2 = 4p(z - k) lying in the y-z
 * plane.
 *
 * pts->p: the vertex (0, h, k)
 * pts->next->p: another point on the parabola
 * pts->next->next: NULL
 * p: the constant from the above equation
 *
 * This routine inserts num_new_points points between the two input points to
 * better approximate the parabolic curve passing between them.
 *
 * Returns number of points successfully added.
 */
int
approximate_parabolic_curve(struct rt_pnt_node *pts, fastf_t p, int num_new_points)
{
    fastf_t error, max_error, seg_slope, seg_intercept;
    point_t v, point, p0, p1, new_point = VINIT_ZERO;
    struct rt_pnt_node *node, *worst_node, *new_node;
    int i;

    if (pts == NULL || pts->next == NULL || num_new_points < 1) {
	return 0;
    }

    VMOVE(v, pts->p);

    for (i = 0; i < num_new_points; ++i) {
	worst_node = node = pts;
	max_error = 0.0;

	/* Find the least accurate line segment, and calculate a new parabola
	 * point to insert between its endpoints.
	 */
	while (node->next != NULL) {
	    VMOVE(p0, node->p);
	    VMOVE(p1, node->next->p);

	    seg_slope = (p1[Z] - p0[Z]) / (p1[Y] - p0[Y]);
	    seg_intercept = p0[Z] - seg_slope * p0[Y];

	    /* get farthest point on the equivalent parabola at the origin */
	    parabola_point_farthest_from_line(point, p, seg_slope);

	    /* translate result to actual parabola position */
	    point[Y] += v[Y];
	    point[Z] += v[Z];

	    /* see if the maximum distance between the parabola and the line
	     * (the error) is larger than the largest recorded error
	     * */
	    error = distance_point_to_line(point, seg_slope, seg_intercept);

	    if (error > max_error) {
		max_error = error;
		worst_node = node;
		VMOVE(new_point, point);
	    }

	    node = node->next;
	}

	/* insert new point between endpoints of the least accurate segment */
	BU_ALLOC(new_node, struct rt_pnt_node);
	VMOVE(new_node->p, new_point);
	new_node->next = worst_node->next;
	worst_node->next = new_node;
    }

    return num_new_points;
}

/* Assume the existence of a line and a hyperbola with asymptote origin at the
 * origin, both in the y-z plane (x = 0.0). The hyperbola and line are
 * described by arguments a, b, and m, from the respective equations
 * (z = +-(a/b) * sqrt(y^2 + b^2)) and (z = my + b).
 *
 * The line is assumed to intersect the positive half of the hyperbola at two
 * points.
 *
 * The portion of the hyperbola between these two intersection points takes on
 * a single maximum value with respect to the intersecting line when its slope
 * is the same as the line's (i.e. when the tangent line and intersecting line
 * are parallel).
 *
 * The first derivate slope equation z' = ay / (b * sqrt(y^2 + b^2)) implies
 * that the hyperbola has slope m when y = mb^2 / sqrt(a^2 - b^2m^2).
 *
 * We calculate y from a, b, and m, and then z from y.
 */
static void
hyperbola_point_farthest_from_line(point_t max_point, fastf_t a, fastf_t b, fastf_t m)
{
    fastf_t y = (m * b * b) / sqrt((a * a) - (b * b * m * m));

    max_point[X] = 0.0;
    max_point[Y] = y;
    max_point[Z] = (a / b) * sqrt(y * y + b * b);
}

/* Approximate part of the hyperbola lying in the Y-Z plane described by:
 *  ((z - k)^2 / a^2) - ((y - h)^2 / b^2) = 1
 *
 * pts->p: the asymptote origin (0, h, k)
 * pts->next->p: another point on the hyperbola
 * pts->next->next: NULL
 * a, b: the constants from the above equation
 *
 * This routine inserts num_new_points points between the two input points to
 * better approximate the hyperbolic curve passing between them.
 *
 * Returns number of points successfully added.
 */
int
approximate_hyperbolic_curve(struct rt_pnt_node *pts, fastf_t a, fastf_t b, int num_new_points)
{
    fastf_t error, max_error, seg_slope, seg_intercept;
    point_t v, point, p0, p1, new_point = VINIT_ZERO;
    struct rt_pnt_node *node, *worst_node, *new_node;
    int i;

    if (pts == NULL || pts->next == NULL || num_new_points < 1) {
	return 0;
    }

    VMOVE(v, pts->p);

    for (i = 0; i < num_new_points; ++i) {
	worst_node = node = pts;
	max_error = 0.0;

	/* Find the least accurate line segment, and calculate a new hyperbola
	 * point to insert between its endpoints.
	 */
	while (node->next != NULL) {
	    VMOVE(p0, node->p);
	    VMOVE(p1, node->next->p);

	    seg_slope = (p1[Z] - p0[Z]) / (p1[Y] - p0[Y]);
	    seg_intercept = p0[Z] - seg_slope * p0[Y];

	    /* get farthest point on the equivalent hyperbola with asymptote origin at
	     * the origin
	     */
	    hyperbola_point_farthest_from_line(point, a, b, seg_slope);

	    /* translate result to actual hyperbola position */
	    point[Y] += v[Y];
	    point[Z] += v[Z] - a;

	    /* see if the maximum distance between the hyperbola and the line
	     * (the error) is larger than the largest recorded error
	     * */
	    error = distance_point_to_line(point, seg_slope, seg_intercept);

	    if (error > max_error) {
		max_error = error;
		worst_node = node;
		VMOVE(new_point, point);
	    }

	    node = node->next;
	}

	/* insert new point between endpoints of the least accurate segment */
	BU_ALLOC(new_node, struct rt_pnt_node);
	VMOVE(new_node->p, new_point);
	new_node->next = worst_node->next;
	worst_node->next = new_node;
    }

    return num_new_points;
}

void
ellipse_point_at_radian(
	point_t result,
	const vect_t center,
	const vect_t axis_a,
	const vect_t axis_b,
	fastf_t radian)
{
    fastf_t cos_rad, sin_rad;

    cos_rad = cos(radian);
    sin_rad = sin(radian);

    VJOIN2(result, center, cos_rad, axis_a, sin_rad, axis_b);
}

void
plot_ellipse(
	struct bu_list *vhead,
	const vect_t center,
	const vect_t axis_a,
	const vect_t axis_b,
	int num_points)
{
    int i;
    point_t p;
    fastf_t radian, radian_step;

    radian_step = M_2PI / num_points;

    ellipse_point_at_radian(p, center, axis_a, axis_b,
	    radian_step * (num_points - 1));
    RT_ADD_VLIST(vhead, p, BN_VLIST_LINE_MOVE);

    radian = 0;
    for (i = 0; i < num_points; ++i) {
	ellipse_point_at_radian(p, center, axis_a, axis_b, radian);
	RT_ADD_VLIST(vhead, p, BN_VLIST_LINE_DRAW);

	radian += radian_step;
    }
}

int
_rt_tcl_list_to_int_array(const char *list, int **array, int *array_len)
{
    int i, len;
    const char **argv;

    if (!list || !array || !array_len) return 0;

    /* split initial list */
    if (bu_argv_from_tcl_list(list, &len, (const char ***)&argv) != 0) {
	return 0;
    }

    if (len < 1) return 0;

    if (*array_len < 1) {
	*array = (int *)bu_calloc(len, sizeof(int), "array");
	*array_len = len;
    }

    for (i = 0; i < len && i < *array_len; i++) {
	(void)bu_opt_int(NULL, 1, &argv[i], &((*array)[i]));
    }

    bu_free((char *)argv, "argv");
    return len < *array_len ? len : *array_len;
}


int
_rt_tcl_list_to_fastf_array(const char *list, fastf_t **array, int *array_len)
{
    int i, len;
    const char **argv;

    if (!list || !array || !array_len) return 0;

    /* split initial list */
    if (bu_argv_from_tcl_list(list, &len, (const char ***)&argv) != 0) {
	return 0;
    }

    if (len < 1) return 0;

    if (*array_len < 1) {
	*array = (fastf_t *)bu_calloc(len, sizeof(fastf_t), "array");
	*array_len = len;
    }

    for (i = 0; i < len && i < *array_len; i++) {
	(void)bu_opt_fastf_t(NULL, 1, &argv[i], &((*array)[i]));
    }

    bu_free((char *)argv, "argv");
    return len < *array_len ? len : *array_len;
}


#ifdef USE_OPENCL

#ifndef BRLCAD_OPENCL_DIR
#  define BRLCAD_OPENCL_DIR "opencl"
#endif

const int clt_semaphore = 12; /* FIXME: for testing; this isn't our semaphore */

static int clt_initialized = 0;
static cl_device_id clt_device;
static cl_context clt_context;
static cl_command_queue clt_queue;
static cl_program clt_sh_program, clt_mh_program;
static cl_kernel clt_frame_kernel;
static cl_kernel clt_count_hits_kernel, clt_store_segs_kernel, clt_shade_segs_kernel;
static cl_kernel clt_boolweave_kernel, clt_boolfinal_kernel;

static size_t max_wg_size;
static cl_uint max_compute_units;

static cl_mem clt_rand_halftab;

static cl_mem clt_db_ids, clt_db_indexes, clt_db_prims, clt_db_bvh, clt_db_regions, clt_db_iregions;
static cl_mem clt_db_rtree, clt_db_bool_regions, clt_db_regions_table;
static cl_uint clt_db_nprims;
static cl_uint clt_db_nregions;



/**
 * For now, just get the first device from the first platform.
 */
cl_device_id
clt_get_cl_device(void)
{
    cl_int error;
    cl_platform_id platform;
    cl_device_id device;
    int using_cpu = 0;

    error = clGetPlatformIDs(1, &platform, NULL);
    if (error != CL_SUCCESS) bu_bomb("failed to find an OpenCL platform");

    error = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    if (error == CL_DEVICE_NOT_FOUND) {
        using_cpu = 1;
        error = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device, NULL);
    }
    if (error != CL_SUCCESS) bu_bomb("failed to find an OpenCL device (using this method)");

    if (using_cpu) bu_log("clt: no GPU devices available; using CPU for OpenCL\n");

    clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &max_wg_size, NULL);
    clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &max_compute_units, NULL);
    return device;
}


static char *
clt_read_code(const char *filename, size_t *length)
{
    FILE *fp;
    char *data;

    fp = fopen(filename , "r");
    if (!fp) bu_exit(-1, "failed to read OpenCL code file (%s)\n", filename);

    bu_fseek(fp, 0, SEEK_END);
    *length = bu_ftell(fp);
    rewind(fp);

    data = (char*)bu_malloc((*length+1)*sizeof(char), "failed bu_malloc() in clt_read_code()");

    if(fread(data, *length, 1, fp) != 1)
    bu_bomb("failed to read OpenCL code");

    fclose(fp);
    return data;
}

cl_program
clt_get_program(cl_context context, cl_device_id device, cl_uint count, const char *filenames[], const char *options)
{
    const char *pc_string;
    size_t pc_length;
    cl_program program;
    cl_program *programs;
    cl_int error;
    cl_uint i;
    char file[MAXPATHLEN] = {0};
    char path[MAXPATHLEN] = {0};

    programs = (cl_program*)bu_calloc(count, sizeof(cl_program), "programs");
    for (i=0; i<count; i++) {
	snprintf(path, MAXPATHLEN, "%s", bu_dir(NULL, 0, BU_DIR_DATA, BRLCAD_OPENCL_DIR, filenames[i], NULL));

        pc_string = clt_read_code(path, &pc_length);
        programs[i] = clCreateProgramWithSource(context, 1, &pc_string, &pc_length, &error);
        bu_free((char*)pc_string, "code");
        if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL program");
        error = clCompileProgram(programs[i], 1, &device, options, 0, NULL, NULL, NULL, NULL);
        if (error != CL_SUCCESS) {
            size_t log_size;
            char *log_data;

            clGetProgramBuildInfo(programs[i], device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
            log_data = (char*)bu_malloc(log_size*sizeof(char), "failed to allocate memory for log");
            clGetProgramBuildInfo(programs[i], device, CL_PROGRAM_BUILD_LOG, log_size, log_data, NULL);
            bu_log("BUILD LOG (%s):\n%s\n", path, log_data);
            bu_bomb("failed to compile OpenCL program");
        }
    }

    program = clLinkProgram(context, 1, &device, " ", count, programs, NULL, NULL, &error);
    if (error != CL_SUCCESS) bu_bomb("failed to link OpenCL program");

    for (i=0; i<count; i++) {
        clReleaseProgram(programs[i]);
    }

    bu_free(programs, "programs");
    return program;
}


static void
clt_cleanup(void)
{
    if (!clt_initialized) return;

    clReleaseMemObject(clt_rand_halftab);

    clReleaseKernel(clt_count_hits_kernel);
    clReleaseKernel(clt_store_segs_kernel);
    clReleaseKernel(clt_boolweave_kernel);
    clReleaseKernel(clt_boolfinal_kernel);
    clReleaseKernel(clt_shade_segs_kernel);

    clReleaseKernel(clt_frame_kernel);

    clReleaseCommandQueue(clt_queue);
    clReleaseProgram(clt_mh_program);
    clReleaseProgram(clt_sh_program);
    clReleaseContext(clt_context);

    clt_initialized = 0;
}

void
clt_init(void)
{
    cl_int error;

    bu_semaphore_acquire(clt_semaphore);
    if (!clt_initialized) {
        const char *main_files[] = {
            "solver.cl",
            "bool.cl",

            "arb8_shot.cl",
            "bot_shot.cl",
            "ehy_shot.cl",
            "ell_shot.cl",
            "epa_shot.cl",
            "eto_shot.cl",
            "sph_shot.cl",
            "ebm_shot.cl",
	    "part_shot.cl",
            "rec_shot.cl",
            "tgc_shot.cl",
            "tor_shot.cl",
            "rhc_shot.cl",
            "rpc_shot.cl",
            "hrt_shot.cl",

            "rt.cl",
        };
	char path[MAXPATHLEN] = {0};
	char args[MAXPATHLEN] = {0};

        clt_initialized = 1;
        atexit(clt_cleanup);

        clt_device = clt_get_cl_device();

        clt_context = clCreateContext(NULL, 1, &clt_device, NULL, NULL, &error);
        if (error != CL_SUCCESS) bu_bomb("failed to create an OpenCL context");

        clt_queue = clCreateCommandQueue(clt_context, clt_device, 0, &error);
        if (error != CL_SUCCESS) bu_bomb("failed to create an OpenCL command queue");

	/* locate opencl directory */
	snprintf(path, MAXPATHLEN, "%s", bu_dir(NULL, 0, BU_DIR_DATA, BRLCAD_OPENCL_DIR, NULL));

	/* compile opencl programs */
	snprintf(args, MAXPATHLEN, "-I %s -D RT_SINGLE_HIT=1", path);
        clt_sh_program = clt_get_program(clt_context, clt_device, sizeof(main_files)/sizeof(*main_files), main_files, args);
	snprintf(args, MAXPATHLEN, "-I %s -D RT_SINGLE_HIT=0", path);
        clt_mh_program = clt_get_program(clt_context, clt_device, sizeof(main_files)/sizeof(*main_files), main_files, args);

        clt_frame_kernel = clCreateKernel(clt_sh_program, "do_pixel", &error);
        if (error != CL_SUCCESS) bu_bomb("failed to create an OpenCL kernel");

        clt_count_hits_kernel = clCreateKernel(clt_mh_program, "count_hits", &error);
        if (error != CL_SUCCESS) bu_bomb("failed to create an OpenCL kernel");
        clt_store_segs_kernel = clCreateKernel(clt_mh_program, "store_segs", &error);
        if (error != CL_SUCCESS) bu_bomb("failed to create an OpenCL kernel");
	clt_boolweave_kernel = clCreateKernel(clt_mh_program, "rt_boolweave", &error);
	if (error != CL_SUCCESS) bu_bomb("failed to create an OpenCL kernel");
	clt_boolfinal_kernel = clCreateKernel(clt_mh_program, "rt_boolfinal", &error);
	if (error != CL_SUCCESS) bu_bomb("failed to create an OpenCL kernel");
        clt_shade_segs_kernel = clCreateKernel(clt_mh_program, "shade_segs", &error);
        if (error != CL_SUCCESS) bu_bomb("failed to create an OpenCL kernel");


	clt_rand_halftab = clCreateBuffer(clt_context, CL_MEM_READ_ONLY|CL_MEM_HOST_WRITE_ONLY|CL_MEM_COPY_HOST_PTR, sizeof(bn_rand_halftab), bn_rand_halftab, &error);
	if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL rand_haltab buffer");
    }
    bu_semaphore_release(clt_semaphore);
}


size_t
clt_solid_pack(struct bu_pool *pool, struct soltab *stp)
{
    size_t size;

    switch (stp->st_id) {
	case ID_TOR:		size = clt_tor_pack(pool, stp);	break;
	case ID_TGC:		size = clt_tgc_pack(pool, stp);	break;
	case ID_ELL:		size = clt_ell_pack(pool, stp);	break;
	case ID_ARB8:		size = clt_arb_pack(pool, stp);	break;
	case ID_REC:		size = clt_rec_pack(pool, stp);	break;
	case ID_SPH:		size = clt_sph_pack(pool, stp);	break;
	case ID_EBM:		size = clt_ebm_pack(pool, stp);	break;
	case ID_PARTICLE:	size = clt_part_pack(pool, stp);break;
	case ID_EHY:		size = clt_ehy_pack(pool, stp);	break;
	case ID_ARS:
	case ID_BOT:		size = clt_bot_pack(pool, stp);	break;
	case ID_EPA:		size = clt_epa_pack(pool, stp);	break;
	case ID_ETO:		size = clt_eto_pack(pool, stp); break;
	case ID_RHC:		size = clt_rhc_pack(pool, stp); break;
	case ID_RPC:		size = clt_rpc_pack(pool, stp); break;
	case ID_HRT:		size = clt_hrt_pack(pool, stp); break;
	default:		size = 0;			break;
    }
    return size;
}


void
clt_db_store(size_t count, struct soltab *solids[])
{
    cl_int error;

    if (count != 0) {
        cl_uchar *ids;
	cl_int *iregions;
        cl_uint *indexes;
        struct bu_pool *pool;
        size_t i;

	ids = (cl_uchar*)bu_calloc(count, sizeof(*ids), "ids");
	iregions = (cl_int*)bu_calloc(count, sizeof(*iregions), "iregions");
	for (i=0; i < count; i++) {
	    const struct soltab *stp = solids[i];
	    const struct region *regp = (struct region *)BU_PTBL_GET(&stp->st_regions,0);

            ids[i] = stp->st_id;
	    RT_CK_REGION(regp);
	    iregions[i] = regp->reg_bit;
	}

	indexes = (cl_uint*)bu_calloc(count+1, sizeof(*indexes), "indexes");
	indexes[0] = 0;

	pool = bu_pool_create(1024 * 1024);
	for (i=1; i <= count; i++) {
	    size_t size;
            /*bu_log("#%d:\t %s:", i, OBJ[ids[i-1]].ft_name);*/
	    size = clt_solid_pack(pool, solids[i-1]);
            /*bu_log("\t(%ld bytes)\n",size);*/
	    indexes[i] = indexes[i-1] + size;
	}
        bu_log("OCLDB:\t%ld primitives\n\t%.2f KB indexes, %.2f KB ids, %.2f KB prims\n", count,
		(sizeof(*indexes)*(count+1))/1024.0, (sizeof(*ids)*count)/1024.0, indexes[count]/1024.0);

	if (indexes[count] != 0) {
	    clt_db_prims = clCreateBuffer(clt_context, CL_MEM_READ_ONLY|CL_MEM_HOST_WRITE_ONLY|CL_MEM_COPY_HOST_PTR, indexes[count], pool->block, &error);
	    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL indexes buffer");
	}
        bu_pool_delete(pool);

	clt_db_ids = clCreateBuffer(clt_context, CL_MEM_READ_ONLY|CL_MEM_HOST_WRITE_ONLY|CL_MEM_COPY_HOST_PTR, sizeof(*ids)*count, ids, &error);
	if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL ids buffer");
	clt_db_iregions = clCreateBuffer(clt_context, CL_MEM_READ_ONLY|CL_MEM_HOST_WRITE_ONLY|CL_MEM_COPY_HOST_PTR, sizeof(*iregions)*count, iregions, &error);
	if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL iregions buffer");
	clt_db_indexes = clCreateBuffer(clt_context, CL_MEM_READ_ONLY|CL_MEM_HOST_WRITE_ONLY|CL_MEM_COPY_HOST_PTR, sizeof(*indexes)*(count+1), indexes, &error);
	if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL indexes buffer");

	bu_free(indexes, "indexes");
	bu_free(iregions, "iregions");
	bu_free(ids, "ids");
    }

    clt_db_nprims = count;
}

void
clt_db_store_bvh(size_t count, struct clt_linear_bvh_node *nodes)
{
    cl_int error;

    bu_log("OCLBVH:\t%ld nodes\n\t%.2f KB total\n", count, (sizeof(struct clt_linear_bvh_node)*count)/1024.0);
    clt_db_bvh = clCreateBuffer(clt_context, CL_MEM_READ_ONLY|CL_MEM_HOST_WRITE_ONLY|CL_MEM_COPY_HOST_PTR, sizeof(struct clt_linear_bvh_node)*count, nodes, &error);
    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL bvh buffer");
}

void
clt_db_store_regions(size_t sz_btree_array, struct bit_tree *btp, size_t nregions, struct cl_bool_region *regions, struct cl_region *mtls)
{
    cl_int error;

    if (nregions != 0) {
	bu_log("OCLRegions:\t%ld regions\n\t%.2f KB regions, %.2f KB mtls\n", nregions, (sizeof(*regions)*nregions)/1024.0, (sizeof(*mtls)*nregions)/1024.0);
	clt_db_bool_regions = clCreateBuffer(clt_context, CL_MEM_READ_ONLY|CL_MEM_HOST_WRITE_ONLY|CL_MEM_COPY_HOST_PTR, sizeof(*regions)*nregions, regions, &error);
	if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL boolean regions buffer");

	clt_db_rtree = clCreateBuffer(clt_context, CL_MEM_READ_ONLY|CL_MEM_HOST_WRITE_ONLY|CL_MEM_COPY_HOST_PTR, sizeof(*btp)*sz_btree_array, btp, &error);
	if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL boolean trees buffer");

	clt_db_regions = clCreateBuffer(clt_context, CL_MEM_READ_ONLY|CL_MEM_HOST_WRITE_ONLY|CL_MEM_COPY_HOST_PTR, sizeof(*mtls)*nregions, mtls, &error);
	if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL regions buffer");
    }

    clt_db_nregions = nregions;
}

void
clt_db_store_regions_table(cl_uint *regions_table, size_t regions_table_size)
{
    cl_int error;

    if (regions_table_size != 0) {
	clt_db_regions_table = clCreateBuffer(clt_context, CL_MEM_READ_ONLY|CL_MEM_HOST_WRITE_ONLY|CL_MEM_COPY_HOST_PTR, sizeof(cl_uint)*regions_table_size, regions_table, &error);
	if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL regions_table buffer");
    }
}

void
clt_db_release(void)
{
    clReleaseMemObject(clt_db_iregions);
    clReleaseMemObject(clt_db_regions);
    clReleaseMemObject(clt_db_bvh);
    clReleaseMemObject(clt_db_prims);
    clReleaseMemObject(clt_db_indexes);
    clReleaseMemObject(clt_db_ids);
    clReleaseMemObject(clt_db_rtree);
    clReleaseMemObject(clt_db_bool_regions);
    clReleaseMemObject(clt_db_regions_table);

    clt_db_nprims = 0;
    clt_db_nregions = 0;
}

void
clt_frame(void *pixels, uint8_t o[2], int cur_pixel, int last_pixel,
	  int width, int ibackground[3], int inonbackground[3],
	  double airdensity, double haze[3], fastf_t gamma,
          mat_t view2model, fastf_t cell_width, fastf_t cell_height,
          fastf_t aspect, int lightmodel, int a_no_booleans)
{
    const size_t npix = last_pixel-cur_pixel+1;

    size_t wxh[2];
    size_t swxh[2];
    size_t i;

    struct {
        cl_double16 view2model;
	cl_double3 haze;
        cl_double cell_width, cell_height, aspect;
	cl_double airdensity, gamma;
	cl_uint randhalftabsize;
        cl_int cur_pixel, last_pixel, width;
        cl_int lightmodel;
        cl_uchar2 o;
        cl_uchar3 ibackground, inonbackground;
    }p;

    size_t sz_pixels;
    cl_mem ppixels;
    cl_int error;

    p.randhalftabsize = bn_randhalftabsize;
    p.cur_pixel = cur_pixel;
    p.last_pixel = last_pixel;
    p.width = width;
    V2MOVE(p.o.s, o);
    VMOVE(p.ibackground.s, ibackground);
    VMOVE(p.inonbackground.s, inonbackground);
    p.airdensity = airdensity;
    VMOVE(p.haze.s, haze);
    p.gamma = gamma;
    MAT_COPY(p.view2model.s, view2model);
    p.cell_width = cell_width;
    p.cell_height = cell_height;
    p.aspect = aspect;
    p.lightmodel = lightmodel;

    sz_pixels = sizeof(cl_uchar)*o[1]*npix;
    ppixels = clCreateBuffer(clt_context, CL_MEM_WRITE_ONLY|CL_MEM_HOST_READ_ONLY, sz_pixels, NULL, &error);
    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL pixels buffer");

    wxh[0] = (size_t)width;
    wxh[1] = (size_t)npix/width;

    swxh[0] = swxh[1] = 1;
    for (i=wxh[0]; (i % 2) == 0 && swxh[0] < 8; i/=2)
	swxh[0] *= 2;
    for (i=wxh[1]; (i % 2) == 0 && swxh[1] < 8; i/=2)
	swxh[1] *= 2;

    bu_log("%ldx%ld grid, %ldx%ld subgrids\n", wxh[0], wxh[1], swxh[0], swxh[1]);

    if (a_no_booleans) {
	bu_semaphore_acquire(clt_semaphore);
	error = clSetKernelArg(clt_frame_kernel, 0, sizeof(cl_mem), &ppixels);
	error |= clSetKernelArg(clt_frame_kernel, 1, sizeof(cl_uchar2), &p.o);
	error |= clSetKernelArg(clt_frame_kernel, 2, sizeof(cl_int), &p.cur_pixel);
	error |= clSetKernelArg(clt_frame_kernel, 3, sizeof(cl_int), &p.last_pixel);
	error |= clSetKernelArg(clt_frame_kernel, 4, sizeof(cl_int), &p.width);
	error |= clSetKernelArg(clt_frame_kernel, 5, sizeof(cl_mem), &clt_rand_halftab);
	error |= clSetKernelArg(clt_frame_kernel, 6, sizeof(cl_uint), &p.randhalftabsize);
	error |= clSetKernelArg(clt_frame_kernel, 7, sizeof(cl_uchar3), &p.ibackground);
	error |= clSetKernelArg(clt_frame_kernel, 8, sizeof(cl_uchar3), &p.inonbackground);
	error |= clSetKernelArg(clt_frame_kernel, 9, sizeof(cl_double), &p.airdensity);
	error |= clSetKernelArg(clt_frame_kernel, 10, sizeof(cl_double3), &p.haze);
	error |= clSetKernelArg(clt_frame_kernel, 11, sizeof(cl_double), &p.gamma);
	error |= clSetKernelArg(clt_frame_kernel, 12, sizeof(cl_double16), &p.view2model);
	error |= clSetKernelArg(clt_frame_kernel, 13, sizeof(cl_double), &p.cell_width);
	error |= clSetKernelArg(clt_frame_kernel, 14, sizeof(cl_double), &p.cell_height);
	error |= clSetKernelArg(clt_frame_kernel, 15, sizeof(cl_double), &p.aspect);
	error |= clSetKernelArg(clt_frame_kernel, 16, sizeof(cl_int), &lightmodel);
	error |= clSetKernelArg(clt_frame_kernel, 17, sizeof(cl_uint), &clt_db_nprims);
	error |= clSetKernelArg(clt_frame_kernel, 18, sizeof(cl_mem), &clt_db_ids);
	error |= clSetKernelArg(clt_frame_kernel, 19, sizeof(cl_mem), &clt_db_bvh);
	error |= clSetKernelArg(clt_frame_kernel, 20, sizeof(cl_mem), &clt_db_indexes);
	error |= clSetKernelArg(clt_frame_kernel, 21, sizeof(cl_mem), &clt_db_prims);
	error |= clSetKernelArg(clt_frame_kernel, 22, sizeof(cl_mem), &clt_db_regions);
	error |= clSetKernelArg(clt_frame_kernel, 23, sizeof(cl_mem), &clt_db_iregions);
	if (error != CL_SUCCESS) bu_bomb("failed to set OpenCL kernel arguments");
	error = clEnqueueNDRangeKernel(clt_queue, clt_frame_kernel, 2, NULL, wxh,
		swxh, 0, NULL, NULL);
	bu_semaphore_release(clt_semaphore);
    } else {
	size_t sz_counts;
	cl_int *counts;
	cl_mem pcounts;
	size_t sz_h;
	cl_uint *h;
	cl_mem ph;
	size_t sz_segs;
	cl_mem psegs;
	size_t sz_ipartitions;
	cl_mem head_partition;
	size_t sz_partitions;
	cl_mem ppartitions;
	cl_int max_depth;
	size_t sz_bv;
	cl_mem segs_bv;
	size_t sz_regiontable;
	cl_mem regiontable_bv;
	size_t snpix = swxh[0]*swxh[1];
	const cl_uint ZERO = 0u;

	sz_counts = sizeof(cl_int)*npix;
	pcounts = clCreateBuffer(clt_context, CL_MEM_WRITE_ONLY|CL_MEM_HOST_READ_ONLY, sz_counts, NULL, &error);
	if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL counts buffer");

	bu_semaphore_acquire(clt_semaphore);
	error = clSetKernelArg(clt_count_hits_kernel, 0, sizeof(cl_mem), &pcounts);
	error |= clSetKernelArg(clt_count_hits_kernel, 1, sizeof(cl_int), &p.cur_pixel);
	error |= clSetKernelArg(clt_count_hits_kernel, 2, sizeof(cl_int), &p.last_pixel);
	error |= clSetKernelArg(clt_count_hits_kernel, 3, sizeof(cl_int), &p.width);
	error |= clSetKernelArg(clt_count_hits_kernel, 4, sizeof(cl_double16), &p.view2model);
	error |= clSetKernelArg(clt_count_hits_kernel, 5, sizeof(cl_double), &p.cell_width);
	error |= clSetKernelArg(clt_count_hits_kernel, 6, sizeof(cl_double), &p.cell_height);
	error |= clSetKernelArg(clt_count_hits_kernel, 7, sizeof(cl_double), &p.aspect);
	error |= clSetKernelArg(clt_count_hits_kernel, 8, sizeof(cl_uint), &clt_db_nprims);
	error |= clSetKernelArg(clt_count_hits_kernel, 9, sizeof(cl_mem), &clt_db_ids);
	error |= clSetKernelArg(clt_count_hits_kernel, 10, sizeof(cl_mem), &clt_db_bvh);
	error |= clSetKernelArg(clt_count_hits_kernel, 11, sizeof(cl_mem), &clt_db_indexes);
	error |= clSetKernelArg(clt_count_hits_kernel, 12, sizeof(cl_mem), &clt_db_prims);
	if (error != CL_SUCCESS) bu_bomb("failed to set OpenCL kernel arguments");
	error = clEnqueueNDRangeKernel(clt_queue, clt_count_hits_kernel, 2, NULL, wxh,
		swxh, 0, NULL, NULL);
	bu_semaphore_release(clt_semaphore);

	/* once we can do the scan on the device we won't need these transfers */
	counts = (cl_int*)bu_calloc(1, sz_counts, "counts");
	clEnqueueReadBuffer(clt_queue, pcounts, CL_TRUE, 0, sz_counts, counts, 0, NULL, NULL);
	clReleaseMemObject(pcounts);

	sz_h = sizeof(cl_uint)*(npix+1);
	h = (cl_uint*)bu_calloc(1, sz_h, "h");
	h[0] = 0;
	max_depth = 0;
	for (i=1; i<=npix; i++) {
	    cl_int nsegs;

	    BU_ASSERT((counts[i-1] % 2) == 0);

	    /* number of segs is half the number of hits */
	    nsegs = counts[i-1]/2;
	    h[i] = h[i-1] + nsegs;
	    if (nsegs > max_depth)
		max_depth = nsegs;
	}
	bu_free(counts, "counts");

	ph = clCreateBuffer(clt_context, CL_MEM_READ_ONLY|CL_MEM_HOST_WRITE_ONLY|CL_MEM_COPY_HOST_PTR, sz_h, h, &error);
	if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL offs buffer");

	sz_segs = sizeof(struct cl_seg)*h[npix];

	sz_ipartitions = sizeof(cl_uint)*npix; /* store index to first partition of the ray */
	sz_partitions = sizeof(struct cl_partition)*h[npix]*2; /*create partition buffer with size= 2*number of segments */
	sz_bv = sizeof(cl_uint)*(h[npix]*2)*(max_depth/32 + 1); /* bitarray to represent the segs in each partition */
	sz_regiontable = sizeof(cl_uint)*npix*(clt_db_nregions/32 +1); /* bitarray to represent the regions involved in each partition */

	bu_free(h, "h");

	if (sz_segs != 0) {
	    psegs = clCreateBuffer(clt_context, CL_MEM_READ_WRITE|CL_MEM_HOST_NO_ACCESS, sz_segs, NULL, &error);
	    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL segs buffer");

	    bu_semaphore_acquire(clt_semaphore);
	    error = clSetKernelArg(clt_store_segs_kernel, 0, sizeof(cl_mem), &psegs);
	    error |= clSetKernelArg(clt_store_segs_kernel, 1, sizeof(cl_mem), &ph);
	    error |= clSetKernelArg(clt_store_segs_kernel, 2, sizeof(cl_int), &p.cur_pixel);
	    error |= clSetKernelArg(clt_store_segs_kernel, 3, sizeof(cl_int), &p.last_pixel);
	    error |= clSetKernelArg(clt_store_segs_kernel, 4, sizeof(cl_int), &p.width);
	    error |= clSetKernelArg(clt_store_segs_kernel, 5, sizeof(cl_double16), &p.view2model);
	    error |= clSetKernelArg(clt_store_segs_kernel, 6, sizeof(cl_double), &p.cell_width);
	    error |= clSetKernelArg(clt_store_segs_kernel, 7, sizeof(cl_double), &p.cell_height);
	    error |= clSetKernelArg(clt_store_segs_kernel, 8, sizeof(cl_double), &p.aspect);
	    error |= clSetKernelArg(clt_store_segs_kernel, 9, sizeof(cl_uint), &clt_db_nprims);
	    error |= clSetKernelArg(clt_store_segs_kernel, 10, sizeof(cl_mem), &clt_db_ids);
	    error |= clSetKernelArg(clt_store_segs_kernel, 11, sizeof(cl_mem), &clt_db_bvh);
	    error |= clSetKernelArg(clt_store_segs_kernel, 12, sizeof(cl_mem), &clt_db_indexes);
	    error |= clSetKernelArg(clt_store_segs_kernel, 13, sizeof(cl_mem), &clt_db_prims);
	    if (error != CL_SUCCESS) bu_bomb("failed to set OpenCL kernel arguments");
	    error = clEnqueueNDRangeKernel(clt_queue, clt_store_segs_kernel, 2, NULL, wxh,
		    swxh, 0, NULL, NULL);
	    bu_semaphore_release(clt_semaphore);
	} else {
	    psegs = NULL;
	}

	if (sz_ipartitions != 0) {
	    head_partition = clCreateBuffer(clt_context, CL_MEM_READ_WRITE|CL_MEM_HOST_NO_ACCESS, sz_ipartitions, NULL, &error);
	    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL head partitions buffer");

	    error = clEnqueueFillBuffer(clt_queue, head_partition, &ZERO, sizeof(ZERO), 0, sz_ipartitions, 0, NULL, NULL);
	    if (error != CL_SUCCESS) bu_bomb("failed to bzero OpenCL head partitions buffer");
	} else {
	    head_partition = NULL;
	}

	if (sz_bv != 0) {
	    segs_bv = clCreateBuffer(clt_context, CL_MEM_READ_WRITE|CL_MEM_HOST_NO_ACCESS, sz_bv, NULL, &error);
	    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL segs bitvector buffer");

	    error = clEnqueueFillBuffer(clt_queue, segs_bv, &ZERO, sizeof(ZERO), 0, sz_bv, 0, NULL, NULL);
	    if (error != CL_SUCCESS) bu_bomb("failed to bzero OpenCL segs bitvector buffer");
	} else {
	    segs_bv = NULL;
	}

	if (sz_regiontable != 0) {
	    regiontable_bv = clCreateBuffer(clt_context, CL_MEM_READ_WRITE|CL_MEM_HOST_NO_ACCESS, sz_regiontable, NULL, &error);
	    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL segs bitvector buffer");

	    error = clEnqueueFillBuffer(clt_queue, regiontable_bv, &ZERO, sizeof(ZERO), 0, sz_regiontable, 0, NULL, NULL);
	    if (error != CL_SUCCESS) bu_bomb("failed to bzero OpenCL segs bitvector buffer");
	} else {
	    regiontable_bv = NULL;
	}

	if (sz_partitions != 0) {
	    ppartitions = clCreateBuffer(clt_context, CL_MEM_READ_WRITE|CL_MEM_HOST_NO_ACCESS, sz_partitions, NULL, &error);
	    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL partitions buffer");

	    bu_semaphore_acquire(clt_semaphore);
	    error = clSetKernelArg(clt_boolweave_kernel, 0, sizeof(cl_mem), &ppartitions);
	    error |= clSetKernelArg(clt_boolweave_kernel, 1, sizeof(cl_mem), &head_partition);
	    error |= clSetKernelArg(clt_boolweave_kernel, 2, sizeof(cl_mem), &psegs);
	    error |= clSetKernelArg(clt_boolweave_kernel, 3, sizeof(cl_mem), &ph);
	    error |= clSetKernelArg(clt_boolweave_kernel, 4, sizeof(cl_mem), &segs_bv);
	    error |= clSetKernelArg(clt_boolweave_kernel, 5, sizeof(cl_int), &p.cur_pixel);
	    error |= clSetKernelArg(clt_boolweave_kernel, 6, sizeof(cl_int), &p.last_pixel);
	    error |= clSetKernelArg(clt_boolweave_kernel, 7, sizeof(cl_int), &max_depth);
	    if (error != CL_SUCCESS) bu_bomb("failed to set OpenCL kernel arguments");
	    error = clEnqueueNDRangeKernel(clt_queue, clt_boolweave_kernel, 1, NULL, &npix,
		    &snpix, 0, NULL, NULL);
	    bu_semaphore_release(clt_semaphore);

	    bu_semaphore_acquire(clt_semaphore);
	    error = clSetKernelArg(clt_boolfinal_kernel, 0, sizeof(cl_mem), &ppartitions);
	    error |= clSetKernelArg(clt_boolfinal_kernel, 1, sizeof(cl_mem), &head_partition);
	    error |= clSetKernelArg(clt_boolfinal_kernel, 2, sizeof(cl_mem), &psegs);
	    error |= clSetKernelArg(clt_boolfinal_kernel, 3, sizeof(cl_mem), &ph);
	    error |= clSetKernelArg(clt_boolfinal_kernel, 4, sizeof(cl_mem), &segs_bv);
	    error |= clSetKernelArg(clt_boolfinal_kernel, 5, sizeof(cl_int), &max_depth);
	    error |= clSetKernelArg(clt_boolfinal_kernel, 6, sizeof(cl_mem), &clt_db_bool_regions);
	    error |= clSetKernelArg(clt_boolfinal_kernel, 7, sizeof(cl_uint), &clt_db_nregions);
	    error |= clSetKernelArg(clt_boolfinal_kernel, 8, sizeof(cl_mem), &clt_db_rtree);
	    error |= clSetKernelArg(clt_boolfinal_kernel, 9, sizeof(cl_mem), &regiontable_bv);
	    error |= clSetKernelArg(clt_boolfinal_kernel, 10, sizeof(cl_int), &p.cur_pixel);
	    error |= clSetKernelArg(clt_boolfinal_kernel, 11, sizeof(cl_int), &p.last_pixel);
	    error |= clSetKernelArg(clt_boolfinal_kernel, 12, sizeof(cl_mem), &clt_db_regions_table);
	    if (error != CL_SUCCESS) bu_bomb("failed to set OpenCL kernel arguments");
	    error = clEnqueueNDRangeKernel(clt_queue, clt_boolfinal_kernel, 1, NULL, &npix,
		    &snpix, 0, NULL, NULL);
	    bu_semaphore_release(clt_semaphore);
	} else {
	    ppartitions = NULL;
	    lightmodel = -1;
	}

	bu_semaphore_acquire(clt_semaphore);
	error = clSetKernelArg(clt_shade_segs_kernel, 0, sizeof(cl_mem), &ppixels);
	error |= clSetKernelArg(clt_shade_segs_kernel, 1, sizeof(cl_uchar2), &p.o);
	error |= clSetKernelArg(clt_shade_segs_kernel, 2, sizeof(cl_mem), &psegs);
        error |= clSetKernelArg(clt_shade_segs_kernel, 3, sizeof(cl_mem), &head_partition);
	error |= clSetKernelArg(clt_shade_segs_kernel, 4, sizeof(cl_int), &p.cur_pixel);
	error |= clSetKernelArg(clt_shade_segs_kernel, 5, sizeof(cl_int), &p.last_pixel);
	error |= clSetKernelArg(clt_shade_segs_kernel, 6, sizeof(cl_int), &p.width);
	error |= clSetKernelArg(clt_shade_segs_kernel, 7, sizeof(cl_mem), &clt_rand_halftab);
	error |= clSetKernelArg(clt_shade_segs_kernel, 8, sizeof(cl_uint), &p.randhalftabsize);
	error |= clSetKernelArg(clt_shade_segs_kernel, 9, sizeof(cl_uchar3), &p.ibackground);
	error |= clSetKernelArg(clt_shade_segs_kernel, 10, sizeof(cl_uchar3), &p.inonbackground);
	error |= clSetKernelArg(clt_shade_segs_kernel, 11, sizeof(cl_double), &p.airdensity);
	error |= clSetKernelArg(clt_shade_segs_kernel, 12, sizeof(cl_double3), &p.haze);
	error |= clSetKernelArg(clt_shade_segs_kernel, 13, sizeof(cl_double), &p.gamma);
	error |= clSetKernelArg(clt_shade_segs_kernel, 14, sizeof(cl_double16), &p.view2model);
	error |= clSetKernelArg(clt_shade_segs_kernel, 15, sizeof(cl_double), &p.cell_width);
	error |= clSetKernelArg(clt_shade_segs_kernel, 16, sizeof(cl_double), &p.cell_height);
	error |= clSetKernelArg(clt_shade_segs_kernel, 17, sizeof(cl_double), &p.aspect);
	error |= clSetKernelArg(clt_shade_segs_kernel, 18, sizeof(cl_int), &lightmodel);
	error |= clSetKernelArg(clt_shade_segs_kernel, 19, sizeof(cl_mem), &clt_db_ids);
	error |= clSetKernelArg(clt_shade_segs_kernel, 20, sizeof(cl_mem), &clt_db_indexes);
	error |= clSetKernelArg(clt_shade_segs_kernel, 21, sizeof(cl_mem), &clt_db_prims);
	error |= clSetKernelArg(clt_shade_segs_kernel, 22, sizeof(cl_mem), &clt_db_regions);
	error |= clSetKernelArg(clt_shade_segs_kernel, 23, sizeof(cl_mem), &ppartitions);

	if (error != CL_SUCCESS) bu_bomb("failed to set OpenCL kernel arguments");
	error = clEnqueueNDRangeKernel(clt_queue, clt_shade_segs_kernel, 1, NULL, &npix,
		&snpix, 0, NULL, NULL);
	bu_semaphore_release(clt_semaphore);

	clReleaseMemObject(ph);
	clReleaseMemObject(psegs);
	clReleaseMemObject(ppartitions);
	clReleaseMemObject(segs_bv);
	clReleaseMemObject(regiontable_bv);
	clReleaseMemObject(head_partition);
    }
    clEnqueueReadBuffer(clt_queue, ppixels, CL_TRUE, 0, sz_pixels, pixels, 0, NULL, NULL);
    clReleaseMemObject(ppixels);
}
#endif


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
