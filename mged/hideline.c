/* 
 *			H I D E L I N E . C
 * 
 * Description -
 *	Takes the vector  list for the  current  display  and  raytraces
 *	along those vectors.  If the first point hit in the model is the
 *	same as that  vector,  continue  the line  through  that  point;
 *	otherwise,  stop  drawing  that  vector  or  draw  dotted  line.
 *	Produces Unix-plot type output.
 *
 *	The command is "H file.pl [stepsize] [%epsilon]". Stepsize is the
 *	number of segments into which the window size should be broken.
 *	%Epsilon specifies how close two points must be before they are
 *	considered equal. A good values for stepsize and %epsilon are 128
 *	and 1, respectively.
 *
 * Author -  
 *	Mark Huston Bowden  
 *
 * History -
 *	01 Aug 88		Began initial coding
 *  
 *  Source -
 *	Research  Institute,  E-47 
 *	University of Alabama in Huntsville  
 *	Huntsville, AL 35899
 *	(205) 895-6467	UAH
 *	(205) 876-1089	Redstone Arsenal
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./solid.h"
#include "./mged_dm.h"

#define MAXOBJECTS	3000

#define VIEWSIZE	(2*Viewscale)
#define TRUE	1
#define FALSE	0

#define MOVE(v)	  VMOVE(last_move,(v))

#define DRAW(v)	{ vect_t a,b;\
		  MAT4X3PNT(a,model2view,last_move);\
		  MAT4X3PNT(b,model2view,(v));\
		  pdv_3line(plotfp, a, b ); }

extern struct db_i *dbip;	/* current database instance */

fastf_t epsilon;
vect_t aim_point;
struct solid *sp;

/*
 * hit_headon - routine called by rt_shootray if ray hits model
 */

static int
hit_headon(ap,PartHeadp)
register struct application *ap;
struct partition *PartHeadp;
{
	register char diff_solid;
	vect_t	diff;
	register fastf_t len;

	if (PartHeadp->pt_forw->pt_forw != PartHeadp)
	  Tcl_AppendResult(interp, "hit_headon: multiple partitions\n", (char *)NULL);

	VJOIN1(PartHeadp->pt_forw->pt_inhit->hit_point,ap->a_ray.r_pt,
	    PartHeadp->pt_forw->pt_inhit->hit_dist, ap->a_ray.r_dir);
	VSUB2(diff,PartHeadp->pt_forw->pt_inhit->hit_point,aim_point);

	diff_solid = strcmp(sp->s_path[0]->d_namep,
	    PartHeadp->pt_forw->pt_inseg->seg_stp->st_name);
	len = MAGNITUDE(diff);

	if (	NEAR_ZERO(len,epsilon)
	    ||
	    ( diff_solid &&
	    VDOT(diff,ap->a_ray.r_dir) > 0 )
	    )
		return(1);
	else
		return(0);
}

/*
 * hit_tangent - routine called by rt_shootray if ray misses model
 *
 *     We know we are shooting at the model since we are aiming at the
 *     vector list MGED created. However, shooting at an edge or shooting
 *     tangent to a curve produces only one intersection point at which
 *     time rt_shootray reports a miss. Therefore, this routine is really
 *     a "hit" routine.
 */

static int
hit_tangent(ap,PartHeadp)
register struct application *ap;
struct partition *PartHeadp;
{
	return(1);		/* always a hit */
}

/*
 * hit_overlap - called by rt_shootray if ray hits an overlap
 */

static int
hit_overlap(ap,PartHeadp)
register struct application *ap;
struct partition *PartHeadp;
{
	return(0);		/* never a hit */
}

/*
 *			F _ H I D E L I N E
 */
