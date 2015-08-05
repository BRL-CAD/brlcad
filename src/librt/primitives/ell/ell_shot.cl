#ifdef cl_khr_fp64
    #pragma OPENCL EXTENSION cl_khr_fp64 : enable
#elif defined(cl_amd_fp64)
    #pragma OPENCL EXTENSION cl_amd_fp64 : enable
#else
    #error "Double precision floating point not supported by OpenCL implementation."
#endif


__kernel void
ell_shot(global __write_only double2 *dist, global __write_only int2 *surfno,
const double3 r_pt, const double3 r_dir,
const double3 V, const double16 SoR)
{
    double3 dprime;	// D'
    double3 pprime;	// P'
    double3 xlated;	// translated vector
    double dp, dd;	// D' dot P', D' dot D'
    double k1, k2;	// distance constants of solution
    double root;	// root of radical

    /* out, Mat, vect */
    const double f = 1.0/SoR.sf;
    dprime.x = dot(SoR.s012, r_dir) * f;
    dprime.y = dot(SoR.s456, r_dir) * f;
    dprime.z = dot(SoR.s89a, r_dir) * f;
    xlated = r_pt - V;
    pprime.x = dot(SoR.s012, xlated) * f;
    pprime.y = dot(SoR.s456, xlated) * f;
    pprime.z = dot(SoR.s89a, xlated) * f;

    dp = dot(dprime, pprime);
    dd = dot(dprime, dprime);

    if ((root = dp*dp - dd * (dot(pprime, pprime)-1.0)) < 0) {
	*surfno = (int2){INT_MAX, INT_MAX};
	return;		// No hit
    } else {
	root = sqrt(root);
	if ((k1=(-dp+root)/dd) <= (k2=(-dp-root)/dd)) {
	    /* k1 is entry, k2 is exit */
	    *dist = (double2){k1, k2};
	} else {
	    /* k2 is entry, k1 is exit */
	    *dist = (double2){k2, k1};
	}
	*surfno = (int2){0, 0};
	return;		// HIT
    }
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

