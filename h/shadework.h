/*
 *			S H A D E W O R K . H
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 *  $Header$
 */

#define SW_NLIGHTS	16		/* Max # of light sources */

/*
 *			S H A D E W O R K
 */
struct shadework {
/* XXX At least the first three of these need to be spectral curves */
	fastf_t		sw_transmit;	/* 0.0 -> 1.0 */
	fastf_t		sw_reflect;	/* 0.0 -> 1.0 */
	fastf_t		sw_extinction;	/* extinction coeff, mm^-1 */
	fastf_t		sw_refrac_index;
#if RT_MULTISPECTRAL
	struct rt_tabdata *msw_color;
	struct rt_tabdata *msw_basecolor;
#else
	fastf_t		sw_color[3];	/* shaded color */
	fastf_t		sw_basecolor[3]; /* base color */
#endif
	struct hit	sw_hit;		/* ray hit (dist,point,normal) */
	struct uvcoord	sw_uv;
#if RT_MULTISPECTRAL
	struct rt_tabdata *msw_intensity[SW_NLIGHTS];
#else
	fastf_t		sw_intensity[3*SW_NLIGHTS]; /* light intensities */
#endif
	fastf_t		sw_tolight[3*SW_NLIGHTS];   /* light directions */
	char		*sw_visible[SW_NLIGHTS]; /* visibility flags/ptrs */
	int		sw_xmitonly;	/* flag: need sw_transmit only */
	int		sw_inputs;	/* fields from mf_inputs actually filled */
	int		sw_frame;	/* # of current frame */
	fastf_t		sw_frametime;	/* frame time delta off 1st frame */
	fastf_t		sw_pixeltime;	/* pixel time delta off 1st pixel of 1st frame */
	struct seg	*sw_segs;	/* segs which made partition */
};

BU_EXTERN(void		pr_shadework, (CONST char *str, CONST struct shadework *swp));
