/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "./vecmath.h"
#include "./lgt.h"
#include "./screen.h"
#include "./extern.h"
#if defined( CRAY )
#include <sys/category.h>
#include <sys/resource.h>
#include <sys/types.h>
#if defined( CRAY2 )
#undef MAXINT
#include <sys/param.h>
#endif
#define MAX_CPU_TICKS	(200000*HZ) /* Max ticks = seconds * ticks/sec.	*/
#define NICENESS	10
#endif
int	ready_Output_Device();
void	close_Output_Device();
#if defined( BSD ) || (defined( SYSV ) && ! defined( mips ))
_LOCAL_ int	intr_sig();
int		(*norml_sig)(), (*abort_sig)();
extern int	stop_sig();
#else
_LOCAL_ void	intr_sig();
void		(*norml_sig)(), (*abort_sig)();
extern void	stop_sig();
#endif
_LOCAL_ void	init_Lgts();
void		exit_Neatly();

_LOCAL_ int
substr( str, pattern )
char	*str, *pattern;
	{
	if( *str == '\0' )
		return	FALSE;
	if( *str != *pattern || strncmp( str, pattern, strlen( pattern ) ) )
		return	substr( str+1, pattern );
	return	TRUE;
	}

/*	m a i n ( )							*/
main( argc, argv )
char	*argv[];
{	register int	i;
#if ! defined( BSD ) && ! defined( sgi ) && ! defined( CRAY2 )
	(void) setvbuf( stderr, (char *) NULL, _IOLBF, BUFSIZ );
#endif
	beginptr = sbrk(0);

	if( npsw > 1 )
		rt_g.rtg_parallel = 1;
	else
		rt_g.rtg_parallel = 0;
	RES_INIT( &rt_g.res_syscall );
	RES_INIT( &rt_g.res_worker );
	RES_INIT( &rt_g.res_stats );
	RES_INIT( &rt_g.res_results );

#if defined( CRAY )
	nicem( C_PROC, 0, NICENESS );
	rt_log( "Program niced to %d.\n", NICENESS );
	limit( C_PROC, 0, L_CPU, MAX_CPU_TICKS );
	rt_log(	"CPU limit set to %d ticks (at %d/sec that's %d seconds).\n",
		MAX_CPU_TICKS,
		HZ,
		MAX_CPU_TICKS/HZ
		);
#endif
	
	init_Lgts();

	if( ! pars_Argv( argc, argv ) )
		{
		prnt_Usage();
		return	1;
		}

#ifdef sgi
	if( ismex() && tty )
		{
		sgi_console = substr( getenv( "TERM" ), "iris" );
		(void) sprintf( prompt,
				"Do you want to use the IRIS mouse ? [y|n](%c) ",
				sgi_usemouse ? 'y' : 'n'
				);
		if( get_Input( input_ln, MAX_LN, prompt ) != NULL )
			sgi_usemouse = input_ln[0] != 'n';
		if( sgi_usemouse )			
			sgi_Init_Popup_Menu();
		}
#endif
	for( i = 0; i < NSIG; i++ )
		switch( i )
			{
		case SIGINT :
			if( (norml_sig = signal( i, SIG_IGN )) == SIG_IGN )
				{
				if( ! tty )
					abort_sig = SIG_IGN;
				else
					{ /* MEX windows on IRIS (other than
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
#if defined( BSD )
		case SIGCHLD :
#else
		case SIGCLD :
#endif
			break; /* Leave SIGCLD alone.			*/
		case SIGPIPE :
			(void) signal( i, SIG_IGN );
			break;
		case SIGQUIT :
			break;
#if ! defined( SYSV )
#if ! defined( SIGTSTP )
#define SIGTSTP	18
#endif
		case SIGTSTP :
			(void) signal( i, stop_sig );
			break;
#endif
			}
	/* Main loop.							*/
	user_Interaction();
	/*NOTREACHED*/
	return	99; /* Stupid UTX compiler considers this as reachable. */
	}

/*	i n t e r p o l a t e _ F r a m e ( )				*/
int
interpolate_Frame( frame )
int	frame;
	{	fastf_t	rel_frame = (fastf_t) frame / movie.m_noframes;
	if( movie.m_noframes == 1 )
		return	TRUE;
	if( ! movie.m_fullscreen )
		{	register int	frames_across;
			register int	size;
		size = (int) sqrt( (double) movie.m_noframes + 0.5 ) * movie.m_frame_sz;
		frames_across = size / movie.m_frame_sz;
		x_fb_origin = (frame % frames_across) * movie.m_frame_sz;
		y_fb_origin = (frame / frames_across) * movie.m_frame_sz;
		}
	rt_log( "Frame %d:\n", frame );
	if( movie.m_keys )
		return	key_Frame() == -1 ? FALSE : TRUE;
	lgts[0].azim = movie.m_azim_beg +
				rel_frame * (movie.m_azim_end - movie.m_azim_beg);
	lgts[0].elev = movie.m_elev_beg +
				rel_frame * (movie.m_elev_end - movie.m_elev_beg);
	grid_roll = movie.m_roll_beg +
				rel_frame * (movie.m_roll_end - movie.m_roll_beg);
	if( movie.m_over )
		{
		lgts[0].over = TRUE;
		lgts[0].dist = movie.m_dist_beg +
				rel_frame * (movie.m_dist_end - movie.m_dist_beg);
		grid_dist = movie.m_grid_beg +
				rel_frame * (movie.m_grid_end - movie.m_grid_beg);
		}
	else
		{
		lgts[0].over = FALSE;
		if( movie.m_pers_beg >= 0.0 )
			rel_perspective = movie.m_pers_beg +
			rel_frame * (movie.m_pers_end - movie.m_pers_beg);
		}
	rt_log( "\tview azimuth\t%g\n", lgts[0].azim*DEGRAD );
	rt_log( "\tview elevation\t%g\n", lgts[0].elev*DEGRAD );
	rt_log( "\tview roll\t%g\n", grid_roll*DEGRAD );
	if( movie.m_over )
		{
		rt_log( "\teye distance\t%g\n", lgts[0].dist );
		rt_log( "\tgrid distance\t%g\n", grid_dist );
		}
	else
		rt_log( "\tperspective\t%g\n", rel_perspective );
	return	TRUE;
	}

/*	e x i t _ N e a t l y ( )					*/
void
exit_Neatly( status )
int	status;
	{
	prnt_Event( "Quitting...\n" );
	exit( status );
	}

/*	r e a d y _ O u t p u t _ D e v i c e ( )			*/
int
ready_Output_Device( frame )
int	frame;
	{	int	size =
		(int) sqrt( (double) movie.m_noframes + 0.5 ) * grid_sz;
	if( movie.m_noframes > 1 && movie.m_fullscreen )
		{	char	frame_file[MAX_LN];
		/* We must be doing full-screen frames. */
		(void) sprintf( frame_file, "%s.%04d", fb_file, frame );
		if( ! fb_Setup( frame_file, grid_sz ) )
			return	0;
		}
	else
		{
		if( ! fb_Setup( fb_file, size ) )
			return	0;
		fb_Zoom_Window();
		}
	return	1;
	}

/*	c l o s e _ O u t p u t _ D e v i c e ( )			*/
void
close_Output_Device()
	{
	if( strncmp( fbiop->if_name, "/dev/sgi", 8 ) != 0 )
		(void) fb_close( fbiop );
	return;
	}

#if defined( BSD ) || (defined( SYSV ) && ! defined( mips ))
_LOCAL_ int
#else
_LOCAL_ void
#endif
/*ARGSUSED*/
intr_sig( sig )
int	sig;
	{
	(void) signal( SIGINT, intr_sig );
#if defined( BSD )
	return	sig;
#else
	return;
#endif
	}

/*	i n i t _ L g t s ( )
	Set certain default lighting info.
 */
_LOCAL_ void
init_Lgts()
	{
	/* Ambient lighting.						*/
	(void) strcpy( lgts[0].name, "EYE" );
	lgts[0].beam = FALSE;
	lgts[0].over = FALSE;
	lgts[0].rgb[0] = 255;
	lgts[0].rgb[1] = 255;
	lgts[0].rgb[2] = 255;
	lgts[0].azim = 30.0/DEGRAD;
	lgts[0].elev = 30.0/DEGRAD;
	lgts[0].dist = 0.0;
	lgts[0].energy = 0.4;
	lgts[0].stp = SOLTAB_NULL;

	/* Primary lighting.						*/
	(void) strcpy( lgts[1].name, "LIGHT" );
	lgts[1].beam = FALSE;
	lgts[1].over = TRUE;
	lgts[1].rgb[0] = 255;
	lgts[1].rgb[1] = 255;
	lgts[1].rgb[2] = 255;
	lgts[1].azim = 60.0/DEGRAD;
	lgts[1].elev = 60.0/DEGRAD;
	lgts[1].dist = 10000.0;
	lgts[1].energy = 1.0;
	lgts[0].stp = SOLTAB_NULL;

	lgt_db_size = 2;
	return;
	}
