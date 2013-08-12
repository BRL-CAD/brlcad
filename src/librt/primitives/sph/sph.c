/*                           S P H . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2013 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file primitives/sph/sph.c
 *
 * Intersect a ray with a Sphere.
 * Special case of the Generalized Ellipsoid
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <OpenCL/cl.h>
#include "bio.h"

#include "vmath.h"
#include "rtgeom.h"
#include "raytrace.h"


/*
 * Algorithm:
 *
 * Given V, A, where |A| = Radius, there is a set of points on this sphere
 *
 * { (x, y, z) | (x, y, z) is on sphere defined by V, A }
 *
 * To find the intersection of a line with the sphere, consider
 * the parametric line L:
 *
 * L : { P(n) | P + t(n) . D }
 *
 * Call W the actual point of intersection between L and the sphere.
 *
 * NORMALS.  Given the point W on the sphere, what is the vector
 * normal to the tangent plane at that point?
 *
 * N = (W - V) / |A|
 */

struct sph_specific {
    vect_t sph_V;	/* Vector to center of sphere */
    fastf_t sph_radsq;	/* Radius squared */
    fastf_t sph_invrad;	/* Inverse radius (for normal) */
    fastf_t sph_rad;	/* Radius */
    mat_t sph_SoR;	/* Rotate and scale for UV mapping */
};


#define OPENCL


#ifdef OPENCL
const int clt_semaphore = 12; /* FIXME: for testing; this isn't our semaphore */
static int clt_initialized = 0;
static cl_device_id clt_device;
static cl_context clt_context;
static cl_command_queue clt_queue;
static cl_program clt_program;
static cl_kernel clt_kernel;


static void
clt_cleanup()
{
    if (!clt_initialized) return;

    clReleaseKernel(clt_kernel);
    clReleaseCommandQueue(clt_queue);
    clReleaseProgram(clt_program);
    clReleaseContext(clt_context);

    clt_initialized = 0;
}


const char * const clt_program_code = "\
__kernel void sph_shot(__global __write_only double3 *output, double3 o, double3 l, double3 c, double r)\n\
{\n\
    double A = dot(l, l);\n\
    double B = 2*dot(l, o-c);\n\
    double C = dot(o-c, o-c) - r*r;\n\
    double disc = B*B - 4*A*C;\n\
\n\
    if (disc <= 0) return;\n\
\n\
    double q = B < 0 ? (-B + sqrt(disc))/2 : (-B - sqrt(disc))/2;\n\
    \n\
    (*output)[0] = q/A;\n\
    (*output)[1] = C/q;\n\
}\n\
\n\
";


/* for now, just get the first device from the first platform  */
static cl_device_id
clt_get_cl_device()
{
    cl_int error;
    cl_platform_id platform;
    cl_device_id device;

    error = clGetPlatformIDs(1, &platform, NULL);
    if (error != CL_SUCCESS) bu_bomb("failed to find an OpenCL platform");

    error = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    if (error == CL_DEVICE_NOT_FOUND)
	error = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device, NULL);
    if (error != CL_SUCCESS) bu_bomb("failed to find an OpenCL device (using this method)");

    return device;
}


static cl_program
clt_get_program(cl_context context, cl_device_id device, const char *code)
{
    cl_int error;
    cl_program program;
    size_t code_size = strnlen(code, 1<<20);

    program = clCreateProgramWithSource(context, 1, &code, &code_size, &error);
    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL program");

    error = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (error != CL_SUCCESS) {
	size_t log_size;
	char *log_data;
	clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
	log_data = bu_malloc(log_size*sizeof(char), "failed to allocate memory for log");
	clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size+1, log_data, NULL);
	bu_log("BUILD LOG:\n%s\n", log_data);
	bu_bomb("failed to build OpenCL program");
    }

    return program;
}



