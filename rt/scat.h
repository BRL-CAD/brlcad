/*
 *  GIFT/SRIM Radfile record format
 *
 *  Five kinds of records, each 72 (18*4) bytes long consisting of
 *  a four character identifier and 17 single precision (32bits) floats.
 *
 * 	Header		"head"	followed by two additional records
 *	Firing		"fire"	one per ray fired (if it intersected)
 *	Reflection	"refl"	one per intersection point
 *	Escape		"escp"	one per escaped ray (after last intersection)
 *	End
 */

#define	MAXREFLECT	16
#define	DEFAULTREFLECT	16

static struct rayinfo {
	int	sight;
	int	x,y;		/* x y corrdintaes of the grid */
	vect_t	ip;		/* intersection point */
	vect_t	norm;		/* normal vector */
	vect_t	spec;		/* specular direction */
	struct	curvature curvature;
	fastf_t	dist;
	vect_t	dir;		/* incoming direction vector */
	int	reg, sol, surf;
};

struct physics {
	int cpu;
	int rflt_ptr;
	fastf_t cdist;
	fastf_t ampl[2];
	fastf_t phase[2];
	vect_t pvec1;
	vect_t pvec2;
	vect_t dircv;
	fastf_t rcvhph;
	fastf_t rcvvph;
	fastf_t hrcv;
	fastf_t vrcv;
	int caustic;
	fastf_t r1;
	fastf_t r2;
	vect_t anext;
	vect_t bnext;
};


/* radar specific values */
extern fastf_t r_reflections;	/* Number of maximum reflections */
extern fastf_t wavelength;	/* Radar wavelength */
extern fastf_t xhpol;	/* Transmitter vertical polarization */
extern fastf_t xvpol;	/* Transmitter horizontal polarization */ 
extern fastf_t rhpol;	/* Receiver vertical polarization */
extern fastf_t rvpol;	/* Receiver horizontal polarization */
extern vect_t uhoriz;	/* horizontal emanation plane unit vector. */
extern vect_t unorml;	/* normal unit vector to emanation plane. */
extern vect_t cemant;	/* center vector of emanation plane. */
extern vect_t uvertp;	/* vertical emanation plane unit vector. */
extern fastf_t epsilon;
extern fastf_t totali;
extern fastf_t totalq;

extern struct rayinfo rayinfo[MAX_PSW][MAXREFLECT];

#undef M_PI

#define M_PI 3.14159265
