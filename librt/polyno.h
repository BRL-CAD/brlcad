/*
	<polyno.h> -- definitions for handling polynomial equations
*/

#define		MAXP			4
#define 	SMALL			.00001
#define		Abs( a )		((a) >= 0 ? (a) : -(a))
#define 	Max( a, b )		((a) > (b) ? (a) : (b))

/* error return value for 'polyMul'		*/
#define		PM_NULL			(poly *)0

/* polynomial data type:			*/
typedef  struct {
	int		dgr;
	double		cf[MAXP+1];
}  poly;


/* library functions:				*/
extern poly	*polyAdd(), *polySub(), *polyMul(), *polyScal();
extern void	quadratic(), cubic(), quartic(), synDiv(), prntpoly();