static void
clt_init()
{
    cl_int error;

    bu_semaphore_acquire(clt_semaphore);
    if (clt_initialized) {
	bu_semaphore_release(clt_semaphore);
	return;
    }

    clt_initialized = 1;
    atexit(clt_cleanup);

    clt_device = clt_get_cl_device();

    clt_context = clCreateContext(NULL, 1, &clt_device, NULL, NULL, &error);
    if (error != CL_SUCCESS) bu_bomb("failed to create an OpenCL context");

    clt_queue = clCreateCommandQueue(clt_context, clt_device, 0, &error);
    if (error != CL_SUCCESS) bu_bomb("failed to create an OpenCL command queue");

    clt_program = clt_get_program(clt_context, clt_device, clt_program_code);

    clt_kernel = clCreateKernel(clt_program, "sph_shot", &error);
    if (error != CL_SUCCESS) bu_bomb("failed to create an OpenCL kernel");

    bu_log("initialized OpenCL");
    bu_semaphore_release(clt_semaphore);
}


static cl_double3
clt_shot(cl_double3 o /* ray origin */, cl_double3 l /* ray direction (unit vector) */,
cl_double3 c /* sphere center */, cl_double r /* sphere radius */)
{
    cl_int error;
    cl_mem output;
    cl_double3 result;
    cl_event done_kernel;
    const size_t global_size = 1;


    VSET(result.s, 0, 0, 0);
    output = clCreateBuffer(clt_context, CL_MEM_USE_HOST_PTR | CL_MEM_HOST_READ_ONLY | CL_MEM_WRITE_ONLY,
	    sizeof(cl_double3), &result, &error);
    if (error != CL_SUCCESS) bu_bomb("failed to create OpenCL output buffer");

    /* it's too expensive to create a new kernel for each shot; disable parallelism from librt */
    bu_semaphore_acquire(clt_semaphore);

    error = clSetKernelArg(clt_kernel, 0, sizeof(cl_mem), &output);
    error |= clSetKernelArg(clt_kernel, 1, sizeof(cl_double3), &o);
    error |= clSetKernelArg(clt_kernel, 2, sizeof(cl_double3), &l);
    error |= clSetKernelArg(clt_kernel, 3, sizeof(cl_double3), &c);
    error |= clSetKernelArg(clt_kernel, 4, sizeof(cl_double), &r);
    if (error != CL_SUCCESS) bu_bomb("failed to set OpenCL kernel arguments");

    error = clEnqueueNDRangeKernel(clt_queue, clt_kernel, 1, NULL, &global_size, NULL, 0, NULL, &done_kernel);
    if (error != CL_SUCCESS) bu_bomb("failed to enqueue OpenCL kernel");
    if (clWaitForEvents(1, &done_kernel) != CL_SUCCESS) bu_bomb("failure in clWaitForEvents()");

    bu_semaphore_release(clt_semaphore);

    clReleaseMemObject(output);
    return result;
}
#endif


/**
 * R T _ S P H _ P R E P
 *
 * Given a pointer to a GED database record, and a transformation matrix,
 * determine if this is a valid sphere, and if so, precompute various
 * terms of the formula.
 *
 * Returns -
 * 0 SPH is OK
 * !0 Error in description
 *
 * Implicit return -
 * A struct sph_specific is created, and its address is stored in
 * stp->st_specific for use by rt_sph_shot().
 * If the ELL is really a SPH, stp->st_id is modified to ID_SPH.
 */
int
rt_sph_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    register struct sph_specific *sph;
    fastf_t magsq_a, magsq_b, magsq_c;
    vect_t Au, Bu, Cu;	/* A, B, C with unit length */
    fastf_t f;
    struct rt_ell_internal *eip;
clt_init();
    eip = (struct rt_ell_internal *)ip->idb_ptr;
    RT_ELL_CK_MAGIC(eip);

#ifdef OPENCL
    clt_init();
