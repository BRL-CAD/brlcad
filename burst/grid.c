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

#define DEBUG_GRID	false
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "./vecmath.h"
#include "./ascii.h"
#include "./extern.h"

/* Local communication with worker(). */
static int	currshot;	/* Current shot index. */
static int	lastshot;	/* Final shot index. */

static int	gridwidth;	/* Grid width in cells. */
static int	gridheight;	/* Grid height in cells. */

static fastf_t	gridhor[3];	/* Horizontal grid direction cosines. */
static fastf_t	gridver[3];	/* Vertical grid direction cosines. */
static fastf_t	gridsoff[3];	/* Grid origin translated by standoff. */
static fastf_t	viewdir[3];	/* Direction of grid rays. */

static fastf_t	delta;		/* Angular delta ray of spall cone. */
static fastf_t	comphi;		/* Angle between ring and cone axis. */
static fastf_t	phiinc;		/* Angle between concentric rings. */

static fastf_t	cantdelta[3];	/* Delta ray specified by yaw and pitch.*/

static struct application ag;	/* Global application structure. */

_LOCAL_ bool		gridShot();
_LOCAL_ int		burstRay();
_LOCAL_ int		f_Hit();
_LOCAL_ int		f_HushOverlap();
_LOCAL_ int		f_Miss();
_LOCAL_ int		f_Overlap();
_LOCAL_ int		f_Penetrate();
_LOCAL_ int		f_Spall();
_LOCAL_ int		f_Canted_Warhead();
_LOCAL_ int		f_Warhead_Miss();
_LOCAL_ void		view_pix(), view_end();
_LOCAL_ void		spallVec();
void			consVector();

fastf_t
min( a, b )
fastf_t	a, b;
	{
	return	Min( a, b );
	}

fastf_t
max( a, b )
fastf_t	a, b;
	{
	return	Max( a, b );
	}

void
gridInit()
	{	fastf_t		gridrt, gridlf, gridup, griddn;

	notify( "Initializing grid..." );
	rt_prep_timer();

	/* Compute grid unit vectors. */
	gridRotate(	viewazim, viewelev, 0.0,
			gridhor, gridver );

	if( yaw != 0.0 || pitch != 0.0 )
		{	fastf_t	negsinyaw = -sin( yaw );
			fastf_t	sinpitch = sin( pitch );
			fastf_t	xdeltavec[3], ydeltavec[3];
		cantwarhead = true;
		Scale2Vec( gridhor,  negsinyaw, xdeltavec );
		Scale2Vec( gridver,  sinpitch,  ydeltavec );
		Add2Vec( xdeltavec, ydeltavec, cantdelta );
		}

	/* Unit vector from origin of model toward eye. */
	consVector( viewdir, viewazim, viewelev );

	/* Compute distances from grid origin (model origin) to each
		border of grid, and grid indices at borders of grid.
	 */
	if( firemode & FM_SHOT )
		{
		if( firemode & FM_FILE )
			rewind( shotfp );
		gridxorg = gridxfin = gridyorg = gridyfin = 0;
		}
	else
		{
		gridrt = max(	gridhor[X] * rtip->mdl_max[X],
				gridhor[X] * rtip->mdl_min[X]
				) +
			  max(	gridhor[Y] * rtip->mdl_max[Y],
				gridhor[Y] * rtip->mdl_min[Y]
				) +
			  max(	gridhor[Z] * rtip->mdl_max[Z],
				gridhor[Z] * rtip->mdl_min[Z]
				);
		gridlf = min(	gridhor[X] * rtip->mdl_max[X],
				gridhor[X] * rtip->mdl_min[X]
				) +
			  min(	gridhor[Y] * rtip->mdl_max[Y],
				gridhor[Y] * rtip->mdl_min[Y]
				) +
			  min(	gridhor[Z] * rtip->mdl_max[Z],
				gridhor[Z] * rtip->mdl_min[Z]
				);
		gridup = max(	gridver[X] * rtip->mdl_max[X],
				gridver[X] * rtip->mdl_min[X]
				) +
			  max(	gridver[Y] * rtip->mdl_max[Y],
				gridver[Y] * rtip->mdl_min[Y]
				) +
			  max(	gridver[Z] * rtip->mdl_max[Z],
				gridver[Z] * rtip->mdl_min[Z]
				);
		griddn = min(	gridver[X] * rtip->mdl_max[X],
				gridver[X] * rtip->mdl_min[X]
				) +
			  min(	gridver[Y] * rtip->mdl_max[Y],
				gridver[Y] * rtip->mdl_min[Y]
				) +
			  min(	gridver[Z] * rtip->mdl_max[Z],
				gridver[Z] * rtip->mdl_min[Z]
				);
		gridxorg = gridlf / cellsz;
		gridxfin = gridrt / cellsz;
		gridyorg = griddn / cellsz;
		gridyfin = gridup / cellsz;

		/* Allow for randomization of cells. */
		gridxorg--;
		gridxfin++;
		gridyorg--;
		gridxorg++;
		}

	/* Compute stand-off distance. */
	standoff = max(	viewdir[X] * rtip->mdl_max[X],
			viewdir[X] * rtip->mdl_min[X]
			) +
		  max(	viewdir[Y] * rtip->mdl_max[Y],
			viewdir[Y] * rtip->mdl_min[Y]
			) +
		  max(	viewdir[Z] * rtip->mdl_max[Z],
			viewdir[Z] * rtip->mdl_min[Z]
			);

	/* Determine largest grid dimension for frame buffer display. */
	gridwidth  = gridxfin - gridxorg + 1;
	gridheight = gridyfin - gridyorg + 1;
	gridsz = Max( gridwidth, gridheight );

	/* Vector to grid origin. */
	Scale2Vec( viewdir, standoff, gridsoff );

	/* Direction of grid rays. */
	ScaleVec( viewdir, -1.0 );

	prntTimer( "grid" );
	return;
	}

