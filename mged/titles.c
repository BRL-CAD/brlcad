/*
 *			T I T L E S . C
 *
 * Functions -
 *	dotitles	Output GED "faceplate" & titles.
 *
 *  Author -
 *	Michael John Muuss
 *	Kieth A. Applin
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#define USE_OLD_MENUS 0

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "externs.h"
#include "./ged.h"
#include "./titles.h"
#include "./mged_solid.h"
#include "./sedit.h"
#include "./mgedtcl.h"
#include "./mged_dm.h"

int	state;
char	*state_str[] = {
	"-ZOT-",
	"VIEWING",
	"SOL PICK",
	"SOL EDIT",
	"OBJ PICK",
	"OBJ PATH",
	"OBJ EDIT",
	"VERTPICK",
	"UNKNOWN",
};

extern mat_t perspective_mat;  /* defined in dozoom.c */
extern struct rt_db_internal	es_int;

/*
 *			C R E A T E _ T E X T _ O V E R L A Y
 *
 *  Prepare the numerical display of the currently edited solid/object.
 */
void
create_text_overlay( vp )
register struct bu_vls	*vp;
{
	struct directory	*dp;
	register int	i;
	struct bu_vls vls;

	BU_CK_VLS(vp);
	bu_vls_init(&vls);

	/*
	 * Set up for character output.  For the best generality, we
	 * don't assume that the display can process a CRLF sequence,
	 * so each line is written with a separate call to DM_DRAW_STRING_2D().
	 */

	/* print solid info at top of screen */
	if( es_edflag >= 0 ) {
		dp = illump->s_path[illump->s_last];

		bu_vls_strcat( vp, "** SOLID -- " );
		bu_vls_strcat( vp, dp->d_namep );
		bu_vls_strcat( vp, ": ");

		vls_solid( vp, &es_int, bn_mat_identity );

		if(illump->s_last) {
			bu_vls_strcat( vp, "\n** PATH --  ");
			for(i=0; i <= illump->s_last; i++) {
				bu_vls_strcat( vp, "/" );
				bu_vls_strcat( vp, illump->s_path[i]->d_namep);
			}
			bu_vls_strcat( vp, ": " );

			/* print the evaluated (path) solid parameters */
			vls_solid( vp, &es_int, es_mat );
		}
	}

	/* display path info for object editing also */
	if( state == ST_O_EDIT ) {
		bu_vls_strcat( vp, "** PATH --  ");
		for(i=0; i <= illump->s_last; i++) {
			bu_vls_strcat( vp, "/" );
			bu_vls_strcat( vp, illump->s_path[i]->d_namep);
		}
		bu_vls_strcat( vp, ": " );

		/* print the evaluated (path) solid parameters */
		if( illump->s_Eflag == 0 ) {
			mat_t	new_mat;
			/* NOT an evaluated region */
			/* object edit option selected */
			bn_mat_mul(new_mat, modelchanges, es_mat);

			vls_solid( vp, &es_int, new_mat );
		}
	}

	{
	  register char *start;
	  register char *p;
	  register int imax = 0;
	  register int i = 0;
	  register int j;
	  struct bu_vls vls;

	  start = bu_vls_addr( vp );
	  /*
	   * Some display managers don't handle TABs properly, so
	   * we replace any TABs with spaces. Also, look for the
	   * maximum line length.
	   */
	  for(p = start; *p != '\0'; ++p){
	    if(*p == '\t')
	      *p = ' ';
	    else if(*p == '\n'){
	      if(i > imax)
		imax = i;
	      i = 0;
	    }else
	      ++i;
	  }

	  if(i > imax)
	    imax = i;

	  /* Prep string for use with Tcl/Tk */
	  ++imax;
	  i = 0;
	  bu_vls_init(&vls);
	  for(p = start; *p != '\0'; ++p){
	    if(*p == '\n'){
	      for(j = 0; j < imax - i; ++j)
		bu_vls_putc(&vls, ' ');

	      bu_vls_putc(&vls, *p);
	      i = 0;
	    }else{
	      bu_vls_putc(&vls, *p);
	      ++i;
	    }
	  }

	  Tcl_SetVar(interp, bu_vls_addr(&edit_info_vls),
		     bu_vls_addr(&vls), TCL_GLOBAL_ONLY);
	  bu_vls_free(&vls);
	}
}

/*
 *			S C R E E N _ V L S
 *
 *  Output a vls string to the display manager,
 *  as a text overlay on the graphics area (ugh).
 *
 * Set up for character output.  For the best generality, we
 * don't assume that the display can process a CRLF sequence,
 * so each line is written with a separate call to DM_DRAW_STRING_2D().
 */
