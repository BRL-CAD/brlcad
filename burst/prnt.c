/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or AV-298-6651
*/
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <Sc/Sc.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./vecmath.h"
#include "./ascii.h"
#include "./extern.h"
#define MAX_COLS	128

#define PHANTOM_ARMOR	111

int
doMore( linesp )
int	*linesp;
	{	register int	ret = true;
	if( ! tty )
		return	true;
	TcSaveTty( HmTtyFd );
	TcSetRaw( HmTtyFd );
	TcClrEcho( HmTtyFd );
	ScSetStandout();
	prompt( "-- More -- " );
	ScClrStandout();
	(void) fflush( stdout );
	switch( HmGetchar() )
		{
	case 'q' :
	case 'n' :
		ret = false;
		break;
	case LF :
	case CR :
		*linesp = 1;
		break;
	default :
		*linesp = (PROMPT_Y-SCROLL_TOP);
		break;
		}
	TcResetTty( HmTtyFd );
	return	ret;
	}

_LOCAL_ int
f_Nerror( ap )
struct application	*ap;
	{
	rt_log( "Couldn't compute thickness or exit point along normal direction.\n" );
	V_Print( "\tpnt", ap->a_ray.r_pt, rt_log );
	V_Print( "\tdir", ap->a_ray.r_dir, rt_log );
	ap->a_rbeam = 0.0;
	VSET( ap->a_vvec, 0.0, 0.0, 0.0 );
	return	0;
	}

/*	f_Normal()

	Shooting from surface of object along reversed entry normal to
	compute exit point along normal direction and normal thickness.
	Exit point returned in "a_vvec", thickness returned in "a_rbeam".
 */
_LOCAL_ int
f_Normal( ap, pt_headp )
struct application	*ap;
struct partition	*pt_headp;
	{	register struct partition	*pp = pt_headp->pt_forw;
		register struct partition	*cp;
		register struct xray		*rp = &ap->a_ray;
		register struct hit		*ohitp;
	for(	cp = pp->pt_forw;
		cp != pt_headp && SameCmp( pp->pt_regionp, cp->pt_regionp );
		cp = cp->pt_forw
		)
		;
	ohitp = cp->pt_back->pt_outhit;
	ap->a_rbeam = ohitp->hit_dist - pp->pt_inhit->hit_dist;
	VJOIN1( ap->a_vvec, rp->r_pt, ohitp->hit_dist, rp->r_dir );
	return	1;
	}

#include <errno.h>
/* These aren't defined in BSD errno.h.					*/
extern int	errno;
extern int	sys_nerr;
extern char	*sys_errlist[];

void
locPerror( msg )
char    *msg;
	{
	if( errno > 0 && errno < sys_nerr )
		rt_log( "%s: %s\n", msg, sys_errlist[errno] );
	else
		rt_log( "BUG: errno not set, shouldn't call perror.\n" );
	return;
	}

int
notify( str, mode )
char    *str;
int	mode;
	{       register int    i;
		static int      lastlen = -1;
		register int    len;
		static char	buf[LNBUFSZ] = { 0 };
		register char	*p;
	if( ! tty )
		return	false;
	switch( mode )
		{
	case NOTIFY_APPEND :
		p = buf + lastlen;
		break;
	case NOTIFY_DELETE :
		for( p = buf+lastlen; p > buf && *p != NOTIFY_DELIM; p-- )
			;
		break;
	case NOTIFY_ERASE :
		p = buf;
		break;
		}
	if( str != NULL )
		{
		if( p > buf )
			*p++ = NOTIFY_DELIM;
		(void) strcpy( p, str );
		}
	else
		*p = NUL;
	(void) ScMvCursor( PROMPT_X, PROMPT_Y );
	len = strlen( buf );
	if( len > 0 )
		{
		(void) ScSetStandout();
		(void) fputs( buf, stdout );
		(void) ScClrStandout();
		}

	/* Blank out remainder of previous command. */
	for( i = len; i < lastlen; i++ )
		(void) putchar( ' ' );
	(void) ScMvCursor( PROMPT_X, PROMPT_Y );
	(void) fflush( stdout );
	lastlen = len;
	return	true;
	}

