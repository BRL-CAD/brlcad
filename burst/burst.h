/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or AV-298-6651

	$Header$ (BRL)
 */
#ifndef INCL_BURST
#define INCL_BURST
/* boolean types */
typedef int	bool;
#define true		1
#define false		0
#define success		0
#define failure		1
/* allow for SGI screw-up, the single-precision math libraries */
#if defined( sgi ) && ! defined( mips )
#define SINGLE_PRECISION true
#else
#define SINGLE_PRECISION false
#endif
/* menu configuration */
#define MENU_LFT	1
#define MENU_TOP	2
#define MENU_MAXVISITEMS	10
/* prompt line configuration */
#define PROMPT_X	HmXPROMPT
#define PROMPT_Y	HmYPROMPT
/* banner (border) configuration */
#define BORDER_CHR	'_'
#define BORDER_Y	(PROMPT_Y+1)
/* grid offset printing window */
#define GRID_X		55	/* where grid indices are printed */
#define GRID_Y		BORDER_Y
/* scroll region configuration */
#define SCROLL_TOP	(BORDER_Y+1)
#define SCROLL_BTM	(ScLI-1) /* bottom line causes scroll */
/* timer (cpu statistics) window configuration */
#define TIMER_X		1
#define TIMER_Y		1
/* buffer sizes */
#define LNBUFSZ		133	/* buffer for one-line messages */
#define MAXDEVWID	1024	/* maximum width of frame buffer */

#define CHAR_COMMENT	'#'

/* default parameters */
#define DFL_AZIMUTH	0.0
#define DFL_BARRIERS	100
#define DFL_BDIST	0.0
#define DFL_CELLSIZE	1.0
#define DFL_CONEANGLE	90.0
#define DFL_DEFLECT	false
#define DFL_DITHER	false
#define DFL_ELEVATION	0.0
#define DFL_NRAYS	200
#define DFL_OVERLAPS	true
#define DFL_RIPLEVELS	1
#define DFL_UNITS	U_MILLIMETERS

/* firing mode bit definitions */
#define FM_GRID  0	/* generate grid of shotlines */
#define FM_DFLT	 FM_GRID
#define FM_PART  (1)	/* bit 0: ON = partial envelope, OFF = full */
#define FM_SHOT	 (1<<1)	/* bit 1: ON = discrete shots, OFF = gridding */
#define FM_FILE	 (1<<2)	/* bit 2: ON = file input, OFF = direct input */
#define FM_3DIM	 (1<<3)	/* bit 3: ON = 3-D coords., OFF = 2-D coords */
#define FM_BURST (1<<4) /* bit 4: ON = discrete burst points, OFF = shots */

/* flags for notify() */
#define	NOTIFY_APPEND	1
#define NOTIFY_DELETE	2
#define NOTIFY_ERASE	4

#define NOTIFY_DELIM	':'

#define P_HEADER_INIT	'1'
#define P_ASPECT_INIT	'2'
#define P_CELL_IDENT	'3'
#define P_RAY_INTERSECT	'4'
#define P_BURST_HEADER	'5'
#define P_REGION_HEADER	'6'
#define P_SHIELD_COMP	'7'

#define TITLE_LEN	72
#define TIMER_LEN	72

#define U_INCHES	0
#define U_FEET		1
#define U_MILLIMETERS	2
#define U_CENTIMETERS	3
#define U_METERS	4
#define U_BAD		-1

#define UNITS_INCHES		"inches"
#define UNITS_FEET		"feet"
#define UNITS_MILLIMETERS	"millimeters"
#define UNITS_CENTIMETERS	"centimeters"
#define UNITS_METERS		"meters"

/* white space in input tokens */
#define WHITESPACE	" \t"

#define R_OUTAIR	100
#define G_OUTAIR	100
#define B_OUTAIR	255
#define R_INAIR		100
#define G_INAIR		255
#define B_INAIR		100
#define R_COMP		0
#define G_COMP		0
#define B_COMP		255
#define R_CRIT		255
#define G_CRIT		0
#define B_CRIT		255
#define R_SHIELD	255
#define G_SHIELD	255
#define B_SHIELD	255

#define C_MAIN		0
#define C_CRIT		1
#define C_SHIELD	2

#define TWO_PI		6.28318530717958647692528676655900576839433879875022

#define Epsilon		0.001
#define LOS_TOL		0.1
#define OVERLAP_TOL	0.25	/* thinner overlaps not reported */
#define EXIT_AIR	9	/* exit air is called 09 air */
#define OUTSIDE_AIR	1	/* outside air is called 01 air */

#define Air(rp)		((rp)->reg_aircode > 0)
#define DiffAir(rp,sp)	((rp)->reg_aircode != (sp)->reg_aircode)
#define SameAir(rp,sp)	((rp)->reg_aircode == (sp)->reg_aircode)
#define SameCmp(rp,sp)	((rp)->reg_regionid == (sp)->reg_regionid)
#define OutsideAir(rp)	((rp)->reg_aircode == OUTSIDE_AIR)
#define InsideAir(rp)	(Air(rp)&& !OutsideAir(rp))

#define Check_Iflip( _pp, _normal, _rdir )\
	{	fastf_t	f;\
	if( _pp->pt_inflip )\
		{\
		ScaleVec( _normal, -1.0 );\
		_pp->pt_inflip = 0;\
		}\
	f = Dot( _rdir, _normal );\
	if( f >= 0.0 )\
		{\
		ScaleVec( _normal, -1.0 );\
		rt_log( "Fixed flipped entry normal\n" );\
		}\
	}
#define Check_Oflip( _pp, _normal, _rdir )\
	{	fastf_t	f;\
	if( _pp->pt_outflip )\
		{\
		ScaleVec( _normal, -1.0 );\
		_pp->pt_outflip = 0;\
		}\
	f = Dot( _rdir, _normal );\
	if( f <= 0.0 )\
		{\
		ScaleVec( _normal, -1.0 );\
		rt_log( "Fixed flipped exit normal\n" );\
		}\
	}

#define Malloc_Bomb( _bytes_ ) \
                rt_log( "\"%s\"(%d) : allocation of %d bytes failed.\n",\
                                __FILE__, __LINE__, _bytes_ )

#define Swap_Doubles( a_, b_ ) \
		{	fastf_t	f_ = a_; \
		a_ = b_; \
		b_ = f_; \
		}
#define Toggle(f)	(f) = !(f)

/* stolen from Doug Gwyn's /vld/include/std.h */
#define DEGRAD	57.2957795130823208767981548141051703324054724665642

typedef struct ids	Ids;
struct ids
	{
	short	i_lower;
	short	i_upper;
	Ids	*i_next;
	};
#define IDS_NULL	(Ids *) 0

typedef struct colors	Colors;
struct colors
	{
	short	c_lower;
	short	c_upper;
	unsigned char	c_rgb[3];
	Colors	*c_next;
	};
#define COLORS_NULL	(Colors *) 0

typedef struct pt_queue	Pt_Queue;
struct pt_queue
	{
	int			q_space;
	struct partition	*q_part;
	Pt_Queue		*q_next;
	};

#define PT_Q_NULL	(Pt_Queue *) 0
#endif /* INCL_BURST */
