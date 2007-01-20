/*                          P L O T . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file plot.c
 *
 *	Provide UNIX-plot output of the current view.
 *
 *  Authors -
 *  	Michael John Muuss	(This version)
 *	Douglas A. Gwyn		(3-D UNIX Plot routines)
 *  	Gary S. Moss		(Original gedplot program)
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <math.h>
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "plot3.h"

#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"


/*
 *  			F _ P L O T
 *
 *  plot file [opts]
 *  potential options might include:
 *	grid, 3d w/color, |filter, infinite Z
 */
int
f_plot(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	register struct solid		*sp;
	register struct rt_vlist	*vp;
	register FILE *fp;
	static vect_t clipmin, clipmax;
	static vect_t last;		/* last drawn point */
	static vect_t fin;
	static vect_t start;
	int Three_D;			/* 0=2-D -vs- 1=3-D */
	int Z_clip;			/* Z clipping */
	int Dashing;			/* linetype is dashed */
	int floating;			/* 3-D floating point plot */
	int	is_pipe = 0;

	if(argc < 2){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help plot");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( not_state( ST_VIEW, "UNIX Plot of view" ) )
	  return TCL_ERROR;

	/* Process any options */
	Three_D = 1;				/* 3-D w/color, by default */
	Z_clip = 0;				/* NO Z clipping, by default*/
	floating = 0;
	while( argv[1] != (char *)0 && argv[1][0] == '-' )  {
		switch( argv[1][1] )  {
		case 'f':
			floating = 1;
			break;
		case '3':
			Three_D = 1;
			break;
		case '2':
			Three_D = 0;		/* 2-D, for portability */
			break;
		case 'g':
		  /* do grid */
		  Tcl_AppendResult(interp, "grid unimplemented\n", (char *)NULL);
		  break;
		case 'z':
		case 'Z':
		  /* Enable Z clipping */
		  Tcl_AppendResult(interp, "Clipped in Z to viewing cube\n", (char *)NULL);
		  Z_clip = 1;
		  break;
		default:
		  Tcl_AppendResult(interp, "bad PLOT option ", argv[1], "\n", (char *)NULL);
		  break;
		}
		argv++;
	}
	if( argv[1] == (char *)0 )  {
	  Tcl_AppendResult(interp, "no filename or filter specified\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( argv[1][0] == '|' )  {
		struct bu_vls	str;
		bu_vls_init( &str );
		bu_vls_strcpy( &str, &argv[1][1] );
		while( (++argv)[1] != (char *)0 )  {
			bu_vls_strcat( &str, " " );
			bu_vls_strcat( &str, argv[1] );
		}
		if( (fp = popen( bu_vls_addr( &str ), "w" ) ) == NULL )  {
			perror( bu_vls_addr( &str ) );
			return TCL_ERROR;
		}

		Tcl_AppendResult(interp, "piped to ", bu_vls_addr( &str ),
				 "\n", (char *)NULL);
		bu_vls_free( &str );
		is_pipe = 1;
	}  else  {
		if( (fp = fopen( argv[1], "w" )) == NULL )  {
		  perror( argv[1] );
		  return TCL_ERROR;
		}

		Tcl_AppendResult(interp, "plot stored in ", argv[1], "\n", (char *)NULL);
		is_pipe = 0;
	}

	if( floating )  {
		pd_3space( fp,
			-view_state->vs_vop->vo_center[MDX] - view_state->vs_vop->vo_scale,
			-view_state->vs_vop->vo_center[MDY] - view_state->vs_vop->vo_scale,
			-view_state->vs_vop->vo_center[MDZ] - view_state->vs_vop->vo_scale,
			-view_state->vs_vop->vo_center[MDX] + view_state->vs_vop->vo_scale,
			-view_state->vs_vop->vo_center[MDY] + view_state->vs_vop->vo_scale,
			-view_state->vs_vop->vo_center[MDZ] + view_state->vs_vop->vo_scale );
		Dashing = 0;
		pl_linmod( fp, "solid" );
		FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid)  {
			/* Could check for differences from last color */
			pl_color( fp,
				sp->s_color[0],
				sp->s_color[1],
				sp->s_color[2] );
			if( Dashing != sp->s_soldash )  {
				if( sp->s_soldash )
					pl_linmod( fp, "dotdashed");
				else
					pl_linmod( fp, "solid");
				Dashing = sp->s_soldash;
			}
			rt_vlist_to_uplot( fp, &(sp->s_vlist) );
		}
		goto out;
	}

	/*
	 *  Integer output version, either 2-D or 3-D.
	 *  Viewing region is from -1.0 to +1.0
	 *  which is mapped to integer space -2048 to +2048 for plotting.
	 *  Compute the clipping bounds of the screen in view space.
	 */
	clipmin[X] = -1.0;
	clipmax[X] =  1.0;
	clipmin[Y] = -1.0;
	clipmax[Y] =  1.0;
	if( Z_clip )  {
		clipmin[Z] = -1.0;
		clipmax[Z] =  1.0;
	} else {
		clipmin[Z] = -1.0e20;
		clipmax[Z] =  1.0e20;
	}

	if( Three_D )
		pl_3space( fp, (int)GED_MIN, (int)GED_MIN, (int)GED_MIN, (int)GED_MAX, (int)GED_MAX, (int)GED_MAX );
	else
		pl_space( fp, (int)GED_MIN, (int)GED_MIN, (int)GED_MAX, (int)GED_MAX );
	pl_erase( fp );
	Dashing = 0;
	pl_linmod( fp, "solid");
	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid)  {
		if( Dashing != sp->s_soldash )  {
			if( sp->s_soldash )
				pl_linmod( fp, "dotdashed");
			else
				pl_linmod( fp, "solid");
			Dashing = sp->s_soldash;
		}
		for( BU_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
			register int	i;
			register int	nused = vp->nused;
			register int	*cmd = vp->cmd;
			register point_t *pt = vp->pt;
			for( i = 0; i < nused; i++,cmd++,pt++ )  {
				switch( *cmd )  {
				case RT_VLIST_POLY_START:
				case RT_VLIST_POLY_VERTNORM:
					continue;
				case RT_VLIST_POLY_MOVE:
				case RT_VLIST_LINE_MOVE:
					/* Move, not draw */
					MAT4X3PNT( last, view_state->vs_vop->vo_model2view, *pt );
					continue;
				case RT_VLIST_POLY_DRAW:
				case RT_VLIST_POLY_END:
				case RT_VLIST_LINE_DRAW:
					/* draw */
					MAT4X3PNT(fin, view_state->vs_vop->vo_model2view, *pt);
					VMOVE( start, last );
					VMOVE( last, fin );
					break;
				}
				if(
					vclip( start, fin, clipmin, clipmax ) == 0
				)  continue;

				if( Three_D )  {
					/* Could check for differences from last color */
					pl_color( fp,
						sp->s_color[0],
						sp->s_color[1],
						sp->s_color[2] );
					pl_3line( fp,
						(int)( start[X] * GED_MAX ),
						(int)( start[Y] * GED_MAX ),
						(int)( start[Z] * GED_MAX ),
						(int)( fin[X] * GED_MAX ),
						(int)( fin[Y] * GED_MAX ),
						(int)( fin[Z] * GED_MAX ) );
				}  else  {
					pl_line( fp,
						(int)( start[0] * GED_MAX ),
						(int)( start[1] * GED_MAX ),
						(int)( fin[0] * GED_MAX ),
						(int)( fin[1] * GED_MAX ) );
				}
			}
		}
	}
out:
	if( is_pipe )
		(void)pclose( fp );
	else
		(void)fclose( fp );

	return TCL_ERROR;
}