void
spallInit()
	{	fastf_t	theta;	 /* solid angle of spall sampling cone */
		fastf_t phi;	 /* angle between ray and cone axis */
		fastf_t philast; /* gaurd against floating point error */
		int	spallct; /* actual no. of sampling rays */
		int	m, n;

	/* Compute sampling cone of rays which are equally spaced. */
	theta = TWO_PI * (1.0 - cos( conehfangle )); /* Solid angle. */
	delta = sqrt( theta/nspallrays );	/* Angular ray delta. */
	n = conehfangle / delta;
	phiinc = conehfangle / n;
	philast = conehfangle + EPSILON;
	/* Crank through spall cone generation once to count actual number
		generated.
	 */
	for( phi = 0.0, spallct = 0; phi <= philast; phi += phiinc )
		{	fastf_t	sinphi = sin( phi );
			fastf_t	gamma, gammainc, gammalast;
		sinphi = Abs( sinphi );
		m = (TWO_PI * sinphi)/delta + 1;
		gammainc = TWO_PI / m;
		gammalast = TWO_PI-gammainc+EPSILON;
		for( gamma = 0.0; gamma <= gammalast; gamma += gammainc )
			spallct++;
		}
	raysolidangle = theta / spallct;
	rt_log( "Solid angle of sampling cone = %g\n", theta );
	rt_log( "Solid angle per sampling ray = %g\n", raysolidangle );
	rt_log( "%d sampling rays\n", spallct );
	return;
	}

void
gridModel()
	{
	if( cantwarhead )
		ag.a_onehit = true;
	else
		ag.a_onehit = false;
	ag.a_hit = f_Hit;
	ag.a_miss = f_Miss;
	ag.a_overlap = reportoverlaps ? f_Overlap : f_HushOverlap;
	ag.a_rt_i = rtip;

	plotInit();

	/* Output initial line for this aspect. */
	prntAspectInit();

	fatalerror = false;
	userinterrupt = false;	/* Set by interrupt handler. */

	/* Get starting and ending shot number. */
	currshot = 0;
	lastshot = gridwidth * gridheight - 1;

	rt_prep_timer();
	notify( "Raytracing..." );

	/*
	 * SERIAL case -- one CPU does all the work.
	 */
	if( ! gridShot() )
		return;
	view_end();
	return;
	}

_LOCAL_ int
readShot( vec )
register fastf_t	*vec;
	{
	if( !(firemode & FM_3DIM) )
		{	int	items;
		/* Read firing coordinates from file. */
		if( (items =
#if defined(sgi) && ! defined(mips)
		     fscanf( shotfp, "%f %f", &vec[X], &vec[Y] )) != 2 )
#else
		     fscanf( shotfp, "%lf %lf", &vec[X], &vec[Y] )) != 2 )
#endif
			{
			if( items != EOF )
				{
				rt_log( "Fatal error: only %d firing coordinates read.\n", items );
				return	false;
				}
			else
				return	EOF;
			}
		else
			{
			vec[X] /= unitconv;
			vec[Y] /= unitconv;
			}
		}
	else
	if( firemode & FM_3DIM )
		{	int	items;
		/* Read firing coordinates from file. */
		if( (items =
#if defined(sgi) && ! defined(mips)
		     fscanf( shotfp, "%f %f %f", &vec[X], &vec[Y], &vec[Z] )) != 3 )
#else
		     fscanf( shotfp, "%lf %lf %lf", &vec[X], &vec[Y], &vec[Z] )) != 3 )
#endif

			{
			if( items != EOF )
				{
				rt_log( "Fatal error: %d firing coordinates read.\n", items );
				return	false;
				}
			else
				return	EOF;
			}
		else
			{
			vec[X] /= unitconv;
			vec[Y] /= unitconv;
			vec[Z] /= unitconv;
			}
		}
	else
		{
		rt_log( "BUG: readShot called with bad firemode.\n" );
		return	false;
		}
	return	true;
	}

