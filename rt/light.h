/*
 *			L I G H T . H
 */
struct light_specific {
	/* User-specified fields */
	fastf_t	lt_intensity;	/* Intensity Lumens (cd*sr): total output */
	fastf_t	lt_angle;	/* beam dispersion angle (degrees) 0..180 */
	fastf_t	lt_fraction;	/* fraction of total light */
	int	lt_shadows;	/* !0 if this light casts shadows */
	int	lt_infinite;	/* !0 if infinitely distant */
	int	lt_implicit;	/* !0 if implicitly modeled or invisible */
	/* Internal fields */
	vect_t	lt_color;	/* RGB, as 0..1 */
	fastf_t	lt_radius;	/* approximate radius of spherical light */
	fastf_t	lt_cosangle;	/* cos of lt_angle */
	vect_t	lt_pos;		/* location in space of light */
	vect_t	lt_vec;		/* Unit vector from origin to light */
	vect_t	lt_aim;		/* Unit vector - light beam direction */
	char	*lt_name;	/* identifying string */
	struct	region *lt_rp;	/* our region of origin */
	struct	light_specific *lt_forw;	/* Forward link */
};
#define LIGHT_NULL	((struct light_specific *)0)
