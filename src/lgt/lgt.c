/*                           L G T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file lgt/lgt.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <assert.h>
#include "bio.h"

#include "vmath.h"
#include "raytrace.h"
#include "fb.h"

#include "./hmenu.h"
#include "./lgt.h"
#include "./extern.h"
#include "./screen.h"

#ifndef SIGTSTP
#  define SIGTSTP 18
#endif
#ifndef SIGCLD
#  define SIGCLD SIGCHLD
#endif

#if !defined(NSIG)
#  define NSIG	64		/* conservative */
#endif

int	ready_Output_Device(int frame);
void	close_Output_Device(int frame);
static void	intr_sig(int sig);
static void	init_Lgts(void);
void		exit_Neatly(int status);
int		key_Frame(void);

/*	m a i n ( )							*/
int
main(int argc, char **argv)
{
    int	i;

    bu_setlinebuf(stderr);

    bu_log( "\n\nThis program is deprecated and will not be supported in future releases\n" );
    bu_log( "\tPlease use \"rtedge\" instead\n" );
    bu_log( "\tPlease notify \"devs@brlcad.org\" if you need enhancements to \"rtedge\"\n" );
    bu_log( "\nPress \"Enter\" to continue\n\n" );
    (void)getchar();
    npsw = bu_avail_cpus();
    if ( npsw > MAX_PSW )
	npsw = MAX_PSW;
    if ( npsw > 1 )
	rt_g.rtg_parallel = 1;
    else
	rt_g.rtg_parallel = 0;
    bu_semaphore_init( RT_SEM_LAST );

    init_Lgts();

    if ( ! pars_Argv( argc, argv ) )
    {
	prnt_Usage();
	return	1;
    }

    for ( i = 0; i < NSIG; i++ )
	switch ( i )
	{
	    case SIGINT :
		if ( (norml_sig = signal( i, SIG_IGN )) == SIG_IGN )
		{
		    if ( ! tty )
			abort_sig = SIG_IGN;
		    else
		    {
			/* MEX windows on IRIS (other than
			   the console) ignore SIGINT. */
			prnt_Scroll( "WARNING: Signal 1 was being ignored!" );
			goto	tty_sig;
		    }
		}
		else
		{
		tty_sig:
		    norml_sig = intr_sig;
		    abort_sig = abort_RT;
		    (void) signal( i,  norml_sig );
		}
		break;
	    case SIGCHLD :
		break; /* Leave SIGCHLD alone. */
	    case SIGPIPE :
		(void) signal( i, SIG_IGN );
		break;
	    case SIGQUIT :
		break;
	    case SIGTSTP :
		(void) signal( i, stop_sig );
		break;
	}
    /* Main loop.							*/
    user_Interaction();
    /*NOTREACHED*/
    return	99; /* Stupid UTX compiler considers this as reachable. */
}

/*	i n t e r p o l a t e _ F r a m e ( )				*/
int
interpolate_Frame(int frame)
{
    fastf_t	rel_frame = (fastf_t) frame / movie.m_noframes;
    if ( movie.m_noframes == 1 )
	return	1;
    if ( ! movie.m_fullscreen )
    {
	int	frames_across;
	int	size;
	size = MovieSize( movie.m_frame_sz, movie.m_noframes );
	frames_across = size / movie.m_frame_sz;
	x_fb_origin = (frame % frames_across) * movie.m_frame_sz;
	y_fb_origin = (frame / frames_across) * movie.m_frame_sz;
    }
    bu_log( "Frame %d:\n", frame );
    if ( movie.m_keys )
	return	key_Frame() == -1 ? 0 : 1;
    lgts[0].azim = movie.m_azim_beg +
	rel_frame * (movie.m_azim_end - movie.m_azim_beg);
    lgts[0].elev = movie.m_elev_beg +
	rel_frame * (movie.m_elev_end - movie.m_elev_beg);
    grid_roll = movie.m_roll_beg +
	rel_frame * (movie.m_roll_end - movie.m_roll_beg);
    bu_log( "\tview azimuth\t%g\n", lgts[0].azim*RAD2DEG );
    bu_log( "\tview elevation\t%g\n", lgts[0].elev*RAD2DEG );
    bu_log( "\tview roll\t%g\n", grid_roll*RAD2DEG );
    if ( movie.m_over )
    {
	lgts[0].over = 1;
	lgts[0].dist = movie.m_dist_beg +
	    rel_frame * (movie.m_dist_end - movie.m_dist_beg);
	grid_dist = movie.m_grid_beg +
	    rel_frame * (movie.m_grid_end - movie.m_grid_beg);
	bu_log( "\teye distance\t%g\n", lgts[0].dist );
	bu_log( "\tgrid distance\t%g\n", grid_dist );
    }
    else
    {
	lgts[0].over = 0;
	if ( ZERO(movie.m_pers_beg) && ZERO(movie.m_pers_end) )
	{
	    rel_perspective = 0.0;
	    grid_dist = movie.m_grid_beg +
		rel_frame * (movie.m_grid_end - movie.m_grid_beg);
	    bu_log( "\tgrid distance\t%g\n", grid_dist );
	}
	else
	    if ( movie.m_pers_beg >= 0.0 )
		rel_perspective = movie.m_pers_beg +
		    rel_frame * (movie.m_pers_end - movie.m_pers_beg);
	bu_log( "\tperspective\t%g\n", rel_perspective );
    }
    return	1;
}

