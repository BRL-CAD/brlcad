#pragma OPENCL EXTENSION cl_khr_fp64: enable

/*
__kernel void sph_shot(__global __write_only double3 *output,
	const double3 o, const double3 l, const double3 c, const double r)
{
    double A = dot(l, l);
    double B = 2*dot(l, o-c);
    double C = dot(o-c, o-c) - r*r;
    double disc = B*B - 4*A*C;

    if (disc <= 0) return;
    double q = B < 0 ? (-B + sqrt(disc))/2 : (-B - sqrt(disc))/2;
    (*output)[0] = q/A;
    (*output)[1] = C/q;
}
*/


__kernel void sph_shot(__global __write_only double3 *output,
	const double3 o, const double3 dir, const double3 V, const double radsq)
{
    double3 ov;        /* ray origin to center (V - P) */
    double magsq_ov;   /* length squared of ov */
    double b;          /* second term of quadratic eqn */
    double root;       /* root of radical */

    ov = V - o;
    magsq_ov = ov[0]*ov[0] + ov[1]*ov[1] + ov[2]*ov[2];
    printf("TZ: ov: %0.30f\t%0.30f\t%0.30f\n", ov[0], ov[1], ov[2]);
    b = dot(dir, ov);
    printf("TZ: b: %0.30f\n", b);

    if (magsq_ov >= radsq) {
	/* ray origin is outside of sphere */
	if (b < 0) {
	    /* ray direction is away from sphere */
	    return;           /* No hit */
	}
	root = b*b - magsq_ov + radsq;
	if (root <= 0) {
	    /* no real roots */
	    return;           /* No hit */
	}
    } else {
	root = b*b - magsq_ov + radsq;
    }
    root = sqrt(root);

    (*output)[0] = b - root;
    (*output)[1] = b + root;
    return;        /* HIT */
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
