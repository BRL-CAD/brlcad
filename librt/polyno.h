/*
	<polyno.h> -- definitions for handling polynomial equations
*/

/* reasonable maximum size for most uses:	*/
#define		MAXP			9

/* error return value for 'polyMul'		*/
#define		PM_NULL			(poly *)0

/* polynomial data type:			*/
typedef  struct
	{
	int		dgr;
	double		cf[MAXP];
	}  poly;

#define	polyAllo()		((poly *)malloc( sizeof(poly) ))

extern char	*malloc();


/* library functions:				*/
extern poly	*polyAdd(), *polySub(), *polyMul(), *polyScal();
extern void	quadratic(), synDiv(), prntpoly();
