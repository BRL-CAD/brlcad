#ifndef plastic_h
#define plastic_h

#define PL_MAGIC        0xbeef00d
#define PL_NULL ((struct phong_specific *)0)
#define PL_O(m) offsetof(struct phong_specific, m)

/* Local information */
struct phong_specific {
	int	magic;
	int	shine;
	double	wgt_specular;
	double	wgt_diffuse;
	double	transmit;       /* Moss "transparency" */
	double	reflect;        /* Moss "transmission" */
	double	refrac_index;
	double	extinction;
	double	emission[3];
	struct	mfuncs *mfp;
};

extern struct bu_structparse phong_parse[];
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
