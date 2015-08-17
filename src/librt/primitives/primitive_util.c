/*               P R I M I T I V E _ U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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
#include "../librt_private.h"

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
	const struct rt_tess_tol *ttol,
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
    primitive_diagonal_mm = DIST_PT_PT(bbox_min, bbox_max);

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
approximate_parabolic_curve(struct rt_pt_node *pts, fastf_t p, int num_new_points)
{
    fastf_t error, max_error, seg_slope, seg_intercept;
    point_t v, point, p0, p1, new_point = VINIT_ZERO;
    struct rt_pt_node *node, *worst_node, *new_node;
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
	BU_ALLOC(new_node, struct rt_pt_node);
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
approximate_hyperbolic_curve(struct rt_pt_node *pts, fastf_t a, fastf_t b, int num_new_points)
{
    fastf_t error, max_error, seg_slope, seg_intercept;
    point_t v, point, p0, p1, new_point = VINIT_ZERO;
    struct rt_pt_node *node, *worst_node, *new_node;
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
	BU_ALLOC(new_node, struct rt_pt_node);
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
const int clt_semaphore = 12; /* FIXME: for testing; this isn't our semaphore */

static int clt_initialized = 0;
static cl_device_id clt_device;
static cl_context clt_context;
static cl_command_queue clt_queue;
static cl_program clt_program;
static cl_kernel clt_shot_kernel, clt_norm_kernel, clt_frame_kernel;

static size_t max_wg_size;
static cl_uint max_compute_units;

static cl_mem clt_rand_halftab;

static cl_mem clt_db_ids, clt_db_indexes, clt_db_prims, clt_db_bvh;
static cl_uint clt_db_nprims;



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

    fseek(fp, 0, SEEK_END);
    *length = ftell(fp);
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
    cl_int error;
    cl_program program;
    char **strings;
    const char **pc_strings;
    size_t *lengths;
    const size_t *pc_lengths;
    cl_uint i;

    strings = (char**)bu_calloc(count, sizeof(char*), "strings");
    pc_strings = (const char**)strings;
    lengths = (size_t*)bu_calloc(count, sizeof(size_t), "lengths");
    pc_lengths = (const size_t*)lengths;
    for (i=0; i<count; i++) {
        strings[i] = clt_read_code(filenames[i], &lengths[i]);
    }

    program = clCreateProgramWithSource(context, count, pc_strings, pc_lengths, &error);
    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL program");

    error = clBuildProgram(program, 1, &device, options, NULL, NULL);
    if (error != CL_SUCCESS) {
        size_t log_size;
        char *log_data;

        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        log_data = (char*)bu_malloc(log_size*sizeof(char), "failed to allocate memory for log");
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log_data, NULL);
        bu_log("BUILD LOG:\n%s\n", log_data);
        bu_bomb("failed to build OpenCL program");
    } else {
#if 0
	cl_uint n;
	size_t *sizes;
	char **buffers;

	clGetProgramInfo(program, CL_PROGRAM_NUM_DEVICES, sizeof(n), &n, NULL);

	sizes = (size_t*)bu_calloc(n, sizeof(size_t), "sizes");

	clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, sizeof(size_t)*n, sizes,NULL);

	buffers = (char**)bu_calloc(n, sizeof(char*), "buffers");
	for (i=0; i<n; ++i) {
	   buffers[i] = (char*)bu_calloc(sizes[i], sizeof(char), "buffers");
	}
	clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(char*)*n, buffers, NULL);

	for (i=0; i<n; ++i) {
	  FILE *fp;
	  char outfile[64];

	  snprintf(outfile, sizeof(outfile), "rt%u.ptx", i);
	  fp = fopen(outfile, "w");
	  if (!fp) bu_exit(-1, "failed to write OpenCL code file (%s)\n", outfile);

	  if (fwrite(buffers[i], sizes[i], 1, fp) != 1)
	      bu_bomb("failed to write OpenCL binaries");
	  fclose(fp);
	}