void
screen_vls( xbase, ybase, vp )
int	xbase;
int	ybase;
register struct bu_vls	*vp;
{
  register char	*start;
  register char	*end;
  register int	y;

  BU_CK_VLS( vp );
  y = ybase;

  start = bu_vls_addr( vp );
  while( *start != '\0' )  {
    if( (end = strchr( start, '\n' )) == NULL )  return;

    *end = '\0';
#if 0
    {
      register char *p;

      /* Some display managers don't handle TABs properly, so
	 we replace any TABs with spaces. */
      for(p = start; *p != '\0'; ++p)
	if(*p == '\t')
	  *p = ' ';
    }
#endif
    DM_DRAW_STRING_2D(dmp, start,
		      GED2PM1(xbase), GED2PM1(y), 0, 0);
    start = end+1;
    y += TEXT0_DY;
  }
}

/*
 *			D O T I T L E S
 *
 * Produce titles, etc, on the screen.
 * NOTE that this routine depends on being called AFTER dozoom();
 */
void
dotitles(overlay_vls)
struct bu_vls *overlay_vls;
{
	register int    i;
	register int    x, y;			/* for menu computations */
	static vect_t   work, work1;		/* work vector */
	static vect_t   temp, temp1;
	register int    yloc, xloc;
	auto fastf_t	az, el, tw;
	int		scroll_ybot;
	struct bu_vls   vls;
	typedef char    c_buf[80];
	auto c_buf      cent_x, cent_y, cent_z, size, ang_x, ang_y, ang_z;
	int             ss_line_not_drawn=1; /* true if the second status line has not been drawn */

	if(dbip == DBI_NULL)
	  return;

	bu_vls_init(&vls);
	
	/* Set the Tcl variables to the appropriate values. */

	if (illump != SOLID_NULL) {
	  struct bu_vls path_lhs, path_rhs;

	  bu_vls_init(&path_lhs);
	  bu_vls_init(&path_rhs);
	  for (i = 0; i < ipathpos; i++)
	    bu_vls_printf(&path_lhs, "/%s", illump->s_path[i]->d_namep);
	  for (; i <= illump->s_last; i++)
	    bu_vls_printf(&path_rhs, "/%s", illump->s_path[i]->d_namep);

	  bu_vls_printf(&vls, "%s(path_lhs)", MGED_DISPLAY_VAR);
	  Tcl_SetVar(interp, bu_vls_addr(&vls), bu_vls_addr(&path_lhs), TCL_GLOBAL_ONLY);
	  bu_vls_trunc(&vls, 0);
	  bu_vls_printf(&vls, "%s(path_rhs)", MGED_DISPLAY_VAR);
	  Tcl_SetVar(interp, bu_vls_addr(&vls), bu_vls_addr(&path_rhs), TCL_GLOBAL_ONLY);
	  bu_vls_free(&path_rhs);
	  bu_vls_free(&path_lhs);
	} else {
	  bu_vls_printf(&vls, "%s(path_lhs)", MGED_DISPLAY_VAR);
	  Tcl_SetVar(interp, bu_vls_addr(&vls), "", TCL_GLOBAL_ONLY);
	  bu_vls_trunc(&vls, 0);
	  bu_vls_printf(&vls, "%s(path_rhs)", MGED_DISPLAY_VAR);
	  Tcl_SetVar(interp, bu_vls_addr(&vls), "", TCL_GLOBAL_ONLY);
	}

