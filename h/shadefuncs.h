/*
 *			M A T E R I A L . H
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 *  $Header$
 */

/*
 *			M F U N C S
 *
 *  The interface to the various material property & texture routines.
 */
struct mfuncs {
	long		mf_magic;	/* To validate structure */
	char		*mf_name;	/* Keyword for material */
	struct mfuncs	*mf_forw;	/* Forward link */
	int		mf_inputs;	/* shadework inputs needed */
	int		mf_flags;	/* Flags describing shader */
	int		(*mf_setup)();	/* Routine for preparing */
	int		(*mf_render)();	/* Routine for rendering */
	void		(*mf_print)();	/* Routine for printing */
	void		(*mf_free)();	/* Routine for releasing storage */
};
#define MF_MAGIC	0x55968058
#define MF_NULL		((struct mfuncs *)0)
#define RT_CK_MF(_p)	BU_CKMAG(_p, MF_MAGIC, "mfuncs")

/*
 *  mf_inputs lists what optional shadework fields are needed.
 *  dist, point, color, & default(trans,reflect,ri) are always provided
 */
#define MFI_NORMAL	0x01		/* Need normal */
#define MFI_UV		0x02		/* Need uv */
#define MFI_LIGHT	0x04		/* Need light visibility */
#define MFI_HIT		0x08		/* Need just hit point */


/* mf_flags lists important details about individual shaders */
#define MFF_PROC	0x01		/* shader is procedural, computes tr/re/hits */

#define SW_NLIGHTS	16		/* Max # of light sources */

/*
 *			S H A D E W O R K
 */
struct shadework {
	fastf_t		sw_transmit;	/* 0.0 -> 1.0 */
	fastf_t		sw_reflect;	/* 0.0 -> 1.0 */
	fastf_t		sw_refrac_index;
	fastf_t		sw_extinction;	/* extinction coeff, mm^-1 */
	fastf_t		sw_color[3];	/* shaded color */
	fastf_t		sw_basecolor[3]; /* base color */
	struct hit	sw_hit;		/* ray hit (dist,point,normal) */
	struct uvcoord	sw_uv;
	fastf_t		sw_intensity[3*SW_NLIGHTS]; /* light intensities */
	fastf_t		sw_tolight[3*SW_NLIGHTS];   /* light directions */
	char		*sw_visible[SW_NLIGHTS]; /* visibility flags/ptrs */
	int		sw_xmitonly;	/* flag: need sw_transmit only */
	int		sw_inputs;	/* fields from mf_inputs actually filled */
	int		sw_frame;	/* # of current frame */
	fastf_t		sw_frametime;	/* frame time delta off 1st frame */
	fastf_t		sw_pixeltime;	/* pixel time delta off 1st pixel of 1st frame */
	struct seg	*sw_segs;	/* segs which made partition */
};

extern void pr_shadework();