#endif
    }

    bu_free(lengths, "failed bu_free() in clt_get_program()");
    for (i=0; i<count; i++) {
        bu_free(strings[i], "failed bu_free() in clt_get_program()");
    }
    bu_free(strings, "failed bu_free() in clt_get_program()");
    return program;
}


static void
clt_cleanup(void)
{
    if (!clt_initialized) return;

    clReleaseMemObject(clt_rand_halftab);

    clReleaseKernel(clt_frame_kernel);

    clReleaseKernel(clt_norm_kernel);
    clReleaseKernel(clt_shot_kernel);

    clReleaseCommandQueue(clt_queue);
    clReleaseProgram(clt_program);
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

            "arb8_shot.cl",
            "ehy_shot.cl",
            "ell_shot.cl",
            "sph_shot.cl",
            "rec_shot.cl",
            "tgc_shot.cl",
            "tor_shot.cl",

            "rt.cl",
        };

        clt_initialized = 1;
        atexit(clt_cleanup);

        clt_device = clt_get_cl_device();

        clt_context = clCreateContext(NULL, 1, &clt_device, NULL, NULL, &error);
        if (error != CL_SUCCESS) bu_bomb("failed to create an OpenCL context");

        clt_queue = clCreateCommandQueue(clt_context, clt_device, 0, &error);
        if (error != CL_SUCCESS) bu_bomb("failed to create an OpenCL command queue");

        clt_program = clt_get_program(clt_context, clt_device, sizeof(main_files)/sizeof(*main_files), main_files, "-I.");

        clt_shot_kernel = clCreateKernel(clt_program, "solid_shot", &error);
        if (error != CL_SUCCESS) bu_bomb("failed to create an OpenCL kernel");
        clt_norm_kernel = clCreateKernel(clt_program, "solid_norm", &error);
        if (error != CL_SUCCESS) bu_bomb("failed to create an OpenCL kernel");

        clt_frame_kernel = clCreateKernel(clt_program, "do_pixel", &error);
        if (error != CL_SUCCESS) bu_bomb("failed to create an OpenCL kernel");


	clt_rand_halftab = clCreateBuffer(clt_context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR, sizeof(bn_rand_halftab), bn_rand_halftab, &error);
	if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL rand_haltab buffer");
    }
    bu_semaphore_release(clt_semaphore);
}


size_t
clt_solid_length(struct soltab *stp)
{
    switch (stp->st_id) {
        case ID_TOR:    return clt_tor_length(stp);
        case ID_TGC:    return clt_tgc_length(stp);
        case ID_ELL:    return clt_ell_length(stp);
        case ID_ARB8:   return clt_arb_length(stp);
        case ID_REC:    return clt_rec_length(stp);
        case ID_SPH:    return clt_sph_length(stp);
        case ID_EHY:    return clt_ehy_length(stp);
        default:        return 0;
    }
}

void
clt_solid_pack(void *dst, struct soltab *src)
{
    switch (src->st_id) {
        case ID_TOR:    clt_tor_pack(dst, src);     break;
        case ID_TGC:    clt_tgc_pack(dst, src);     break;
        case ID_ELL:    clt_ell_pack(dst, src);     break;
        case ID_ARB8:   clt_arb_pack(dst, src);     break;
        case ID_REC:    clt_rec_pack(dst, src);     break;
        case ID_SPH:    clt_sph_pack(dst, src);     break;
        case ID_EHY:    clt_ehy_pack(dst, src);     break;
        default:                                    break;
    }
}