#endif

    /* Validate that |A| > 0, |B| > 0, |C| > 0 */
    magsq_a = MAGSQ(eip->a);
    magsq_b = MAGSQ(eip->b);
    magsq_c = MAGSQ(eip->c);
    if (magsq_a < rtip->rti_tol.dist_sq || magsq_b < rtip->rti_tol.dist_sq || magsq_c < rtip->rti_tol.dist_sq) {
	bu_log("rt_sph_prep():  sph(%s) zero length A(%g), B(%g), or C(%g) vector\n",
	       stp->st_name, magsq_a, magsq_b, magsq_c);
	return 1;		/* BAD */
    }

    /* Validate that |A|, |B|, and |C| are nearly equal */
    if (!NEAR_EQUAL(magsq_a, magsq_b, rtip->rti_tol.dist_sq)
	|| !NEAR_EQUAL(magsq_a, magsq_c, rtip->rti_tol.dist_sq)) {
	return 1;		/* ELL, not SPH */
    }

    /* Create unit length versions of A, B, C */
    f = 1.0/sqrt(magsq_a);
    VSCALE(Au, eip->a, f);
    f = 1.0/sqrt(magsq_b);
    VSCALE(Bu, eip->b, f);
    f = 1.0/sqrt(magsq_c);
    VSCALE(Cu, eip->c, f);

    /* Validate that A.B == 0, B.C == 0, A.C == 0 (check dir only) */
    f = VDOT(Au, Bu);
    if (! NEAR_ZERO(f, rtip->rti_tol.dist)) {
	bu_log("rt_sph_prep():  sph(%s) A not perpendicular to B, f=%f\n", stp->st_name, f);
	return 1;		/* BAD */
    }
    f = VDOT(Bu, Cu);
    if (! NEAR_ZERO(f, rtip->rti_tol.dist)) {
	bu_log("rt_sph_prep():  sph(%s) B not perpendicular to C, f=%f\n", stp->st_name, f);
	return 1;		/* BAD */
    }
    f = VDOT(Au, Cu);
    if (! NEAR_ZERO(f, rtip->rti_tol.dist)) {
	bu_log("rt_sph_prep():  sph(%s) A not perpendicular to C, f=%f\n", stp->st_name, f);
	return 1;		/* BAD */
    }

    /*
     * This ELL is really an SPH
     */
    stp->st_id = ID_SPH;		/* "fix" soltab ID */
    stp->st_meth = &OBJ[ID_SPH];

    /* Solid is OK, compute constant terms now */
    BU_GET(sph, struct sph_specific);
    stp->st_specific = (genptr_t)sph;

    VMOVE(sph->sph_V, eip->v);

    sph->sph_radsq = magsq_a;
    sph->sph_rad = sqrt(sph->sph_radsq);
    sph->sph_invrad = 1.0 / sph->sph_rad;

    /*
     * Save the matrix which rotates our ABC to world
     * XYZ respectively, and scales points on surface
     * to unit length.  Used here in UV mapping.
     * See ell.c for details.
     */
    MAT_IDN(sph->sph_SoR);
    VSCALE(&sph->sph_SoR[0], eip->a, 1.0/magsq_a);
    VSCALE(&sph->sph_SoR[4], eip->b, 1.0/magsq_b);
    VSCALE(&sph->sph_SoR[8], eip->c, 1.0/magsq_c);

    /* Compute bounding sphere */
    VMOVE(stp->st_center, sph->sph_V);
    stp->st_aradius = stp->st_bradius = sph->sph_rad;

    /* Compute bounding RPP */
    if (stp->st_meth->ft_bbox(ip, &(stp->st_min), &(stp->st_max), &(rtip->rti_tol))) return 1;
    return 0;			/* OK */
}


/**
 * R T _ S P H _ P R I N T
 */
void
rt_sph_print(register const struct soltab *stp)
{
    register const struct sph_specific *sph =
	(struct sph_specific *)stp->st_specific;

    VPRINT("V", sph->sph_V);
    bu_log("Rad %g\n", sph->sph_rad);
    bu_log("Radsq %g\n", sph->sph_radsq);
    bu_log("Invrad %g\n", sph->sph_invrad);
    bn_mat_print("S o R", sph->sph_SoR);
}