void
prntAspectInit()
	{
	if( outfile[0] == NUL )
		return;
	if(	fprintf( outfp,
			"%c % 7.2f% 7.2f% 9.3f% 9.3f% 9.3f% 7.1f% 9.6f\n",
			P_ASPECT_INIT,
			viewazim*DEGRAD,
			viewelev*DEGRAD,
			modlcntr[X]*unitconv,
			modlcntr[Y]*unitconv,
			modlcntr[Z]*unitconv,
			cellsz*unitconv,
			raysolidangle
			) < 0
		)
		{
		rt_log( "prntAspectInit: Write failed to data file!\n" );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	return;
	}

/*	prntBurstHdr()

	Report info about burst point.
 */
void
prntBurstHdr( ap, cpp, app, bpt )
struct application	*ap;
struct partition	*cpp, *app;
fastf_t			*bpt;
	{	FILE			*fp = outfp;
	if( outfile[0] == NUL )
		return;
	if(	fprintf( fp,
			"%c % 8.2f% 8.2f% 8.2f% 8.2f% 8.2f% 8.2f% 8.2f% 8.2f% 8.2f\n",
			P_BURST_HEADER,
			/* entry point to spalling component */
			cpp->pt_inhit->hit_point[X]*unitconv,
			cpp->pt_inhit->hit_point[Y]*unitconv,
			cpp->pt_inhit->hit_point[Z]*unitconv,
			bpt[X]*unitconv, /* burst point */
			bpt[Y]*unitconv,
			bpt[Z]*unitconv,
			/* exit point along normal of component */
			ap->a_vvec[X]*unitconv,
			ap->a_vvec[Y]*unitconv,
			ap->a_vvec[Z]*unitconv
			) < 0
		)
		{
		rt_log( "prntBurst_Header: Write failed to data file!\n" );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	return;
	}

void
prntCellIdent( ap, pt_headp )
register struct application	*ap;
struct partition		*pt_headp;
	{	FILE	*fp = outfp;
	if( outfile[0] == NUL )
		return;
	if( fprintf(	fp,
			"%c % 8.1f% 8.1f%3d% 8.2f% 8.2f% 9.3f% 9.3f\n",
			P_CELL_IDENT,
			ap->a_x*cellsz*unitconv,
				/* Horizontal center of cell.	*/
			ap->a_y*cellsz*unitconv,
				/* Vertical center of cell.	*/
			ap->a_user, /* Two-digit random number.	*/
			(standoff-pt_headp->pt_forw->pt_inhit->hit_dist)*
				unitconv,
			 /* Distance 1st contact to grid plane.	*/
			(standoff-pt_headp->pt_back->pt_outhit->hit_dist)*
				unitconv,
			 /* Distance last contact to grid plane.*/
			ap->a_uvec[X]*unitconv,
			 /* Actual horizontal coord. in cell.	*/
			ap->a_uvec[Y]*unitconv
			 /* Actual vertical coordinate in cell.	*/
			) < 0
		)
		{
		rt_log( "prntCellIdent: Write failed to data file!\n" );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	return;
	}

void
prntColors( colorp, str )
register Colors	*colorp;
char	*str;
	{
	rt_log( "%s:\n", str );
	for( colorp = colorp->c_next ; colorp != COLORS_NULL; colorp = colorp->c_next )
		{
		rt_log( "\t%d..%d\t%d,%d,%d\n",
			(int)colorp->c_lower,
			(int)colorp->c_upper,
			(int)colorp->c_rgb[0],
			(int)colorp->c_rgb[1],
			(int)colorp->c_rgb[2]
			);
		}
	return;
	}

/*
	void prntFiringCoords( register fastf_t *vec )

	If the user has asked for grid coordinates to be saved, write
	them to the output stream 'gridfp'.
 */
void
prntFiringCoords( vec )
register fastf_t *vec;
	{
	if( gridfile[0] == '\0' )
		return;
	assert( gridfp != (FILE *) NULL );
	if( fprintf( gridfp, "%7.2f %7.2f\n", vec[X]*unitconv, vec[Y]*unitconv )
		< 0 )
		{
		rt_log( "prntFiringCoords: Write failed to data file!\n" );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	return;
	}

void
prntGridOffsets( x, y )
int	x, y;
	{
	if( ! tty )
		return;
	(void) ScMvCursor( GRID_X, GRID_Y );
	(void) printf( "[% 4d:% 4d,% 4d:% 4d]",
			x, gridxfin, y, gridyfin
			);
	(void) fflush( stdout );
	return;
	}

void
prntHeaderInit()
	{	long	clock;
		char	*date;
	if( outfile[0] == NUL )
		return;
	clock = time( (long *) 0 );
	date = strtok( asctime( localtime( &clock ) ), "\n" );
	if(	fprintf( outfp, "%c %-26s    %s\n",
			P_HEADER_INIT,
			date,
			title
			) < 0
		)
		{
		rt_log( "prntHeaderInit: Write failed to data file!\n" );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	return;
	}

void
prntIdents( idp, str )
register Ids	*idp;
char	*str;
	{
	rt_log( "%s:\n", str );
	for( idp = idp->i_next ; idp != IDS_NULL; idp = idp->i_next )
		{
		if( idp->i_lower == idp->i_upper )
			rt_log( "\t%d\n", (int) idp->i_lower );
		else
			rt_log( "\t%d..%d\n",
				(int)idp->i_lower,
				(int)idp->i_upper
				);
		}
	return;
	}

/**/
void
prntPagedMenu( menu )
register char	**menu;
	{	register int	done = false;
		int		lines =	(PROMPT_Y-SCROLL_TOP);
	if( ! tty )
		{
		for( ; *menu != NULL; menu++ )
			rt_log( "%s\n", *menu );
		return;
		}
	for( ; *menu != NULL && ! done;  )
		{
		for( ; lines > 0 && *menu != NULL; menu++, --lines )
			rt_log( "%-*s\n", co, *menu );
		if( *menu != NULL )
			done = ! doMore( &lines );
		prompt( "" );
		}
	(void) fflush( stdout );
	return;
	}

/*	prntPhantom()

	Output "phantom armor" pseudo component.
 */
void
prntPhantom( ap, space, los, ct )
struct application		*ap;
int				space;/* Space code behind phantom.	*/
fastf_t				los;  /* LOS of space behind phantom.	*/
int				ct;   /* Count of components hit.	*/
	{	FILE	*fp = outfp;
	if( outfile[0] == NUL )
		return;
	if(	fprintf( fp,
			"%c %4d% 8.2f% 8.2f% 6.1f%3d% 8.2f%5d\n",
			P_RAY_INTERSECT,
			PHANTOM_ARMOR,	/* Special item code.		*/
			0.0,		/* LOS through item.		*/
			0.0,		/* Normal at entrance.		*/
			0.0,		/* Obliquity.			*/
			space,		/* Space code.			*/
			los*unitconv,	/* LOS through space.		*/
			ct		/* Cumulative count of items.	*/
			) < 0
		)
		{
		rt_log( "prntPhantom: Write failed to data file!\n" );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	return;
	}

void
prntRegionHdr( ap, hitp, regp, nbar )
register struct application	*ap;
register struct hit		*hitp;
struct region			*regp;
int				nbar;
	{	FILE	*fp = outfp;
		fastf_t	sqrconvdist;
	if( outfile[0] == NUL )
		return;
	sqrconvdist = hitp->hit_dist * unitconv;
	sqrconvdist = Sqr( sqrconvdist );
	RES_ACQUIRE( &rt_g.res_syscall );		/* lock */
	if(	fprintf( fp,
			"%c %4d% 10.3f% 10.3f% 10.3f% 10.3f%5d%6d\n",
			P_REGION_HEADER,
			regp->reg_regionid,
			hitp->hit_point[X]*unitconv,
			hitp->hit_point[Y]*unitconv,
			hitp->hit_point[Z]*unitconv,
			raysolidangle*sqrconvdist,
			nbar,
			ap->a_user /* Ray number.			*/
			) < 0
		)
		{
		RES_RELEASE( &rt_g.res_syscall );	/* unlock */
		rt_log( "prntRegion_Header: Write failed to data file!\n" );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	RES_RELEASE( &rt_g.res_syscall );	/* unlock */
	return;
	}

#include <varargs.h>
/* VARARGS */
void
prntScr( fmt, va_alist )
char	*fmt;
va_dcl
	{	va_list		ap;
	va_start( ap );
	if( tty )
		{
		TcClrTabs( HmTtyFd );
		if( ScSetScrlReg( SCROLL_TOP, SCROLL_BTM ) )
			{
			(void) ScMvCursor( 1, SCROLL_BTM );
			(void) ScClrEOL();
			(void) _doprnt( fmt, ap, stdout );
			(void) ScClrScrlReg();
			}
		else
		if( ScDL != NULL )
			{
			(void) ScMvCursor( 1, SCROLL_TOP );
			(void) ScDeleteLn();
			(void) ScMvCursor( 1, SCROLL_BTM );
			(void) ScClrEOL();
			(void) _doprnt( fmt, ap, stdout );
			}
		else
			{
			(void) _doprnt( fmt, ap, stdout );
			(void) fputs( "\n", stdout );
			}
		(void) fflush( stdout );
		}
	else
		{
		(void) _doprnt( fmt, ap, stderr );
		(void) fputs( "\n", stderr );
		}
	va_end( ap );
	return;
	}

/*	prntSeg()

	Report info about each component hit along path of main
	penetrator.
 */
void
prntSeg( ap, cpp, burst, space, los, ct )
register struct application	*ap;
register struct partition	*cpp; /* Component partition.		*/
int				burst;/* Boolean - is this a burst pt.	*/
int				space;
fastf_t				los;
int				ct;   /* Count of components hit.	*/
	{	FILE			*fp = outfp;
		fastf_t			cosobliquity;
		register struct hit	*ihitp;
		register struct soltab	*stp;
		struct application	a_thick;

	/* Fill in hit point and normal.				*/
	stp = cpp->pt_inseg->seg_stp;
	ihitp = cpp->pt_inhit;
	RT_HIT_NORM( ihitp, stp, &(ap->a_ray) );
	
	/* Check for flipped normal and fix.				*/
	if( cpp->pt_inflip )
		{
		ScaleVec( ihitp->hit_normal, -1.0 );
		cpp->pt_inflip = 0;
		}
	cosobliquity = Dot( ap->a_ray.r_dir, ihitp->hit_normal );
	if( cosobliquity < 0.0 )
		cosobliquity = -cosobliquity;
#ifdef DEBUG
	else
		rt_log( "Entry normal backwards.\n" );
#endif

	if( outfile[0] == NUL )
		return;

	if( burst )
		{
		/* Now we must find normal thickness through component.	*/
		a_thick = *ap;
		a_thick.a_hit = f_Normal;
		a_thick.a_miss = f_Nerror;
		a_thick.a_level++;
		CopyVec( a_thick.a_ray.r_pt, ihitp->hit_point );
		Scale2Vec( ihitp->hit_normal, -1.0, a_thick.a_ray.r_dir );
		if( rt_shootray( &a_thick ) == -1 && fatalerror )
			{
			/* Fatal error in application routine.	*/
			rt_log( "Fatal error: raytracing aborted.\n" );
			return;
			}
		CopyVec( ap->a_vvec,  a_thick.a_vvec );
		}
	else
		a_thick.a_rbeam = 0.0;
	if(	fprintf( fp,
			"%c %4d% 8.2f% 8.2f% 6.1f%3d% 8.2f%5d\n",
			P_RAY_INTERSECT,
			cpp->pt_regionp->reg_regionid,
				/* Item code.				*/
			(cpp->pt_outhit->hit_dist-ihitp->hit_dist)*unitconv,
				/* LOS through item.			*/
			a_thick.a_rbeam*unitconv, /* Normal thickness.	*/
			AproxEq( cosobliquity, 1.0, 0.01 ) ? 0.0 :
				acos( cosobliquity )*DEGRAD,
				/* Obliquity angle at entrance.		*/
			space,		/* Space code.			*/
			los*unitconv,	/* LOS through space.		*/
			ct		/* Cumulative count of items.	*/
			) < 0
		)
		{
		rt_log( "prntSeg: Write failed to data file!\n" );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	return;
	}

void
prntShieldComp( ap, rayp, qp )
struct application	*ap;
register struct xray	*rayp;
register Pt_Queue	*qp;
	{	FILE			*fp = outfp;
		fastf_t			cosobliquity;
		register struct hit	*ihitp;
		register struct soltab	*stp;
	if( outfile[0] == NUL )
		return;
	if( qp == PT_Q_NULL )
		return;
	prntShieldComp( ap, rayp, qp->q_next );

	/* Fill in hit point and normal.				*/
	stp = qp->q_part->pt_inseg->seg_stp;
	ihitp = qp->q_part->pt_inhit;
	RT_HIT_NORM( ihitp, stp, rayp );
	
	/* Check for flipped normal and fix.				*/
	if( qp->q_part->pt_inflip )
		{
		ScaleVec( ihitp->hit_normal, -1.0 );
		qp->q_part->pt_inflip = 0;
		}
	/* This SHOULD give negative of desired result, but make sure.	*/
	cosobliquity = Dot( rayp->r_dir, ihitp->hit_normal );
	if( cosobliquity < 0.0 )
		cosobliquity = -cosobliquity;
#ifdef DEBUG
	else
		rt_log( "Entry normal backwards.\n" );
#endif
	RES_ACQUIRE( &rt_g.res_syscall );		/* lock */
	if(	fprintf( fp,
			"%c %4d% 8.2f% 6.1f%4d\n",
			P_SHIELD_COMP,
			qp->q_part->pt_regionp->reg_regionid,
				/* Region ident of shielding component.	*/
			(qp->q_part->pt_outhit->hit_dist-ihitp->hit_dist)*unitconv,
				/* Line-of-sight thickness of shield.	*/
			AproxEq( cosobliquity, 1.0, 0.01 ) ? 0.0 :
				acos( cosobliquity )*DEGRAD,
				/* Obliquity angle of spall ray.	*/
			qp->q_space
			) < 0
		)
		{
		RES_RELEASE( &rt_g.res_syscall );	/* unlock */
		rt_log( "prntShieldComp: Write failed to data file!\n" );
		locPerror( "fprintf" );
		exitCleanly( 1 );
		}
	RES_RELEASE( &rt_g.res_syscall );	/* unlock */
	return;
	}

/*
	void	prntTimer( char *str )
 */
void
prntTimer( str )
char    *str;
	{
	(void) rt_read_timer( timer, TIMER_LEN-1 );
	if( tty )
		{
		(void) ScMvCursor( TIMER_X, TIMER_Y );
		if( str == NULL )
			(void) printf( "%s", timer );
		else
			(void) printf( "%s:\t%s", str, timer );
		(void) ScClrEOL();
		(void) fflush( stdout );
		}
	else
		rt_log( "%s:\t%s\n", str == NULL ? "(null)" : str, timer );
	return;
	}

void
prntTitle( title )
char	*title;
	{
	if( ! tty || rt_g.debug )
		rt_log( "%s\n", title == NULL ? "(null)" : title );
	return;
	}

static char	*usage[] =
	{
	"Usage: burst [-b]",
	"\tThe -b option suppresses the screen display (for batch jobs).",
	NULL
	};
void
prntUsage()
	{	register char   **p = usage;
	while( *p != NULL )
		(void) fprintf( stderr, "%s\n", *p++ );
	return;
	}

void
prompt( str )
char    *str;
	{
	(void) ScMvCursor( PROMPT_X, PROMPT_Y );
	if( str == (char *) NULL )
		(void) ScClrEOL();
	else
		{
		(void) ScSetStandout();
		(void) fputs( str, stdout );
		(void) ScClrStandout();
		}
	(void) fflush( stdout );
	return;
	}

int
qAdd( pp, qpp )
struct partition	*pp;
Pt_Queue		**qpp;
	{	Pt_Queue	*newq;
	RES_ACQUIRE( &rt_g.res_syscall );
	if( (newq = (Pt_Queue *) malloc( sizeof(Pt_Queue) )) == PT_Q_NULL )
		{
		Malloc_Bomb( sizeof(Pt_Queue) );
		RES_RELEASE( &rt_g.res_syscall );
		return	0;
		}
	RES_RELEASE( &rt_g.res_syscall );
	newq->q_next = *qpp;
	newq->q_part = pp;
	newq->q_space = pp->pt_forw->pt_regionp->reg_aircode;
	*qpp = newq;
	return	1;
	}

void
qFree( qp )
Pt_Queue	*qp;
	{
	if( qp == PT_Q_NULL )
		return;
	qFree( qp->q_next );
	RES_ACQUIRE( &rt_g.res_syscall );
	free( (char *) qp );
	RES_RELEASE( &rt_g.res_syscall );
	return;
	}

void
warning( str )
char	*str;
	{
	if( tty )
		HmError( str );
	else
		prntScr( str );
	return;
	}