int
f_area(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	register struct solid		*sp;
	register struct rt_vlist	*vp;
	static vect_t last;
	static vect_t fin;
	FILE *fp_r;
	FILE *fp_w;
	int fd1[2]; /* mged | cad_boundp */
	int fd2[2]; /* cad_boundp | cad_parea */
	int fd3[2]; /* cad_parea | mged */
	int pid1;
	int pid2;
	int rpid;
	int retcode;
	char result[RT_MAXLINE] = {0};
	char tol_str[32] = {0};
	char *tol_ptr;

#ifndef _WIN32
	/* XXX needs fixing */

	CHECK_DBI_NULL;

	if(argc < 1 || 2 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help area");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( not_state( ST_VIEW, "Presented Area Calculation" ) == TCL_ERROR )
		return TCL_ERROR;

	if( BU_LIST_IS_EMPTY( &dgop->dgo_headSolid ) ) {
		Tcl_AppendResult(interp, "No objects displayed!!!\n", (char *)NULL );
		return TCL_ERROR;
	}

	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid)  {
	  if( !sp->s_Eflag && sp->s_soldash != 0 )  {
	    struct bu_vls vls;

	    bu_vls_init(&vls);
	    bu_vls_printf(&vls, "help area");
	    Tcl_Eval(interp, bu_vls_addr(&vls));
	    bu_vls_free(&vls);
	    return TCL_ERROR;
	  }
	}

	if(argc == 2){
	  Tcl_AppendResult(interp, "Tolerance is ", argv[1], "\n", (char *)NULL);
	  tol_ptr = argv[1];
	}else{
	  struct bu_vls tmp_vls;
	  double tol = 0.005;

	  bu_vls_init(&tmp_vls);
	  sprintf(tol_str, "%e", tol);
	  tol_ptr = tol_str;
	  bu_vls_printf(&tmp_vls, "Auto-tolerance is %s\n", tol_str);
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	}

	if(pipe(fd1) != 0){
	  perror("f_area");
	  return TCL_ERROR;
	}

	if(pipe(fd2) != 0){
	  perror("f_area");
	  return TCL_ERROR;
	}

	if(pipe(fd3) != 0){
	  perror("f_area");
	  return TCL_ERROR;
	}

	if ((pid1 = fork()) == 0){
	  dup2(fd1[0], fileno(stdin));
	  dup2(fd2[1], fileno(stdout));

	  close(fd1[0]);
	  close(fd1[1]);
	  close(fd2[0]);
	  close(fd2[1]);
	  close(fd3[0]);
	  close(fd3[1]);

	  execlp("cad_boundp", "cad_boundp", "-t", tol_ptr, (char *)NULL);
	}

	if ((pid2 = fork()) == 0){
	  dup2(fd2[0], fileno(stdin));
	  dup2(fd3[1], fileno(stdout));

	  close(fd1[0]);
	  close(fd1[1]);
	  close(fd2[0]);
	  close(fd2[1]);
	  close(fd3[0]);
	  close(fd3[1]);

	  execlp("cad_parea", "cad_parea", (char *)NULL);
	}

	close(fd1[0]);
	close(fd2[0]);
	close(fd2[1]);
	close(fd3[1]);

	fp_w = fdopen(fd1[1], "w");
	fp_r = fdopen(fd3[0], "r");

	/*
	 * Write out rotated but unclipped, untranslated,
	 * and unscaled vectors
	 */
	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid)  {
	  for( BU_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
	    register int	i;
	    register int	nused = vp->nused;
	    register int	*cmd = vp->cmd;
	    register point_t *pt = vp->pt;
	    for( i = 0; i < nused; i++,cmd++,pt++ )  {
	      switch( *cmd )  {
	      case RT_VLIST_POLY_START:
	      case RT_VLIST_POLY_VERTNORM:
		continue;
	      case RT_VLIST_POLY_MOVE:
	      case RT_VLIST_LINE_MOVE:
		/* Move, not draw */
		MAT4X3VEC(last, view_state->vs_vop->vo_rotation, *pt);
		continue;
	      case RT_VLIST_POLY_DRAW:
	      case RT_VLIST_POLY_END:
	      case RT_VLIST_LINE_DRAW:
		/* draw.  */
		MAT4X3VEC(fin, view_state->vs_vop->vo_rotation, *pt);
		break;
	      }

	      fprintf(fp_w, "%.9e %.9e %.9e %.9e\n",
		      last[X] * base2local,
		      last[Y] * base2local,
		      fin[X] * base2local,
		      fin[Y] * base2local );

	      VMOVE( last, fin );
	    }
	  }
	}

	fclose(fp_w);

	Tcl_AppendResult(interp, "Presented area from this viewpoint, square ",
			 bu_units_string(dbip->dbi_local2base), ":\n", (char *)NULL);

	/* Read result */
	fgets(result, RT_MAXLINE, fp_r);
	Tcl_AppendResult(interp, result, "\n", (char *)NULL);

	while ((rpid = wait(&retcode)) != pid1 && rpid != -1);
	while ((rpid = wait(&retcode)) != pid2 && rpid != -1);

	fclose(fp_r);
	close(fd1[1]);
	close(fd3[0]);
#endif

	return TCL_OK;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