/**
 * R T _ S P H _ S H O T
 *
 * Intersect a ray with a sphere.  If an intersection occurs, a struct
 * seg will be acquired and filled in.
 *
 * Notes: In the quadratic equation, A is MAGSQ(r_dir) which is always
 * equal to 1, so it does not appear.  The sign of B is reversed
 * (vector is reversed) to save negation.  We have factored out the 2
 * and 4 constants.
 *
 * Claim: The straight quadratic formula leads to precision problems
 * if either A or C are small.  In our case A is always 1.  C is a
 * radial distance of the ray origin from the sphere surface.  Thus if
 * we are shooting from near the surface we may have problems.  XXX -
 * investigate this.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_sph_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
#ifdef OPENCL
    cl_double3 o; /* ray origin  */
    cl_double3 l; /* ray direction (unit vector) */
    cl_double3 c; /* sphere center  */
    cl_double r; /* sphere radius */
    cl_double3 result;
    struct seg *segp;

    (void)rp; (void)ap;

    VMOVE(o.s, rp->r_pt);
    VMOVE(l.s, rp->r_dir);
    VMOVE(c.s, stp->st_center);
    r = ((struct sph_specific *)stp->st_specific)->sph_rad;
    result = clt_shot(o, l, c, r);

    if (EQUAL(result.s[0], 0)) return 0; /* no hit  */

    RT_GET_SEG(segp, ap->a_resource);
    segp->seg_stp = stp;

    segp->seg_in.hit_dist = result.s[1];
    segp->seg_out.hit_dist = result.s[0];
    segp->seg_in.hit_surfno = 0;
    segp->seg_out.hit_surfno = 0;
    BU_LIST_INSERT(&(seghead->l), &(segp->l));
    return 2;			/* HIT */

#else
    register struct sph_specific *sph =
	(struct sph_specific *)stp->st_specific;
    register struct seg *segp;

    vect_t ov;		/* ray origin to center (V - P) */
    fastf_t magsq_ov;	/* length squared of ov */
    fastf_t b;		/* second term of quadratic eqn */
    fastf_t root;		/* root of radical */

    VSUB2(ov, sph->sph_V, rp->r_pt);
    b = VDOT(rp->r_dir, ov);
    magsq_ov = MAGSQ(ov);

    if (magsq_ov >= sph->sph_radsq) {
	/* ray origin is outside of sphere */
	if (b < 0) {
	    /* ray direction is away from sphere */
	    return 0;		/* No hit */
	}
	root = b*b - magsq_ov + sph->sph_radsq;
	if (root <= 0) {
	    /* no real roots */
	    return 0;		/* No hit */
	}
    } else {
	root = b*b - magsq_ov + sph->sph_radsq;
    }
    root = sqrt(root);

    RT_GET_SEG(segp, ap->a_resource);
    segp->seg_stp = stp;

    /* we know root is positive, so we know the smaller t */
    segp->seg_in.hit_dist = b - root;
    segp->seg_out.hit_dist = b + root;
    segp->seg_in.hit_surfno = 0;
    segp->seg_out.hit_surfno = 0;
    BU_LIST_INSERT(&(seghead->l), &(segp->l));
    return 2;			/* HIT */
#endif
}


#define RT_SPH_SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;
/**
 * R T _ S P H _ V S H O T
 *
 * This is the Becker vectorized version
 */
void
rt_sph_vshot(struct soltab **stp, struct xray **rp, struct seg *segp, int n, struct application *ap)
    /* An array of solid pointers */
    /* An array of ray pointers */
    /* array of segs (results returned) */
    /* Number of ray/object pairs */

