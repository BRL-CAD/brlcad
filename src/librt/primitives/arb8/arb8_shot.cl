#ifdef cl_khr_fp64
    #pragma OPENCL EXTENSION cl_khr_fp64 : enable
#elif defined(cl_amd_fp64)
    #pragma OPENCL EXTENSION cl_amd_fp64 : enable
#else
    #error "Double precision floating point not supported by OpenCL implementation."
#endif

#define SQRT_SMALL_FASTF 1.0e-39 /* This squared gives zero */


__kernel void
arb8_shot(global double2 *odist, global int2 *osurfno,
const double3 r_pt, const double3 r_dir,
global const double *peqns, const int nmfaces)
{
    int iplane, oplane;
    double in, out;	// ray in/out distances

    in = -INFINITY;
    out = INFINITY;
    iplane = oplane = -1;

    // consider each face
    for (int j = 0; j < nmfaces; j++) {
	const double4 peqn = vload4(j, peqns);
	double s;

	const double dxbdn = dot(peqn.xyz, r_pt) - peqn.w;
	const double dn = -dot(peqn.xyz, r_dir);

	if (dn < -SQRT_SMALL_FASTF) {
	    // exit point, when dir.N < 0.  out = min(out, s)
	    if (out > (s = dxbdn/dn)) {
		out = s;
		oplane = j;
	    }
	} else if (dn > SQRT_SMALL_FASTF) {
	    // entry point, when dir.N > 0.  in = max(in, s)
	    if (in < (s = dxbdn/dn)) {
		in = s;
		iplane = j;
	    }
	} else {
	    /* ray is parallel to plane when dir.N == 0.
	     * If it is outside the solid, stop now.
	     * Allow very small amount of slop, to catch
	     * rays that lie very nearly in the plane of a face.
	     */
	    if (dxbdn > SQRT_SMALL_FASTF) {
		*osurfno = (int2){INT_MAX, INT_MAX};
		return;	// MISS
	    }
	}
	if (in > out) {
	    *osurfno = (int2){INT_MAX, INT_MAX};
	    return;	// MISS
	}
    }
    /* Validate */
    if (iplane == -1 || oplane == -1) {
	*osurfno = (int2){INT_MAX, INT_MAX};
	return;		// MISS
    }
    if (in >= out || out >= INFINITY) {
	*osurfno = (int2){INT_MAX, INT_MAX};
	return;		// MISS
    }

    *odist = (double2){in, out};
    *osurfno = (int2){iplane, oplane};
    return;		// HIT
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