cl_int
clt_shot(size_t sz_hits, struct cl_hit *hits, struct xray *rp, struct soltab *stp, struct application *ap, struct seg *seghead)
{
    const size_t sz_args = clt_solid_length(stp);
    char *args;

    const cl_int id = stp->st_id;

    cl_int i, len;
    cl_double3 r_pt, r_dir;

    const size_t hypersample = 1;
    cl_int error;
    cl_mem pin, plen, pout;

    VMOVE(r_pt.s, rp->r_pt);
    VMOVE(r_dir.s, rp->r_dir);

    args = (char*)bu_malloc(sz_args, "clt_shot");
    clt_solid_pack(args, stp);

    pin = clCreateBuffer(clt_context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR, sz_args, args, &error);
    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL input buffer");
    pout = clCreateBuffer(clt_context, CL_MEM_WRITE_ONLY, sz_hits, NULL, &error);
    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL output buffer");
    plen = clCreateBuffer(clt_context, CL_MEM_WRITE_ONLY, sizeof(len), NULL, &error);
    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL output buffer");

    bu_semaphore_acquire(clt_semaphore);
    error = clSetKernelArg(clt_shot_kernel, 0, sizeof(cl_mem), &plen);
    error |= clSetKernelArg(clt_shot_kernel, 1, sizeof(cl_mem), &pout);
    error |= clSetKernelArg(clt_shot_kernel, 2, sizeof(cl_double3), &r_pt);
    error |= clSetKernelArg(clt_shot_kernel, 3, sizeof(cl_double3), &r_dir);
    error |= clSetKernelArg(clt_shot_kernel, 4, sizeof(cl_int), &id);
    error |= clSetKernelArg(clt_shot_kernel, 5, sizeof(cl_mem), &pin);
    if (error != CL_SUCCESS) bu_bomb("failed to set OpenCL kernel arguments");
    error = clEnqueueNDRangeKernel(clt_queue, clt_shot_kernel, 1, NULL, &hypersample, NULL, 0, NULL, NULL);
    bu_semaphore_release(clt_semaphore);
    if (error != CL_SUCCESS) bu_bomb("failed to enqueue OpenCL kernel");

    if (clFinish(clt_queue) != CL_SUCCESS) bu_bomb("failure in clFinish()");
    clEnqueueReadBuffer(clt_queue, plen, CL_TRUE, 0, sizeof(len), &len, 0, NULL, NULL);
    clReleaseMemObject(plen);
    clEnqueueReadBuffer(clt_queue, pout, CL_TRUE, 0, sz_hits, hits, 0, NULL, NULL);
    clReleaseMemObject(pout);
    clReleaseMemObject(pin);

    bu_free(args, "clt_shot");

    for (i=0; i<len; i+=2) {
        struct seg *segp;
        RT_GET_SEG(segp, ap->a_resource);
        segp->seg_stp = stp;
        segp->seg_in.hit_dist = hits[i].hit_dist;
        segp->seg_in.hit_surfno = hits[i].hit_surfno;
        segp->seg_out.hit_dist = hits[i+1].hit_dist;
        segp->seg_out.hit_surfno = hits[i+1].hit_surfno;
        /* Set aside vector for rt_tor_norm() later */
        VMOVE(segp->seg_in.hit_vpriv, hits[i].hit_vpriv.s);
        VMOVE(segp->seg_out.hit_vpriv, hits[i+1].hit_vpriv.s);
        BU_LIST_INSERT(&(seghead->l), &(segp->l));
    }
    return len;
}


