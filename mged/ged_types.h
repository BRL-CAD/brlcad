/*	SCCSID	%W%	%E%	*/
/*
	ged_types -- data type declarations for GED
*/

/*
 * type definition for new "C" declaration:  "homog_t".
 *
 * This is to make declaring Homogeneous Transform matrices easier.
 */
typedef	float	mat_t[4*4];
typedef	float	*matp_t;
typedef	float	vect_t[4];
typedef	float	*vectp_t;

#ifndef X
#define	X	0
#define	Y	1
#define Z	2
#define H	3
#endif

#define ALPHA	0
#define BETA	1
#define GAMMA	2
