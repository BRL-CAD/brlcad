#ifndef SHADEFUNCS 
#define SHADEFUNCS
/*
 *			S H A D E F U N C S . H
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
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

/* for bu_printb() */
#define MFI_FORMAT	"\020\4HIT\3LIGHT\2UV\1NORMAL"


/* mf_flags lists important details about individual shaders */
#define MFF_PROC	0x01		/* shader is procedural, computes tr/re/hits */

BU_EXTERN(void		mlib_add_shader, (struct mfuncs **headp,
				struct mfuncs *mfp1));
BU_EXTERN(int		mlib_setup, (struct mfuncs **headp,
				struct region *rp,
				struct rt_i *rtip));
BU_EXTERN(void		mlib_free, (struct region *rp));
#endif