int
f_hideline(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	FILE 	*plotfp;
	char 	visible;
	int 	i,numobjs;
	char 	*objname[MAXOBJECTS],title[1];
	fastf_t 	len,u,step;
	FAST float 	ratio;
	vect_t	last_move;
	struct rt_i	*rtip;
	struct resource resource;
	struct application a;
	vect_t temp;
	vect_t last,dir;
	register struct rt_vlist	*vp;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	if(argc < 2 || 4 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help H");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if ((plotfp = fopen(argv[1],"w")) == NULL) {
	  Tcl_AppendResult(interp, "f_hideline: unable to open \"", argv[1],
			   "\" for writing.\n", (char *)NULL);
	  return TCL_ERROR;
	}
	pl_space(plotfp,-2048,-2048,2048,2048);

	/*  Build list of objects being viewed */
	numobjs = 0;
	FOR_ALL_SOLIDS(sp) {
		for (i = 0; i < numobjs; i++)  {
			if( objname[i] == sp->s_path[0]->d_namep )
				break;
		}
		if (i == numobjs)
			objname[numobjs++] = sp->s_path[0]->d_namep;
	}

	Tcl_AppendResult(interp, "Generating hidden-line drawing of the following regions:\n",
			 (char *)NULL);
	for (i = 0; i < numobjs; i++)
	  Tcl_AppendResult(interp, "\t", objname[i], "\n", (char *)NULL);

	/* Initialization for librt */
	if ((rtip = rt_dirbuild(dbip->dbi_filename,title,0)) == RTI_NULL) {
	  Tcl_AppendResult(interp, "f_hideline: unable to open model file \"",
			   dbip->dbi_filename, "\"\n", (char *)NULL);
	  return TCL_ERROR;
	}
	a.a_hit = hit_headon;
	a.a_miss = hit_tangent;
	a.a_overlap = hit_overlap;
	a.a_rt_i = rtip;
	a.a_resource = &resource;
	a.a_level = 0;
	a.a_onehit = 1;
	a.a_diverge = 0;
	a.a_rbeam = 0;

	if (argc > 2) {
		sscanf(argv[2],"%f",&step);
		step = Viewscale/step;
		sscanf(argv[3],"%f",&epsilon);
		epsilon *= Viewscale/100;
	} else {
		step = Viewscale/256;
		epsilon = 0.1*Viewscale;
	}

	for (i = 0; i < numobjs; i++)
	  if (rt_gettree(rtip,objname[i]) == -1)
	    Tcl_AppendResult(interp, "f_hideline: rt_gettree failed on \"",
			     objname[i], "\"\n", (char *)NULL);

	/* Crawl along the vectors raytracing as we go */
	VSET(temp,0,0,-1);				/* looking at model */
	MAT4X3VEC(a.a_ray.r_dir,view2model,temp);
	VUNITIZE(a.a_ray.r_dir);

	FOR_ALL_SOLIDS(sp) {

		ratio = sp->s_size / VIEWSIZE;		/* ignore if small or big */
		if (ratio >= dmp->dmr_bound || ratio < 0.001)
			continue;

		Tcl_AppendResult(interp, "Solid\n", (char *)NULL);
		for( BU_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
			register int	i;
			register int	nused = vp->nused;
			register int	*cmd = vp->cmd;
			register point_t *pt = vp->pt;
			for( i = 0; i < nused; i++,cmd++,pt++ )  {
			  Tcl_AppendResult(interp, "\tVector\n", (char *)NULL);
				switch( *cmd )  {
				case RT_VLIST_POLY_START:
				case RT_VLIST_POLY_VERTNORM:
					break;
				case RT_VLIST_POLY_MOVE:
				case RT_VLIST_LINE_MOVE:
					/* move */
					VMOVE(last, *pt);
					MOVE(last);
					break;
				case RT_VLIST_POLY_DRAW:
				case RT_VLIST_POLY_END:
				case RT_VLIST_LINE_DRAW:
					/* setup direction && length */
					VSUB2(dir, *pt, last);
					len = MAGNITUDE(dir);
					VUNITIZE(dir);
					visible = FALSE;
					{
					  struct bu_vls tmp_vls;

					  bu_vls_init(&tmp_vls);
					  bu_vls_printf(&tmp_vls, "\t\tDraw 0 -> %g, step %g\n", len, step);
					  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
					  bu_vls_free(&tmp_vls);
					}
					for (u = 0; u <= len; u += step) {
						VJOIN1(aim_point,last,u,dir);
						MAT4X3PNT(temp,model2view,aim_point);
						temp[Z] = 100;			/* parallel project */
						MAT4X3PNT(a.a_ray.r_pt,view2model,temp);
						if (rt_shootray(&a)) {
							if (!visible) {
								visible = TRUE;
								MOVE(aim_point);
							}
						} else {
							if (visible) {
								visible = FALSE;
								DRAW(aim_point);
							}
						}
					}
					if (visible)
						DRAW(aim_point);
					VMOVE(last, *pt); /* new last vertex */
				}
			}
		}
	}
	fclose(plotfp);
	return TCL_OK;
}