_LOCAL_ bool
getRayOrigin( ap )
register struct application	*ap;
	{	register fastf_t	*vec = ap->a_uvec;
		fastf_t			gridyinc[3], gridxinc[3];
		fastf_t			scalecx, scalecy;
	if( firemode & FM_SHOT )
		{
		if( firemode & FM_FILE )
			{
			switch( readShot( vec ) )
				{
			case EOF :	return	true;
			case true :	break;
			case false :	return	false;
				}
			}
		else	/* Single shot specified. */
			CopyVec( vec, fire );
		if( firemode & FM_3DIM )
			{	fastf_t	hitpoint[3];
			/* Project 3-d hit-point back into grid space. */
			CopyVec( hitpoint, vec );
			vec[X] = Dot( gridhor, hitpoint );
			vec[Y] = Dot( gridver, hitpoint );
			}
		ap->a_x = vec[X] / cellsz;
		ap->a_y = vec[Y] / cellsz;
		scalecx = vec[X];
		scalecy = vec[Y];
		}
	else
		{	fastf_t xoffset = 0.0;
			fastf_t yoffset = 0.0;
		ap->a_x = currshot % gridwidth + gridxorg;
		ap->a_y = currshot / gridwidth + gridyorg;
		if( dithercells )
			{
			/* 2-digit random number, 1's place gives X
				offset, 10's place gives Y offset.
			 */
#ifdef SYSV /* Use lrand48() only if random() not available.  */
			ap->a_user = lrand48() % 100;
#else
			ap->a_user = random() % 100;
#endif
			xoffset = (ap->a_user%10)*0.1 - 0.5;
			yoffset = (ap->a_user/10)*0.1 - 0.5;
			}
		/* Compute magnitude of grid offsets. */
		scalecx = (fastf_t) ap->a_x + xoffset;
		scalecy = (fastf_t) ap->a_y + yoffset;
		vec[X] = scalecx *= cellsz;
		vec[Y] = scalecy *= cellsz;
		}
	/* Compute cell horizontal and vertical	vectors relative to
		grid origin. */
	Scale2Vec( gridhor, scalecx, gridxinc );
	Scale2Vec( gridver, scalecy, gridyinc );
	Add2Vec( gridsoff, gridyinc, ap->a_ray.r_pt );
	AddVec( ap->a_ray.r_pt, gridxinc );
	return	true;
	}

_LOCAL_ void
prntFiringCoords( vec )
register fastf_t	*vec;
	{
#if false
	rt_log( "Firing coords <%7.2f,%7.2f>\n",
		vec[X]*unitconv, vec[Y]*unitconv );
#endif
	if( gridfile[0] != '\0' )
		(void) fprintf( gridfp,
				"%7.2f %7.2f\n",
				vec[X]*unitconv, vec[Y]*unitconv );
	return;
	}

_LOCAL_ bool
gridShot()
	{	bool			status = true;
		struct application	a;
	a.a_resource = RESOURCE_NULL;
	a.a_hit = ag.a_hit;
	a.a_miss = ag.a_miss;
	a.a_overlap = ag.a_overlap;
	a.a_rt_i = ag.a_rt_i;
	a.a_onehit = ag.a_onehit;
	a.a_user = 0;
	prntGridOffsets( gridxorg, gridyorg );
	noverlaps = 0;
	for( ; ! userinterrupt; view_pix( &a ) )
		{
		if( !(firemode & FM_FILE) && currshot > lastshot )
			break;
		if( ! (status = getRayOrigin( &a )) )
			break;
		currshot++;
		prntFiringCoords( a.a_uvec );
		CopyVec( a.a_ray.r_dir, viewdir );
		a.a_level = 0;	 /* Recursion level. */
		plotGrid( a.a_ray.r_pt );
		if( rt_shootray( &a ) == -1 && fatalerror )
			{
			/* Fatal error in application routine. */
			rt_log( "Fatal error: raytracing aborted.\n" );
			return	false;
			}
		if( !(firemode & FM_FILE) && (firemode & FM_SHOT) )
			break;
		}
	return	status;
	}

