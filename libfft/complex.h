/* The COMPLEX type used throughout */
typedef struct {
	double	re;	/* Real Part */
	double	im;	/* Imaginary Part */
} COMPLEX;

#define	CMAG(c)	(hypot( c.re, c.im ))

/* Sometimes found in <math.h> */
#if !defined(PI)
#	define	PI	3.141592653589793238462643
#endif

#define	TWOPI	6.283185307179586476925286

/* Degree <-> Radian conversions */
#define	RtoD(x)	((x)*57.29577951308232157827)
#define	DtoR(x) ((x)*0.01745329251994329555)