void
clt_norm(struct hit *hitp, struct soltab *stp, struct xray *rp)
{
    const size_t sz_args = clt_solid_length(stp);
    char *args;

    const cl_int id = stp->st_id;

    cl_double3 r_pt, r_dir;

    const size_t hypersample = 1;
    cl_int error;
    cl_mem pin, pout;
    struct cl_hit hit;

    VMOVE(r_pt.s, rp->r_pt);
    VMOVE(r_dir.s, rp->r_dir);

    VMOVE(hit.hit_vpriv.s, hitp->hit_vpriv);
    hit.hit_dist = hitp->hit_dist;
    hit.hit_surfno = hitp->hit_surfno;

    args = (char*)bu_malloc(sz_args, "clt_norm");
    clt_solid_pack(args, stp);

    pin = clCreateBuffer(clt_context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR, sz_args, args, &error);
    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL input buffer");
    pout = clCreateBuffer(clt_context, CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR, sizeof(hit), &hit, &error);
    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL output buffer");

    bu_semaphore_acquire(clt_semaphore);
    error = clSetKernelArg(clt_norm_kernel, 0, sizeof(cl_mem), &pout);
    error |= clSetKernelArg(clt_norm_kernel, 1, sizeof(cl_double3), &r_pt);
    error |= clSetKernelArg(clt_norm_kernel, 2, sizeof(cl_double3), &r_dir);
    error |= clSetKernelArg(clt_norm_kernel, 3, sizeof(cl_int), &id);
    error |= clSetKernelArg(clt_norm_kernel, 4, sizeof(cl_mem), &pin);
    if (error != CL_SUCCESS) bu_bomb("failed to set OpenCL kernel arguments");
    error = clEnqueueNDRangeKernel(clt_queue, clt_norm_kernel, 1, NULL, &hypersample, NULL, 0, NULL, NULL);
    bu_semaphore_release(clt_semaphore);
    if (error != CL_SUCCESS) bu_bomb("failed to enqueue OpenCL kernel");

    if (clFinish(clt_queue) != CL_SUCCESS) bu_bomb("failure in clFinish()");
    clEnqueueReadBuffer(clt_queue, pout, CL_TRUE, 0, sizeof(hit), &hit, 0, NULL, NULL);
    clReleaseMemObject(pout);
    clReleaseMemObject(pin);

    bu_free(args, "clt_norm");

    VMOVE(hitp->hit_normal, hit.hit_normal.s);
    VMOVE(hitp->hit_point, hit.hit_point.s);
    VMOVE(hitp->hit_vpriv, hit.hit_vpriv.s);
}


void
clt_db_store(size_t count, struct soltab *solids[])
{
    cl_int error;
    cl_int *ids;
    cl_uint *indexes;
    char *prims;
    size_t i;

    if (count != 0) {
	ids = (cl_int*)bu_calloc(count, sizeof(cl_int), "ids");
	for (i=0; i < count; i++) {
	    const struct soltab *stp = solids[i];
	    ids[i] = stp->st_id;
	}

	indexes = (cl_uint*)bu_calloc(count+1, sizeof(cl_uint), "indexes");
	indexes[0] = 0;
	for (i=1; i <= count; i++) {
	    size_t size;
	    size = clt_solid_length(solids[i-1]);
	    indexes[i] = indexes[i-1] + size;
	}

	if (indexes[count] != 0) {
	    prims = (char*)bu_malloc(indexes[count], "prims");
	    for (i=0; i<count; i++) {
		clt_solid_pack(prims+indexes[i], solids[i]);
	    }

	    clt_db_prims = clCreateBuffer(clt_context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR, indexes[count], prims, &error);
	    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL indexes buffer");

	    bu_free(prims, "failed bu_free() in clt_db_store()");
	}

	clt_db_ids = clCreateBuffer(clt_context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR, sizeof(cl_int)*count, ids, &error);
	if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL ids buffer");
	clt_db_indexes = clCreateBuffer(clt_context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR, sizeof(cl_uint)*(count+1), indexes, &error);
	if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL indexes buffer");

	bu_free(indexes, "failed bu_free() in clt_db_store()");
	bu_free(ids, "failed bu_free() in clt_db_store()");
    }

    clt_db_nprims = count;
}

void
clt_db_store_bvh(size_t count, struct clt_linear_bvh_node *nodes)
{
    cl_int error;

    clt_db_bvh = clCreateBuffer(clt_context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR, sizeof(struct clt_linear_bvh_node)*count, nodes, &error);
    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL bvh buffer");
}

void
clt_db_release(void)
{
    clReleaseMemObject(clt_db_bvh);
    clReleaseMemObject(clt_db_prims);
    clReleaseMemObject(clt_db_indexes);
    clReleaseMemObject(clt_db_ids);

    clt_db_nprims = 0;
}