void
color_Part( regp, type )
register struct region	*regp;
int			type;
	{	Colors	*colorp;
	if( plotfp == NULL )
		return;
	RES_ACQUIRE( &rt_g.res_syscall );
	switch( type )
		{
	case C_CRIT :
		if( (colorp = findColors( regp->reg_regionid, &colorids ))
			== COLORS_NULL )
			pl_color( plotfp, R_CRIT, G_CRIT, B_CRIT );
		else
			pl_color( plotfp,
				  (int) colorp->c_rgb[0],
				  (int) colorp->c_rgb[1],
				  (int) colorp->c_rgb[2]
				  );
		break;
	case C_SHIELD :
		pl_color( plotfp, R_SHIELD, G_SHIELD, B_SHIELD );
		break;
	case C_MAIN :
		if( (colorp = findColors( regp->reg_regionid, &colorids ))
			== COLORS_NULL )
			{
			if( InsideAir(regp ) )
				pl_color( plotfp,
					  R_INAIR, G_INAIR, B_INAIR );
			else
			if( Air(regp ) )
				pl_color( plotfp,
					  R_OUTAIR, G_OUTAIR, B_OUTAIR );
			else
				pl_color( plotfp, R_COMP, G_COMP, B_COMP );
			}
		else
			pl_color( plotfp,
				 (int) colorp->c_rgb[0],
				 (int) colorp->c_rgb[1],
				 (int) colorp->c_rgb[2]
				 );
		break;
	default :
		rt_log( "color_Part: bad type %d.\n", type );
		break;
		}
	RES_RELEASE( &rt_g.res_syscall );
	return;
	}

/*	enforce_LOS()

	Enforce the line-of-sight tolerance by deleting partitions that are
	too thin.
 */
_LOCAL_ void
enforce_LOS( ap, pt_headp )
register struct application	*ap;
register struct partition	*pt_headp;
	{	register struct partition	*pp;
	for( pp = pt_headp->pt_forw; pp != pt_headp; )
		{	register struct partition *nextpp = pp->pt_forw;
		if( pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist
			<= LOS_TOL )
			{
			DEQUEUE_PT( pp );
			FREE_PT( pp, ap->a_resource );
			}
		pp = nextpp;
		}
	return;
	}