	sprintf(cent_x, "%.3f", -toViewcenter[MDX]*base2local);
	sprintf(cent_y, "%.3f", -toViewcenter[MDY]*base2local);
	sprintf(cent_z, "%.3f", -toViewcenter[MDZ]*base2local);
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "cent=(%s %s %s)", cent_x, cent_y, cent_z);
	Tcl_SetVar(interp, bu_vls_addr(&center_name),
		   bu_vls_addr(&vls), TCL_GLOBAL_ONLY);

	sprintf(size, "sz=%.3f", VIEWSIZE*base2local);
	Tcl_SetVar(interp, bu_vls_addr(&size_name),
		    size, TCL_GLOBAL_ONLY);

	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "%s(units)", MGED_DISPLAY_VAR);
	Tcl_SetVar(interp, bu_vls_addr(&vls),
		   (char *)rt_units_string(dbip->dbi_local2base), TCL_GLOBAL_ONLY);

	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "az=%3.2f  el=%3.2f  tw=%3.2f",
		      curr_dm_list->s_info->azimuth,
		      curr_dm_list->s_info->elevation,
		      curr_dm_list->s_info->twist);
	Tcl_SetVar(interp, bu_vls_addr(&aet_name),
		   bu_vls_addr(&vls), TCL_GLOBAL_ONLY);

	sprintf(ang_x, "%.2f", rate_rotate[X]);
	sprintf(ang_y, "%.2f", rate_rotate[Y]);
	sprintf(ang_z, "%.2f", rate_rotate[Z]);

	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "ang=(%s %s %s)", ang_x, ang_y, ang_z);
	Tcl_SetVar(interp, bu_vls_addr(&ang_name),
		   bu_vls_addr(&vls), TCL_GLOBAL_ONLY);

	DM_SET_LINE_ATTR(dmp, mged_variables->linewidth, 0);

	/* Label the vertices of the edited solid */
	if(es_edflag >= 0 || (state == ST_O_EDIT && illump->s_Eflag == 0))  {
		mat_t			xform;
		struct rt_point_labels	pl[8+1];

		if( mged_variables->perspective <= 0)
		  bn_mat_mul( xform, model2objview, es_mat );
		else{
		  mat_t tmat;

		  bn_mat_mul( tmat, model2objview, es_mat );
		  bn_mat_mul( xform, perspective_mat, tmat );
		}

		label_edited_solid( pl, 8+1, xform, &es_int );

		DM_SET_COLOR(dmp, DM_YELLOW_R, DM_YELLOW_G, DM_YELLOW_B, 1);
		for( i=0; i<8+1; i++ )  {
			if( pl[i].str[0] == '\0' )  break;
			DM_DRAW_STRING_2D( dmp, pl[i].str,
					   GED2PM1(((int)(pl[i].pt[X]*2048))+15),
					   GED2PM1(((int)(pl[i].pt[Y]*2048))+15), 0, 1 );
		}
	}