{
    register struct sph_specific *sph;
    register int i;

    vect_t ov;		/* ray origin to center (V - P) */
    fastf_t magsq_ov;	/* length squared of ov */
    fastf_t b;		/* second term of quadratic eqn */
    fastf_t root;		/* root of radical */

    if (ap) RT_CK_APPLICATION(ap);

    /* for each ray/sphere pair */
    for (i = 0; i < n; i++) {
	if (stp[i] == 0) continue; /* stp[i] == 0 signals skip ray */

	sph = (struct sph_specific *)stp[i]->st_specific;
	VSUB2(ov, sph->sph_V, rp[i]->r_pt);
	b = VDOT(rp[i]->r_dir, ov);
	magsq_ov = MAGSQ(ov);

	if (magsq_ov >= sph->sph_radsq) {
	    /* ray origin is outside of sphere */
	    if (b < 0) {
		/* ray direction is away from sphere */
		RT_SPH_SEG_MISS(segp[i]);		/* No hit */
		continue;
	    }
	    root = b*b - magsq_ov + sph->sph_radsq;
	    if (root <= 0) {
		/* no real roots */
		RT_SPH_SEG_MISS(segp[i]);		/* No hit */
		continue;
	    }
	} else {
	    root = b*b - magsq_ov + sph->sph_radsq;
	}
	root = sqrt(root);

	segp[i].seg_stp = stp[i];

	/* we know root is positive, so we know the smaller t */
	segp[i].seg_in.hit_dist = b - root;
	segp[i].seg_out.hit_dist = b + root;
	segp[i].seg_in.hit_surfno = 0;
	segp[i].seg_out.hit_surfno = 0;
    }
}


/**
 * R T _ S P H _ N O R M
 *
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_sph_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
    register struct sph_specific *sph =
	(struct sph_specific *)stp->st_specific;

    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
    VSUB2(hitp->hit_normal, hitp->hit_point, sph->sph_V);
    VSCALE(hitp->hit_normal, hitp->hit_normal, sph->sph_invrad);
}


/**
 * R T _ S P H _ C U R V E
 *
 * Return the curvature of the sphere.
 */
void
rt_sph_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
    register struct sph_specific *sph =
	(struct sph_specific *)stp->st_specific;

    cvp->crv_c1 = cvp->crv_c2 = - sph->sph_invrad;

    /* any tangent direction */
    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
}


/**
 * R T _ S P H _ U V
 *
 * For a hit on the surface of an SPH, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1.
 *
 * u = azimuth
 * v = elevation
 */
void
rt_sph_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
    register struct sph_specific *sph =
	(struct sph_specific *)stp->st_specific;
    fastf_t r;
    vect_t work;
    vect_t pprime;

    /* hit_point is on surface; project back to unit sphere, creating
     * a vector from vertex to hit point which always has length=1.0
     */
    VSUB2(work, hitp->hit_point, sph->sph_V);
    MAT4X3VEC(pprime, sph->sph_SoR, work);
    /* Assert that pprime has unit length */

    /* U is azimuth, atan() range: -pi to +pi */
    uvp->uv_u = bn_atan2(pprime[Y], pprime[X]) * bn_inv2pi;
    if (uvp->uv_u < 0)
	uvp->uv_u += 1.0;
    /*
     * V is elevation, atan() range: -pi/2 to +pi/2, because sqrt()
     * ensures that X parameter is always >0
     */
    uvp->uv_v = bn_atan2(pprime[Z],
			 sqrt(pprime[X] * pprime[X] + pprime[Y] * pprime[Y])) *
	bn_invpi + 0.5;

    /* approximation: r / (circumference, 2 * pi * aradius) */
    r = ap->a_rbeam + ap->a_diverge * hitp->hit_dist;
    uvp->uv_du = uvp->uv_dv =
	bn_inv2pi * r / stp->st_aradius;
}


/**
 * R T _ S P H _ F R E E
 */
void
rt_sph_free(register struct soltab *stp)
{
    register struct sph_specific *sph =
	(struct sph_specific *)stp->st_specific;

    BU_PUT(sph, struct sph_specific);
}


int
rt_sph_class(void)
{
    return 0;
}


/**
 * R T _ S P H _ P A R A M S
 *
 */
int
rt_sph_params(struct pc_pc_set *UNUSED(ps), const struct rt_db_internal *ip)
{
    if (ip) RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}


/* ELL versions are used for many of the callbacks */


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
