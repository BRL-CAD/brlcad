#include "common.cl"


/*
 * Values for Solid ID.
 */
#define ID_TOR          1       /**< @brief Toroid */
#define ID_TGC          2       /**< @brief Generalized Truncated General Cone */
#define ID_ELL          3       /**< @brief Ellipsoid */
#define ID_ARB8         4       /**< @brief Generalized ARB.  V + 7 vectors */
#define ID_SPH          10      /**< @brief Sphere */
#define ID_EHY          20      /**< @brief Elliptical Hyperboloid  */


__kernel void
solid_shot(global int *len, global struct hit *res, const double3 r_pt, const double3 r_dir, const int id, global const void *args)
{
    int isect;
    
    switch (id) {
    case ID_ARB8:	isect = arb_shot(res, r_pt, r_dir, args);	break;
    case ID_EHY:	isect = ehy_shot(res, r_pt, r_dir, args);	break;
    case ID_ELL:	isect = ell_shot(res, r_pt, r_dir, args);	break;
    case ID_SPH:	isect = sph_shot(res, r_pt, r_dir, args);	break;
    case ID_TGC:	isect = tgc_shot(res, r_pt, r_dir, args);	break;
    case ID_TOR:	isect = tor_shot(res, r_pt, r_dir, args);	break;
    default:		isect = 0;                			break;
    };
    *len = isect;
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

