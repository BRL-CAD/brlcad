#include "on_nurb.h"

/**
 *  			R T _ N U R B _ P R E P
 *
 *  Given a pointer of a GED database record, and a transformation matrix,
 *  determine if this is a valid NURB, and if so, prepare the surface
 *  so the intersections will work.
 */
int
rt_on_nurb_prep(struct soltab *stp, struct rt_db_internal* ip, struct rt_i* rtip)
{
  /* This prepares the NURBS specific data structures to be used
     during intersection... i.e. acceleration data structures and
     whatever else is needed.
     
     Abert's paper (Direct and Fast Ray Tracing of NURBS Surfaces)
     suggests using a bounding volume hierarchy (instead of KD-tree)
     and building it down to a satisfactory flatness criterion (which
     they do not give information about).
  */
  
  return 0;
}


/**
 *			R T _ N U R B _ P R I N T
 */
void
rt_on_nurb_print(register const struct soltab *stp)
{
}


/**
 *  			R T _ N U R B _ S H O T
 *
 *  Intersect a ray with a nurb.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_on_nurb_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
  return 0;
}


/**
 *  			R T _ N U R B _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_on_nurb_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
}


/**
 *			R T _ N U R B _ C U R V E
 *
 *  Return the curvature of the nurb.
 */
void
rt_on_nurb_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
}

/**
 *  			R T _ N U R B _ U V
 *
 *  For a hit on the surface of an nurb, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_on_nurb_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
}


/**
 *		R T _ N U R B _ F R E E
 */
void
rt_on_nurb_free(register struct soltab *stp)
{
}


/**
 *			R T _ N U R B _ P L O T
 */
int
rt_on_nurb_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
  return 0;
}


/**
 *			R T _ N U R B _ T E S S
 */
int
rt_on_nurb_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
  return -1;
}


/**
 *			R T _ N U R B _ E X P O R T 5
 */
int
rt_on_nurb_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
  return 0;
}


/**
 *			R T _ N U R B _ I M P O R T 5
 */
int
rt_on_nurb_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
  return 0;
}


/**
 *			R T _ N U R B _ I F R E E
 */
void
rt_on_nurb_ifree(struct rt_db_internal *ip)
{
}


/**
 *			R T _ N U R B _ D E S C R I B E
 */
int
rt_on_nurb_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
  return 0;
}

/**
 *                      R T _ N U R B _ T C L G E T
 */
int
rt_on_nurb_tclget(Tcl_Interp *interp, const struct rt_db_internal *intern, const char *attr)
{
  return 0;
}


/**
 *                      R T _ N U R B _ T C L A D J U S T
 */
int
rt_on_nurb_tcladjust(Tcl_Interp *interp, struct rt_db_internal *intern, int argc, char **argv)
{
  return 0;
}


