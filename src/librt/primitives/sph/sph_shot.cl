#ifdef cl_khr_fp64
    #pragma OPENCL EXTENSION cl_khr_fp64 : enable
#elif defined(cl_amd_fp64)
    #pragma OPENCL EXTENSION cl_amd_fp64 : enable
#else
    #error "Double precision floating point not supported by OpenCL implementation."
#endif


__kernel void
sph_shot(global __write_only double2 *dist, global __write_only int2 *surfno,
const double3 r_pt, const double3 r_dir,
const double3 V, const double radsq)
{
    double3 ov;        // ray origin to center (V - P)
    double magsq_ov;   // length squared of ov
    double b;          // second term of quadratic eqn
    double root;       // root of radical

    ov = V - r_pt;
    b = dot(r_dir, ov);
    magsq_ov = dot(ov, ov);

    if (magsq_ov >= radsq) {
	// ray origin is outside of sphere
	if (b < 0) {
	    // ray direction is away from sphere
	    *surfno = (int2){INT_MAX, INT_MAX};
	    return;           // No hit
	}
	root = b*b - magsq_ov + radsq;
	if (root <= 0) {
	    // no real roots
	    *surfno = (int2){INT_MAX, INT_MAX};
	    return;           // No hit
	}
    } else {
	root = b*b - magsq_ov + radsq;
    }
    root = sqrt(root);

    // we know root is positive, so we know the smaller t
    *surfno = (int2){0, 0};
    *dist = (double2){b - root, b + root};
    return;        // HIT
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
