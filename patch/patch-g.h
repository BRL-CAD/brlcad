struct input {

	fastf_t x, y, z;
	fastf_t	rsurf_thick;
	int	surf_type;
	int	surf_thick;
	int	spacecode;
	int	cc;
	int  	ept[10];
	int	mirror;
	int	vc;

	int	prevsurf_type;
	char	surf_mode;

} in[10000];

struct patch_verts {
        struct vertex *vp;
        point_t coord;
};

struct patch_faces
{
        struct faceuse *fu;
        fastf_t thick;
};

struct patches{

	fastf_t x,y,z;
	int flag;
	fastf_t radius;
	int mirror;
	fastf_t thick;

};

struct names{

	char	ug[16];
	char	lg[16];
	int	eqlos,
		matcode;

} nm[9999];

struct subtract_list{
	
	int			outsolid,
				insolid,
				inmirror;
	struct subtract_list	*next;
};

point_t		pt[4];
fastf_t		vertice[5][3];
fastf_t		first[5][3];
fastf_t		normal[5][3];
point_t		ce[4];
point_t		centroid,Centroid;	/* object, description centroids */
unsigned char	rgb[3];
int debug = 0;
float mmtin = 25.4;
double conv_mm2in;
fastf_t third = 0.333333333;

/* char  name[17];	*/
char  cname[17];
char  tname[17];
char  surf[2];
char thick[3];
char  space[2];

int numobj = 0;
int nflg = 1;
int aflg = 1;		/* use phantom armor */
int num_unions = 5;	/* number of unions per region */
char *title = "Untitled MGED database";	/* database title */
char *top_level = "all"; /* top-level node name in the database */
int rev_norms = 0;	/* reverse normals for plate mode triangles */
int polysolid = 0;	/* convert triangle-facetted objects to polysolids */

mat_t	m;
char *patchfile;
char *labelfile=NULL;
char *matfile;

#define MAX_THICKNESSES		500	/* Maximum number of different thicknesses
					   for a single plate mode solid */
fastf_t thicks[MAX_THICKNESSES];	/* array of unique plate thicknesses */
int nthicks;				/* number of unique plate thicknesses
					   for a single plate mode solid */

struct patches list[15000];
fastf_t x[1500],y[1500],z[1500];
int mirror[1500];
fastf_t radius[1500];
fastf_t thk[1500];

struct wmember head;			/* solids for current region */
struct wmember heada;			/* for component,regions on one side */
struct wmember headb;			/* for component,mirror regions */
struct wmember headc;			/* second level grouping ? */
struct wmember headd;			/* current thousand series group */
struct wmember heade;			/* group containing everything */
struct wmember headf;			/* check solids group */