void
clt_frame(void *pixels, uint8_t o[3], int cur_pixel, int last_pixel,
	  int width, int ibackground[3], int inonbackground[3],
          mat_t view2model, fastf_t cell_width, fastf_t cell_height,
          fastf_t aspect, int lightmodel)
{
    const size_t npix = last_pixel-cur_pixel+1;

    size_t sz_pixels;
    size_t sz_hits;

    struct {
        cl_double16 view2model;
        cl_double cell_width, cell_height, aspect;
	cl_uint randhalftabsize;
        cl_int cur_pixel, last_pixel, width;
        cl_int lightmodel;
        cl_uchar3 o;
        cl_uchar3 ibackground, inonbackground;
    }p;

    cl_mem ppixels, phits;
    cl_int error;

    sz_pixels = sizeof(cl_uchar)*o[2]*npix;
    sz_hits = sizeof(struct cl_hit)*npix;

    p.randhalftabsize = bn_randhalftabsize;
    p.cur_pixel = cur_pixel;
    p.last_pixel = last_pixel;
    p.width = width;
    VMOVE(p.o.s, o);
    VMOVE(p.ibackground.s, ibackground);
    VMOVE(p.inonbackground.s, inonbackground);
    MAT_COPY(p.view2model.s, view2model);
    p.cell_width = cell_width;
    p.cell_height = cell_height;
    p.aspect = aspect;
    p.lightmodel = lightmodel;

    ppixels = clCreateBuffer(clt_context, CL_MEM_WRITE_ONLY, sz_pixels, NULL, &error);
    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL output buffer");
    phits = clCreateBuffer(clt_context, CL_MEM_READ_WRITE, sz_hits, NULL, &error);
    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL hits buffer");

    bu_semaphore_acquire(clt_semaphore);
    error = clSetKernelArg(clt_frame_kernel, 0, sizeof(cl_mem), &ppixels);
    error |= clSetKernelArg(clt_frame_kernel, 1, sizeof(cl_uchar3), &p.o);
    error |= clSetKernelArg(clt_frame_kernel, 2, sizeof(cl_mem), &phits);
    error |= clSetKernelArg(clt_frame_kernel, 3, sizeof(cl_int), &p.cur_pixel);
    error |= clSetKernelArg(clt_frame_kernel, 4, sizeof(cl_int), &p.last_pixel);
    error |= clSetKernelArg(clt_frame_kernel, 5, sizeof(cl_int), &p.width);
    error |= clSetKernelArg(clt_frame_kernel, 6, sizeof(cl_mem), &clt_rand_halftab);
    error |= clSetKernelArg(clt_frame_kernel, 7, sizeof(cl_uint), &p.randhalftabsize);
    error |= clSetKernelArg(clt_frame_kernel, 8, sizeof(cl_uchar3), &p.ibackground);
    error |= clSetKernelArg(clt_frame_kernel, 9, sizeof(cl_uchar3), &p.inonbackground);
    error |= clSetKernelArg(clt_frame_kernel, 10, sizeof(cl_double16), &p.view2model);
    error |= clSetKernelArg(clt_frame_kernel, 11, sizeof(cl_double), &p.cell_width);
    error |= clSetKernelArg(clt_frame_kernel, 12, sizeof(cl_double), &p.cell_height);
    error |= clSetKernelArg(clt_frame_kernel, 13, sizeof(cl_double), &p.aspect);
    error |= clSetKernelArg(clt_frame_kernel, 14, sizeof(cl_int), &lightmodel);
    error |= clSetKernelArg(clt_frame_kernel, 15, sizeof(cl_uint), &clt_db_nprims);
    error |= clSetKernelArg(clt_frame_kernel, 16, sizeof(cl_mem), &clt_db_ids);
    error |= clSetKernelArg(clt_frame_kernel, 17, sizeof(cl_mem), &clt_db_bvh);
    error |= clSetKernelArg(clt_frame_kernel, 18, sizeof(cl_mem), &clt_db_indexes);
    error |= clSetKernelArg(clt_frame_kernel, 19, sizeof(cl_mem), &clt_db_prims);
    if (error != CL_SUCCESS) bu_bomb("failed to set OpenCL kernel arguments");
    error = clEnqueueNDRangeKernel(clt_queue, clt_frame_kernel, 1, NULL, &npix,
                                   NULL, 0, NULL, NULL);
    bu_semaphore_release(clt_semaphore);

    clEnqueueReadBuffer(clt_queue, ppixels, CL_TRUE, 0, sz_pixels, pixels, 0,
                        NULL, NULL);
    clReleaseMemObject(ppixels);
    clReleaseMemObject(phits);
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
