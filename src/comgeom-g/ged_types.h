/*
 * type definition for new "C" declaration:  "homog_t".
 *
 * This is to make declairing Homogeneous Transform matricies easier.
 */
typedef	float	mat_t[4*4];
typedef	float	*matp_t;
typedef	float	vect_t[4];
typedef	float	*vectp_t;

#define	X	0
#define	Y	1
#define Z	2
#define H	3

#define ALPHA	0
#define BETA	1
#define GAMMA	2

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