/*	e x i t _ N e a t l y ( )					*/
void
exit_Neatly(int status)
{
    prnt_Event( "Quitting...\n" );
    bu_exit( status, NULL );
}

/*	r e a d y _ O u t p u t _ D e v i c e ( )			*/
int
ready_Output_Device(int frame)
{
    int size;
    if ( force_cellsz )
    {
	grid_sz = (int)(view_size / cell_sz);
	V_MAX( grid_sz, 1 ); /* must be non-zero */
	setGridSize( grid_sz );
	prnt_Status();
    }
    /* Calculate size of frame buffer image (pixels across square image). */
    if ( movie.m_noframes > 1 && ! movie.m_fullscreen )
	/* Fit frames of movie. */
	size = MovieSize( grid_sz, movie.m_noframes );
    else
	if ( force_fbsz && ! DiskFile(fb_file) )
	    size = fb_size; /* user-specified size */
	else
	    size = grid_sz; /* just 1 pixel/ray */
    if ( movie.m_noframes > 1 && movie.m_fullscreen )
    {
	char	framefile[MAX_LN];
	/* We must be doing full-screen frames. */
	size = grid_sz;
	(void) snprintf( framefile, MAX_LN, "%s.%04d", prefix, frame );
	if ( ! fb_Setup( framefile, size ) )
	    return	0;
    }
    else
    {
	if ( frame == movie.m_curframe && ! fb_Setup( fb_file, size ) )
	    return	0;
	fb_Zoom_Window();
    }
    return	1;
}

/*	c l o s e _ O u t p u t _ D e v i c e ( )			*/
void
close_Output_Device(int frame)
{
    if ((movie.m_noframes > 1 && movie.m_fullscreen) ||
	(frame == movie.m_endframe))
    {
	(void) fb_close( fbiop );
	fbiop = FBIO_NULL;
    }
    return;
}

static void
intr_sig(int sig)
{
    sig = sig;
    (void) signal( SIGINT, intr_sig );
    return;
}

/*	i n i t _ L g t s ( )
	Set certain default lighting info.
*/
static void
init_Lgts(void)
{
    /* Ambient lighting.						*/
    bu_strlcpy( lgts[0].name, "EYE", sizeof(lgts[0].name));
    lgts[0].beam = 0;
    lgts[0].over = 0;
    lgts[0].rgb[0] = 255;
    lgts[0].rgb[1] = 255;
    lgts[0].rgb[2] = 255;
    lgts[0].azim = 30.0/RAD2DEG;
    lgts[0].elev = 30.0/RAD2DEG;
    lgts[0].dist = 0.0;
    lgts[0].energy = 0.4;
    lgts[0].stp = SOLTAB_NULL;

    /* Primary lighting.						*/
    bu_strlcpy( lgts[1].name, "LIGHT", sizeof(lgts[1].name) );
    lgts[1].beam = 0;
    lgts[1].over = 1;
    lgts[1].rgb[0] = 255;
    lgts[1].rgb[1] = 255;
    lgts[1].rgb[2] = 255;
    lgts[1].azim = 60.0/RAD2DEG;
    lgts[1].elev = 60.0/RAD2DEG;
    lgts[1].dist = 10000.0;
    lgts[1].energy = 1.0;
    lgts[1].stp = SOLTAB_NULL;

    lgt_db_size = 2;
    return;
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
