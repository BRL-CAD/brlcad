/*
 *			L I G H T . H
 */
struct light_specific {
	/* User-specified fields */
	fastf_t	lt_intensity;	/* Intensity Lumens (cd*sr): total output */
	/* Internal fields */
	vect_t	lt_color;	/* RGB, as 0..1 */
	fastf_t	lt_fraction;	/* fraction of light */
	fastf_t	lt_radius;	/* approximate radius of spherical light */
	vect_t	lt_pos;		/* location in space of light */
	vect_t	lt_vec;		/* Unit vector from origin to light */
	struct light_specific *lt_forw;	/* Forward link */
	int	lt_explicit;	/* set !0 if explicitly modeled */
};
#define LIGHT_NULL	((struct light_specific *)0)