_LOCAL_ int
/*ARGSUSED*/
f_HushOverlap( ap, pp, reg1, reg2 )
struct application	*ap;
struct partition	*pp;
struct region		*reg1, *reg2;
	{	fastf_t depth = pp->pt_outhit->hit_dist -
					pp->pt_inhit->hit_dist;
	if( depth < OVERLAP_TOL )
		return  1;
	noverlaps++;
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Overlap( ap, pp, reg1, reg2 )
struct application	*ap;
struct partition	*pp;
struct region		*reg1, *reg2;
	{	point_t pt;
		fastf_t depth = pp->pt_outhit->hit_dist -
					pp->pt_inhit->hit_dist;
	if( depth < OVERLAP_TOL )
		return  1;
	VJOIN1( pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist,
		ap->a_ray.r_dir );
	rt_log( "OVERLAP:\n" );
	rt_log( "reg=%s isol=%s,\n",
		reg1->reg_name, pp->pt_inseg->seg_stp->st_name
		);
	rt_log( "reg=%s osol=%s,\n",
		reg2->reg_name, pp->pt_outseg->seg_stp->st_name
		);
	rt_log( "depth %gmm at (%g,%g,%g) x%d y%d lvl%d\n",
		depth,
		pt[X], pt[Y], pt[Z],
		ap->a_x, ap->a_y, ap->a_level
		);
	noverlaps++;
	return	1;
	}

_LOCAL_ int
f_Hit( ap, pt_headp )
struct application	*ap;
struct partition	*pt_headp;
	{
	/* Output cell identification. */
	prntCellIdent( ap, pt_headp );
	if( cantwarhead )
		return	f_Canted_Warhead( ap, pt_headp );
	else
		return	f_Penetrate( ap, pt_headp );
	}

_LOCAL_ int
f_Warhead_Miss( ap )
struct application	*ap;
	{
	if( setback != 0.0 )
		prntComin( ap, PT_NULL );
	return	0;
	}

_LOCAL_ int
f_Canted_Warhead( ap, pt_headp )
struct application	*ap;
struct partition	*pt_headp;
	{	struct application	a_cant;
		register fastf_t	*rdir = ap->a_ray.r_dir;
		register fastf_t	*cantpt = a_cant.a_ray.r_pt;
		register fastf_t	*cantdir = a_cant.a_ray.r_dir;
		register fastf_t	*hitpointp;
	a_cant = *ap;
	a_cant.a_hit = f_Penetrate;
	a_cant.a_miss = f_Warhead_Miss;
	a_cant.a_overlap = ap->a_overlap; /* Shouldn't need this. */
	a_cant.a_onehit = false;
	a_cant.a_level = ap->a_level+1;

	/* Calculate trajectory of canted warhead. */
	hitpointp = pt_headp->pt_forw->pt_inhit->hit_point;
	color_Part( pt_headp->pt_forw->pt_regionp, C_MAIN );
	plotPartition(	pt_headp->pt_forw->pt_inhit,
			pt_headp->pt_forw->pt_outhit,
			&ap->a_ray,
			pt_headp->pt_forw->pt_regionp
			);
	VJOIN1( cantpt, hitpointp, -setback, rdir );
	/* Perturb ray direction. */
	Add2Vec( rdir, cantdelta, cantdir );
	V_Length( cantdir, 1.0 );
	return	rt_shootray( &a_cant );
	}

_LOCAL_ int
f_Penetrate( ap, pt_headp )
struct application	*ap;
struct partition	*pt_headp;
	{	register struct partition *pp;
		register int		ct = 0; /* Cumulative count. */
		struct partition	*bp = PT_NULL;
	if( setback != 0.0 )
		prntComin( ap, pt_headp );

	/* First delete thin partitions. */
	enforce_LOS( ap, pt_headp );

	/* Output ray intersections.  This code is extremely cryptic because
		it is dealing with errors in the geometry, where there is
		either adjacent airs of differing types, or voids (gaps)
		in the description.  In the case of adjacent airs, phantom
		armor must be output.  For voids, outside air is the default
		(everyone knows that air rushes to fill a vacuum), so we
		must pretend that it is there.  Outside air is also called
		01 air because its aircode equals 1.  Please tread carefully
		on the code within this loop, it is filled with special
		cases involving adjacency of partitions both real (explicit)
		and imagined (implicit).
	 */
	for( pp = pt_headp->pt_forw; pp != pt_headp; pp = pp->pt_forw )
		{	fastf_t	los = 0.0;
			int	voidflag = false;
			register struct partition *np = pp->pt_forw;
			register struct partition *cp;
			register struct region	*regp = pp->pt_regionp;
			register struct region	*nregp = pp->pt_forw->pt_regionp;
		/* Fill in entry and exit points into hit structures. */
		{	register struct hit	*ihitp = pp->pt_inhit;
			register struct hit	*ohitp = pp->pt_outhit;
			register struct xray	*rayp = &ap->a_ray;
		VJOIN1( ihitp->hit_point, rayp->r_pt,
			ihitp->hit_dist, rayp->r_dir );
		VJOIN1( ohitp->hit_point, rayp->r_pt,
			ohitp->hit_dist, rayp->r_dir );
		color_Part( regp, C_MAIN );
		plotPartition( ihitp, ohitp, rayp, regp );
		}

		/* Check for voids. */
		if( np != pt_headp )
			{
			los = np->pt_inhit->hit_dist -
					pp->pt_outhit->hit_dist;
			voidflag = ( los > LOS_TOL );
			/* If the void occurs adjacent to explicit outside
				air, extend the outside air to fill it. */
			if( OutsideAir( np->pt_regionp ) )
				{
				if( voidflag )
					{
					np->pt_inhit->hit_dist =
						pp->pt_outhit->hit_dist;
					voidflag = false;
					}
				/* Keep going until we are past 01 air. */
				for(	cp = np->pt_forw;
					cp != pt_headp;
					cp = cp->pt_forw )
					{
					if( OutsideAir( cp->pt_regionp ) )
						np->pt_outhit->hit_dist =
						cp->pt_outhit->hit_dist;
					else
					if( cp->pt_inhit->hit_dist -
					    np->pt_outhit->hit_dist
						> LOS_TOL
						)
						np->pt_outhit->hit_dist =
						cp->pt_inhit->hit_dist;
					else
						break;
					}
				}
			}
		/* Merge adjacent inside airs of same type. */
		if( np != pt_headp && InsideAir( np->pt_regionp ) )
			{
			prntScr( "Merging inside airs" );
			for(	cp = np->pt_forw;
				cp != pt_headp;
				cp = cp->pt_forw )
				{
				if(	InsideAir( cp->pt_regionp )
				    &&	SameAir( np->pt_regionp,
						 cp->pt_regionp )
				    &&	cp->pt_inhit->hit_dist -
						np->pt_outhit->hit_dist
					<= LOS_TOL
					)
					np->pt_outhit->hit_dist =
						cp->pt_outhit->hit_dist;
				else
					break;
				}
			}

		/* Check for possible phantom armor before internal air,
			that is if it is the first thing hit. */
		if( pp->pt_back == pt_headp && InsideAir( regp ) )
			{	fastf_t	slos;
			slos = pp->pt_outhit->hit_dist -
					pp->pt_inhit->hit_dist;
			prntPhantom( ap, regp->reg_aircode, slos, ++ct );
			}				
		else
		/* If we have a component, output it. */
		if( ! Air( regp ) )
			{
			/* If there is a void, output 01 air as space. */
			if( voidflag )
				{
#if DEBUG_GRID
				rt_log( "outputting 01 air\n" );
#endif
				prntSeg( ap, pp, false,
						OUTSIDE_AIR, los, ++ct );
				}
			else
			/* If air expicitly follows, output space code. */
			if( np != pt_headp && Air( nregp ) )
				{ fastf_t slos = np->pt_outhit->hit_dist -
						np->pt_inhit->hit_dist;
				/* Check for burst point. */
#if DEBUG_GRID
				rt_log( "\texplicit air follows\n" );
#endif
				if(	bp == PT_NULL
				    &&	findIdents( regp->reg_regionid,
							&armorids )
				    &&	findIdents( nregp->reg_aircode,
							&airids )
					)
					{
					bp = pp;
					prntSeg( ap, pp, true,
						 nregp->reg_aircode,
						 slos, ++ct );
					}
				else
					prntSeg( ap, pp, false,
						 nregp->reg_aircode,
						 slos, ++ct );
				}
			else
			if( np == pt_headp )
				{
				/* Last component gets 09 air. */
#if DEBUG_GRID
				rt_log( "\toutputting 09 air\n" );
#endif
				prntSeg( ap, pp, false,
					 EXIT_AIR, 0.0, ++ct );
				}
			else
			/* No air follows component. */
			if( SameCmp( regp, nregp ) )
				{
#if DEBUG_GRID
				rt_log( "\tmerging adjacent components\n" );
#endif
				/* Merge adjacent components with same
					idents. */
				np->pt_inhit->hit_dist =
					pp->pt_inhit->hit_dist;
				continue;
				}
			else
				{
#if DEBUG_GRID
				rt_log( "\tdifferent component follows\n" );
#endif
				prntSeg( ap, pp, false, 0, 0.0, ++ct );
				}
			}
		/* Check for adjacency of differing airs, implicit or
			explicit and output phantom armor as needed. */
		if( InsideAir( regp ) )
			{
			/* Inside air followed by implicit outside air. */
			if( voidflag )
				prntPhantom( ap, OUTSIDE_AIR, los, ++ct );
			}
		/* Check next partition for adjacency problems. */
		if( np != pt_headp )
			{
			/* See if inside air follows impl. outside air. */
			if( voidflag && InsideAir( nregp ) )
				{	fastf_t	slos =
						np->pt_outhit->hit_dist -
						np->pt_inhit->hit_dist;
				prntPhantom( ap, nregp->reg_aircode,
						slos, ++ct );
				}
			else
			/* See if differing airs are adjacent. */
			if(   !	voidflag
			    &&	Air( regp )
			    &&	Air( nregp )
			    &&	DiffAir( nregp, regp )
				)
				{ fastf_t slos = np->pt_outhit->hit_dist -
						 np->pt_inhit->hit_dist;
				prntPhantom( ap, nregp->reg_aircode,
						slos, ++ct );
				}
			}
		/* Output phantom armor if internal air is last hit. */
		if( np == pt_headp && InsideAir( regp ) )
			prntPhantom( ap, EXIT_AIR, 0.0, ++ct );
		}
	if( nriplevels == 0 )
		return	1;

	if( bp != PT_NULL )
		{
		/* This is a burst point. */
		prntBurstHdr(	ap, bp, bp->pt_forw );
		return	burstPoint( ap, bp );
		}
	return	1;
	}

_LOCAL_ int
f_Miss( ap )
register struct application *ap;
	{
	VSETALL( ap->a_color, 0.0 ); /* All misses black. */
	return	0;
	}

void
lgt_Model( ap, pp, hitp, rayp )
register struct application	*ap;
struct partition	*pp;
struct hit		*hitp;
struct xray		*rayp;
	{	Colors  *colorp;
		fastf_t intensity = -Dot( rayp->r_dir, hitp->hit_normal );
	if( intensity < 0.0 )
		intensity = 0.0;

	if(	(colorp =
		findColors( pp->pt_regionp->reg_regionid, &colorids ))
		!= COLORS_NULL
		)
		{
		ap->a_color[RED] = colorp->c_rgb[RED]/255.0;
		ap->a_color[GRN] = colorp->c_rgb[GRN]/255.0;
		ap->a_color[BLU] = colorp->c_rgb[BLU]/255.0;
		}
	else
		ap->a_color[RED] =
		ap->a_color[GRN] =
		ap->a_color[BLU] = 1.0;
	ScaleVec( ap->a_color, intensity );
	return;
	}

_LOCAL_ int
f_Spall( ap, pt_headp )
struct application	*ap;
struct partition	*pt_headp;
	{	Pt_Queue			*qshield = PT_Q_NULL;
		register struct partition	*cpp, *spp;
		register int			nbar;
		register int			ncrit = 0;
	/* Find first barrier in front of the burst point. */
	for(	spp = pt_headp->pt_forw;
		spp != pt_headp
	    &&	spp->pt_outhit->hit_dist < 0.1;
		spp = spp->pt_forw
		) 
		;
	for(	cpp = spp, nbar = 0;
		cpp != pt_headp && nbar <= nbarriers;
		cpp = cpp->pt_forw
		) 
		{	register struct region	*regp = cpp->pt_regionp;
		if( Air( regp ) )
			continue; /* Air doesn't matter here. */
		if( findIdents( regp->reg_regionid, &critids ) )
			{	register struct xray	*rayp = &ap->a_ray;
				register struct hit	*hitp;
			hitp = cpp->pt_inhit;
			if( fbfile[0] != NUL && ncrit == 0 )
				{	register struct soltab	*stp;
				stp = cpp->pt_inseg->seg_stp;
				RT_HIT_NORM( hitp, stp, rayp );
				Check_Iflip( cpp, hitp->hit_normal,
						rayp->r_dir );
				lgt_Model( ap, cpp, hitp, rayp );
				}
			else
				{
				VJOIN1( hitp->hit_point, rayp->r_pt,
					hitp->hit_dist, rayp->r_dir );
				}
			prntRegionHdr( ap, hitp, regp, nbar );
			hitp = cpp->pt_outhit;
			VJOIN1( hitp->hit_point, rayp->r_pt,
				hitp->hit_dist, rayp->r_dir );
			color_Part( regp, C_CRIT );
			plotPartition( cpp->pt_inhit, hitp, rayp, regp );
			prntShieldComp( ap, rayp, qshield );
			ncrit++;
			}
		/* Add all components to list of shielding components. */
		if( cpp->pt_forw != pt_headp )
			{
			if( ! qAdd( cpp, &qshield ) )
				{
				fatalerror = true;
				return	-1;
				}
			nbar++;
			}
		}
	qFree( qshield );
	if( ncrit == 0 )
		return	ap->a_miss( ap );
	else
		return	ncrit;
	}

_LOCAL_ int
round( f )
fastf_t	f;
	{	register int	a;
	a = f;
	if( f - a >= 0.5 )
		return	a + 1;
	else
		return	a;
	}

struct application	a_burst; /* Prototype spall ray. */

_LOCAL_ int
burstPoint( ap, pp )
register struct application	*ap;
register struct partition	*pp;
	{
#ifdef alliant
		register int	d7;	/* known to be in d7 */
#endif
		fastf_t			s_rdir[3];
		register struct hit	*ohitp;
		register struct soltab	*stp;
	prntScr( "burstPoint" );
	/* Fill in hit point and normal. */
	stp = pp->pt_outseg->seg_stp;
	ohitp = pp->pt_outhit;
	RT_HIT_NORM( ohitp, stp, &(ap->a_ray) );
	Check_Oflip( pp, ohitp->hit_normal, ap->a_ray.r_dir );

	a_burst = *ap;
	a_burst.a_miss = f_Miss;
	a_burst.a_hit = f_Spall;
	a_burst.a_overlap = ap->a_overlap; /* Shouldn't need this. */
	a_burst.a_level++;
	a_burst.a_user = 0; /* Ray number. */

	/* By default, spall cone axis is half way between main penetrator
		direction and exit normal of critical component.
	 */
	if( deflectcone )
		{
		AddVec( a_burst.a_ray.r_dir, ohitp->hit_normal );
		V_Length( a_burst.a_ray.r_dir, 1.0 );
		}
	CopyVec( a_burst.a_ray.r_pt, ohitp->hit_point );
	comphi = 0.0; /* Initialize global for concurrent access. */
		
	/* SERIAL case -- one CPU does all the work. */
	return	burstRay();
	}

_LOCAL_ int
burstRay()
	{ /* Need local copy of all but readonly variables for concurrent
		threads of execution. */
		struct application	a_spall;
		fastf_t			phi;
		fastf_t			nrings = conehfangle / phiinc;
		int			m;
	prntScr( "burstray()" );
	a_spall = a_burst;
	a_spall.a_resource = RESOURCE_NULL;
	for( ; ! userinterrupt; )
		{	fastf_t	sinphi;
			fastf_t	gamma, gammainc;
			register int	done;
		RES_ACQUIRE( &rt_g.res_worker );
		phi = comphi;
		comphi += phiinc;
		done = phi > conehfangle;
		RES_RELEASE( &rt_g.res_worker );
		if( done )
			break;
		sinphi = sin( phi );
		sinphi = Abs( sinphi );
		m = (TWO_PI * sinphi)/delta + 1;
		gammainc = TWO_PI / m;
		for(	gamma = 0.0;
			gamma <= TWO_PI-gammainc;
			gamma += gammainc )
			{	register int	ncrit;
			spallVec( a_burst.a_ray.r_dir, a_spall.a_ray.r_dir,
				 phi, gamma );
			plotRay( &a_spall.a_ray );
			RES_ACQUIRE( &rt_g.res_worker );
			a_spall.a_user = a_burst.a_user++;
			RES_RELEASE( &rt_g.res_worker );
			if(	(ncrit = rt_shootray( &a_spall )) == -1
			     &&	fatalerror )
				{
				/* Fatal error in application routine. */
				rt_log( "Fatal error: raytracing aborted.\n" );
				return	0;
				}
			if( fbfile[0] != NUL && ncrit > 0 )
				{	RGBpixel	pixel;
					int		x, y;
					int		err;
				pixel[RED] = a_spall.a_color[RED] * 255;
				pixel[GRN] = a_spall.a_color[GRN] * 255;
				pixel[BLU] = a_spall.a_color[BLU] * 255;
				x = round( 255 +
					Dot( a_spall.a_ray.r_dir, gridhor )
					* nrings );
				y = round( 255 +
					Dot( a_spall.a_ray.r_dir, gridver )
					* nrings );
				RES_ACQUIRE( &rt_g.res_stats );
				err = fb_write( fbiop, x, y, pixel, 1 );
				RES_RELEASE( &rt_g.res_stats );
				if( err == -1 )
					rt_log( "Write failed to pixel <%d,%d>.\n", x, y );
				}
			if( histfile[0] != NUL )
				{
				RES_ACQUIRE( &rt_g.res_syscall );
				(void) fprintf( histfp, "%d\n", ncrit );
				RES_RELEASE( &rt_g.res_syscall );
				}
			}
		}
	return	1;
	}

_LOCAL_ void
spallVec( dvec, s_rdir, phi, gamma )
register fastf_t	*dvec, *s_rdir;
fastf_t			phi, gamma;
	{	static fastf_t		xaxis[3] = { 1.0, 0.0, 0.0 };
		static fastf_t		zaxis[3] = { 0.0, 0.0, 1.0 };
		static fastf_t		negzaxis[3] = { 0.0, 0.0, -1.0 };
		fastf_t			cosphi = cos( phi );
		fastf_t			singamma = sin( gamma );
		fastf_t			cosgamma = cos( gamma );
		fastf_t			csgaphi, ssgaphi;
		fastf_t			sinphi = sin( phi );
		fastf_t			cosdphi[3];
		fastf_t			fvec[3];
		fastf_t			evec[3];

	if(	AproxEqVec( dvec, zaxis, Epsilon )
	    ||	AproxEqVec( dvec, negzaxis, Epsilon )
		)
		{
		CopyVec( evec, xaxis );
		}
	else
		{
		CrossProd( dvec, zaxis, evec );
		}
	CrossProd( evec, dvec, fvec );
	Scale2Vec( dvec, cosphi, cosdphi );
	ssgaphi = singamma * sinphi;
	csgaphi = cosgamma * sinphi;
	VJOIN2( s_rdir, cosdphi, ssgaphi, evec, csgaphi, fvec );
	return;
	}

/*	c o n s _ V e c t o r ( )
	Construct a direction vector out of azimuth and elevation angles
	in radians, allocating storage for it and returning its address.
 */
void
consVector( vec, azim, elev )
register fastf_t	*vec;
fastf_t	azim, elev;
	{ /* Store cosine of the elevation to save calculating twice. */
		fastf_t	cosE;
	cosE = cos( elev );
	vec[0] = cos( azim ) * cosE;
	vec[1] = sin( azim ) * cosE;
	vec[2] = sin( elev );
	return;
	}

#if defined( SYSV )
/*ARGSUSED*/
void
#else
int
#endif
abort_RT( sig )
int	sig;
	{
	(void) signal( SIGINT, abort_RT );
#if false
	if( fbfile[0] != NUL )
		(void) fb_flush( fbiop );
#endif
	userinterrupt = 1;
#if defined( SYSV )
	return;
#else
	return	sig;
#endif
	}

/*	i p o w ( )
	Integer exponent pow() function.
	Returns 'd' to the 'n'th power.
 */
_LOCAL_ fastf_t
ipow( d, n )
register fastf_t	d;
register int	n;
	{	register fastf_t	result = 1.0;
	if( d == 0.0 )
		return	0.0;
	while( n-- > 0 )
		result *= d;
	return	result;
	}

/*	v i e w _ p i x ( ) */
_LOCAL_ void
view_pix( ap )
register struct application	*ap;
	{
#if 0
		/* map grid offsets to frame buffer coordinates */
		int	x = ap->a_x-gridxorg+(gridsz-gridwidth)/2;
		int	y = ap->a_y-gridyorg+(gridsz-gridheight)/2;
#endif
	RES_ACQUIRE( &rt_g.res_syscall );
	prntGridOffsets( ap->a_x, ap->a_y );
	if( tty )
		prntTimer( NULL );
	RES_RELEASE( &rt_g.res_syscall );
	return;
	}

/*	v i e w _ e n d ( ) */
_LOCAL_ void
view_end()
	{
#if false
	if( fbfile[0] != NUL )
		fb_flush( fbiop );
#endif
	if( gridfile[0] != NUL )
		(void) fflush( gridfp );
	if( histfile[0] != NUL )
		(void) fflush( histfp );
	if( plotfile[0] != NUL )
		(void) fflush( plotfp );
	if( outfile[0] != NUL )
		(void) fflush( outfp );
	prntTimer( "view" );
	if( noverlaps > 0 )
		rt_log( "%d overlaps detected over %g mm thick.\n",
			noverlaps, OVERLAP_TOL );
	return;
	}