if(mged_variables->faceplate){
	/* Line across the bottom, above two bottom status lines */
	DM_SET_COLOR(dmp, DM_YELLOW_R, DM_YELLOW_G, DM_YELLOW_B, 1);
	DM_DRAW_LINE_2D( dmp,
			 GED2PM1(XMIN), GED2PM1(TITLE_YBASE-TEXT1_DY),
			 GED2PM1(XMAX), GED2PM1(TITLE_YBASE-TEXT1_DY) );

	if(mged_variables->orig_gui){
	  /* Enclose window in decorative box.  Mostly for alignment. */
	  DM_DRAW_LINE_2D( dmp,
			   GED2PM1(XMIN), GED2PM1(YMIN),
			   GED2PM1(XMAX), GED2PM1(YMIN) );
	  DM_DRAW_LINE_2D( dmp,
			   GED2PM1(XMAX), GED2PM1(YMIN),
			   GED2PM1(XMAX), GED2PM1(YMAX) );
	  DM_DRAW_LINE_2D( dmp,
			   GED2PM1(XMAX), GED2PM1(YMAX),
			   GED2PM1(XMIN), GED2PM1(YMAX) );
	  DM_DRAW_LINE_2D( dmp,
			   GED2PM1(XMIN), GED2PM1(YMAX),
			   GED2PM1(XMIN), GED2PM1(YMIN) );

	  /* Display scroll bars */
	  scroll_ybot = scroll_display( SCROLLY ); 
	  y = MENUY;
	  x = MENUX;

	  /* Display state and local unit in upper right corner, boxed */
#define YPOS	(MENUY - MENU_DY - 75 )
	  DM_DRAW_LINE_2D(dmp,
			  GED2PM1(MENUXLIM), GED2PM1(YPOS),
			  GED2PM1(MENUXLIM), GED2PM1(YMAX));	/* vert. */
	  DM_SET_COLOR(dmp, DM_WHITE_R, DM_WHITE_G, DM_WHITE_B, 1);
	  DM_DRAW_STRING_2D(dmp, state_str[state],
			    GED2PM1(MENUX), GED2PM1(MENUY - MENU_DY), 1, 0 );
#undef YPOS
	}else{
	  scroll_ybot = SCROLLY;
	  x = XMIN + 20;
	  y = YMAX+TEXT0_DY;
	}

	/*
	 * Print information about object illuminated
	 */
	if( illump != SOLID_NULL &&
	    (state==ST_O_PATH || state==ST_O_PICK || state==ST_S_PICK) )  {
	  for( i=0; i <= illump->s_last; i++ )  {
	    if( i == ipathpos  &&  state == ST_O_PATH )  {
	      DM_SET_COLOR(dmp, DM_WHITE_R, DM_WHITE_G, DM_WHITE_B, 1);
	      DM_DRAW_STRING_2D( dmp, "[MATRIX]",
				 GED2PM1(x), GED2PM1(y), 0, 0 );
	      y += MENU_DY;
	    }
	    DM_SET_COLOR(dmp, DM_YELLOW_R, DM_YELLOW_G, DM_YELLOW_B, 1);
	    DM_DRAW_STRING_2D( dmp, illump->s_path[i]->d_namep,
			       GED2PM1(x), GED2PM1(y), 0, 0 );
	    y += MENU_DY;
	  }
	}

	if(mged_variables->orig_gui){
	  /*
	   * The top of the menu (if any) begins at the Y value specified.
	   */
	  mmenu_display( y );

	  /* print parameter locations on screen */
	  if( state == ST_O_EDIT && illump->s_Eflag ) {
		/* region is a processed region */
		MAT4X3PNT(temp, model2objview, es_keypoint);
		xloc = (int)(temp[X]*2048);
		yloc = (int)(temp[Y]*2048);
		DM_SET_COLOR(dmp, DM_YELLOW_R, DM_YELLOW_G, DM_YELLOW_B, 1);
		DM_DRAW_LINE_2D(dmp,
				GED2PM1(xloc-TEXT0_DY), GED2PM1(yloc+TEXT0_DY),
				GED2PM1(xloc+TEXT0_DY), GED2PM1(yloc-TEXT0_DY));
		DM_DRAW_LINE_2D(dmp,
				GED2PM1(xloc-TEXT0_DY), GED2PM1(yloc-TEXT0_DY),
				GED2PM1(xloc+TEXT0_DY), GED2PM1(yloc+TEXT0_DY));
		DM_DRAW_LINE_2D(dmp,
				GED2PM1(xloc+TEXT0_DY), GED2PM1(yloc+TEXT0_DY),
				GED2PM1(xloc-TEXT0_DY), GED2PM1(yloc+TEXT0_DY));
		DM_DRAW_LINE_2D(dmp,
				GED2PM1(xloc+TEXT0_DY), GED2PM1(yloc-TEXT0_DY),
				GED2PM1(xloc-TEXT0_DY), GED2PM1(yloc-TEXT0_DY));
		DM_DRAW_LINE_2D(dmp,
				GED2PM1(xloc+TEXT0_DY), GED2PM1(yloc+TEXT0_DY),
				GED2PM1(xloc+TEXT0_DY), GED2PM1(yloc-TEXT0_DY));
		DM_DRAW_LINE_2D(dmp,
				GED2PM1(xloc-TEXT0_DY), GED2PM1(yloc+TEXT0_DY),
				GED2PM1(xloc-TEXT0_DY), GED2PM1(yloc-TEXT0_DY));
	      }
	}

	/*
	 * Prepare the numerical display of the currently edited solid/object.
	 */
	/*	create_text_overlay( &vls ); */
	if(mged_variables->orig_gui){
	  screen_vls( SOLID_XBASE, scroll_ybot+TEXT0_DY, overlay_vls );
	}else{
	  screen_vls( x, y, overlay_vls );
	}

	/*
	 * General status information on the next to last line
	 */
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls,
		      " cent=(%s, %s, %s), %s %s, ", cent_x, cent_y, cent_z,
		      size, rt_units_string(dbip->dbi_local2base));
	bu_vls_printf(&vls,
		       "az=%3.2f el=%3.2f tw=%3.2f ang=(%s, %s, %s)",
		      curr_dm_list->s_info->azimuth,
		      curr_dm_list->s_info->elevation,
		      curr_dm_list->s_info->twist,
		      ang_x, ang_y, ang_z);
	DM_SET_COLOR(dmp, DM_WHITE_R, DM_WHITE_G, DM_WHITE_B, 1);
	DM_DRAW_STRING_2D( dmp, bu_vls_addr(&vls),
			   GED2PM1(TITLE_XBASE), GED2PM1(TITLE_YBASE), 1, 0 );
} /* if faceplate !0 */

	/*
	 * Second status line
	 */

	/* Priorities for what to display:
	 *	1.  adc info
	 *	2.  keypoint
	 *	3.  illuminated path
	 *
	 * This way the adc info will be displayed during editing
	 */

	if( mged_variables->adcflag ) {
		/* Angle/Distance cursor */
		point_t	pt1, pt2, pt3;
		point_t	center_model;

		VSET(pt1, 
		    (curs_x / 2047.0) *Viewscale,
		    (curs_y / 2047.0) *Viewscale, 0.0);
		VSET(center_model, 
		    -toViewcenter[MDX], -toViewcenter[MDY],
		    -toViewcenter[MDZ]);
		MAT4X3VEC(pt2, Viewrot, center_model);
		VADD2(pt3, pt1, pt2);
		bu_vls_trunc(&vls, 0);
		bu_vls_printf( &vls,
" curs:  a1=%.1f,  a2=%.1f,  dst=%.3f,  cent=(%.3f, %.3f),  delta=(%.3f, %.3f)",
			angle1 * radtodeg, angle2 * radtodeg,
			(c_tdist / 2047.0) *Viewscale*base2local,
			pt3[X]*base2local, pt3[Y]*base2local,
			(curs_x / 2047.0) *Viewscale*base2local,
			(curs_y / 2047.0) *Viewscale*base2local );
		if(mged_variables->faceplate){
		  DM_SET_COLOR(dmp, DM_YELLOW_R, DM_YELLOW_G, DM_YELLOW_B, 1);
		  DM_DRAW_STRING_2D( dmp, bu_vls_addr(&vls),
				     GED2PM1(TITLE_XBASE), GED2PM1(TITLE_YBASE + TEXT1_DY), 1, 0 );
		}
		Tcl_SetVar(interp, bu_vls_addr(&adc_name),
			    bu_vls_addr(&vls), TCL_GLOBAL_ONLY);
		ss_line_not_drawn = 0;
	}else{
	  Tcl_SetVar(interp, bu_vls_addr(&adc_name), "", TCL_GLOBAL_ONLY);
	}

	if( state == ST_S_EDIT || state == ST_O_EDIT )  {
	  struct bu_vls kp_vls;

	  bu_vls_init(&kp_vls);
	  bu_vls_printf( &kp_vls,
			 " Keypoint: %s %s: (%g, %g, %g)",
			 rt_functab[es_int.idb_type].ft_name+3,	/* Skip ID_ */
			 es_keytag,
			 es_keypoint[X] * base2local,
			 es_keypoint[Y] * base2local,
			 es_keypoint[Z] * base2local);
	  if(mged_variables->faceplate && ss_line_not_drawn){
	    DM_SET_COLOR(dmp, DM_YELLOW_R, DM_YELLOW_G, DM_YELLOW_B, 1);
	    DM_DRAW_STRING_2D( dmp, bu_vls_addr(&kp_vls),
			       GED2PM1(TITLE_XBASE), GED2PM1(TITLE_YBASE + TEXT1_DY), 1, 0 );
	    ss_line_not_drawn = 0;
	  }

	  bu_vls_trunc(&vls, 0);
	  bu_vls_printf(&vls, "%s(keypoint)", MGED_DISPLAY_VAR);
	  Tcl_SetVar(interp, bu_vls_addr(&vls), bu_vls_addr(&kp_vls), TCL_GLOBAL_ONLY);

	  bu_vls_free(&kp_vls);
	}else{
	  bu_vls_trunc(&vls, 0);
	  bu_vls_printf(&vls, "%s(keypoint)", MGED_DISPLAY_VAR);
	  Tcl_SetVar(interp, bu_vls_addr(&vls), "", TCL_GLOBAL_ONLY);
	}

	if( illump != SOLID_NULL )  {
	  if(mged_variables->faceplate && ss_line_not_drawn){
	    bu_vls_trunc(&vls, 0);

	    /* Illuminated path */
	    bu_vls_strcat(&vls, " Path: ");
	    for( i=0; i <= illump->s_last; i++ )  {
	      if( i == ipathpos  &&
		  (state == ST_O_PATH || state == ST_O_EDIT) )
		bu_vls_strcat( &vls, "/__MATRIX__" );
	      bu_vls_printf(&vls, "/%s", illump->s_path[i]->d_namep);
	    }
	    DM_SET_COLOR(dmp, DM_YELLOW_R, DM_YELLOW_G, DM_YELLOW_B, 1);
	    DM_DRAW_STRING_2D( dmp, bu_vls_addr(&vls),
			       GED2PM1(TITLE_XBASE), GED2PM1(TITLE_YBASE + TEXT1_DY), 1, 0 );

	    ss_line_not_drawn = 0;
	  }
	}

	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "%.2f fps", 1/frametime );
	if(mged_variables->faceplate && ss_line_not_drawn){
	  DM_SET_COLOR(dmp, DM_YELLOW_R, DM_YELLOW_G, DM_YELLOW_B, 1);
	  DM_DRAW_STRING_2D( dmp, bu_vls_addr(&vls),
			     GED2PM1(TITLE_XBASE), GED2PM1(TITLE_YBASE + TEXT1_DY), 1, 0 );
	}
	Tcl_SetVar(interp, bu_vls_addr(&fps_name),
		    bu_vls_addr(&vls), TCL_GLOBAL_ONLY);

	bu_vls_free(&vls);
}
