#ifdef cl_khr_fp64
    #pragma OPENCL EXTENSION cl_khr_fp64 : enable
#elif defined(cl_amd_fp64)
    #pragma OPENCL EXTENSION cl_amd_fp64 : enable
#else
    #error "Double precision floating point not supported by OpenCL implementation."
#endif


__kernel void sph_shot(__global __write_only double2 *output,
	const double3 o, const double3 dir, const double3 V, const double radsq)
{
    double3 ov;        // ray origin to center (V - P)
    double magsq_ov;   // length squared of ov
    double b;          // second term of quadratic eqn
    double root;       // root of radical

    ov = V - o;
    magsq_ov = dot(ov, ov);
    //printf("TZ: ov: %0.30f\t%0.30f\t%0.30f\n", ov.x, ov.y, ov.z);
    b = dot(dir, ov);
    //printf("TZ: b: %0.30f\n", b);

    if (magsq_ov >= radsq) {
	// ray origin is outside of sphere
	if (b < 0) {
	    // ray direction is away from sphere
	    return;           // No hit
	}
	root = b*b - magsq_ov + radsq;
	if (root <= 0) {
	    // no real roots
	    *output = (double2){0.0, 0.0};
	    return;           // No hit
	}
    } else {
	root = b*b - magsq_ov + radsq;
    }
    root = sqrt(root);

    *output = (double2){b - root, b + root};
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
