/*
 *	Options
 *	h	help
 */
#include <stdio.h>
#include <string.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "bu.h"
#include "raytrace.h"
#include "wdb.h"
#include "../librt/debug.h"

/* declarations to support use of getopt() system call */
char *options = "hd:";
extern char *optarg;
extern int optind, opterr, getopt();

char *progname = "(noname)";
#define DEBUG_QUAD 8
long debug = 0;
/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void usage(char *s)
{
    if (s) (void)fputs(s, stderr);

    (void) fprintf(stderr, "Usage: %s [ -%s ] [<] infile [> outfile]\n",
		   progname, options);
    exit(1);
}

/*
 *	P A R S E _ A R G S --- Parse through command line flags
 */
int parse_args(int ac, char *av[])
{
    int  c;
    char *strrchr();

    if (  ! (progname=strrchr(*av, '/'))  )
	progname = *av;
    else
	++progname;

    /* Turn off getopt's error messages */
    opterr = 0;

    /* get all the option flags from the command line */
    while ((c=getopt(ac,av,options)) != EOF)
	switch (c) {
	case 'd'	: debug = strtol(optarg, NULL, 16); break;
	case '?'	:
	case 'h'	:
	default		: usage("Bad or help flag specified\n"); break;
	}

    return(optind);
}


/*
 *	Compare two successive triangles to see if they are coplanar and
 *	share two verticies.
 *
 */
int
tris_are_planar_quad(struct rt_bot_internal *bot, int faceidx, int idx[4])
{
    int *f = &bot->faces[faceidx*3];  /* get the individual face */
    point_t A, B, C, N1, N2;
    vect_t vAB, vAC;
    fastf_t *v = bot->vertices;

    if (faceidx >= bot->num_faces-1) return 0;


    /* compare the surface normals */

    /* if the first vertex is greater than the number of verticies
     * this is probably a bogus face, and something bad has happened
     */
    if (f[0] > bot->num_vertices-1) {
	fprintf(stderr, "Bounds error\n");
	abort();
    }


    /* look to see if the normals are within tolerance */

    VMOVE(A, &v[f[0]]);
    VMOVE(B, &v[f[1]]);
    VMOVE(C, &v[f[2]]);
    
    VSUB2(vAB, B, A);
    VSUB2(vAC, C, A);

    VCROSS(N1, vAB, vAC);
    VUNITIZE(N1);


    VMOVE(A, &v[f[3]]);
    VMOVE(B, &v[f[4]]);
    VMOVE(C, &v[f[5]]);
    
    VSUB2(vAB, B, A);
    VSUB2(vAC, C, A);

    VCROSS(N2, vAB, vAC);
    VUNITIZE(N2);

    /* if the normals are out of tolerance, simply give up */
    if ( ! VAPPROXEQUAL(N1, N2, 0.005)) return 0;

    if (debug&DEBUG_QUAD) {
	fprintf(stderr, "numv %d\n", bot->num_vertices);
	fprintf(stderr, "possible quad %d %d %d  %d %d %d\n",
		f[0], f[1], f[2], f[3], f[4], f[5]);
    }

    /* find adjacent/matching verticies */
    for (i=0 ; i < 2 ; i++) {
	for (j=3 ; j < 6 ; j++) {
	    if (f[i] == f[j]) {
		/* found 1 vertex match */
		
		for (s=i+1 ; s < 3 ; s++) {
		    for (t=3 ; t < 6 ; t++) {
			if (f[s] == f[t]) {
			    /* double match (assuming no repeated verticies */

			    /*
			     * 0 1  3 4
			     * 0 1  4 3
			     * 0 1  3 5
			     * 0 1  5 3
			     * 0 1  5 4
			     * 0 1  4 5

			     * 0 2  3 4
			     * 0 2  4 3
			     * 0 2  3 5
			     * 0 2  5 3
			     * 0 2  4 5
			     * 0 2  5 4
			     *
			     * 1 2  3 4
			     * 1 2  4 3
			     * 1 2  3 5
			     * 1 2  5 3
			     * 1 2  4 5
			     * 1 2  5 4
			     *
			     */

			}
		    }
		}
	    }
	}
    } 

    if (f[0] == f[5]) {
    } else if (f[1]

    if (f[0] == f[5] && f[2] == f[3]) {
	idx[0] = f[0];
	idx[1] = f[1];
	idx[2] = f[2];
	idx[3] = f[4];
	if (debug&DEBUG_QUAD)
	    fprintf(stderr, "quad %d %d %d %d\n", V4ARGS(idx));
	return 1;
    }


    /* if we don't have matching vertices we're done */
    return 0;


}
int count_quad_faces(struct rt_bot_internal *bot)
{
    int i;
    int count = 0;
    int vidx[4];

    for (i=0 ; i < bot->num_faces ; i++) {
	count += tris_are_planar_quad(bot, i, vidx);
    }

    fprintf(stderr, "%d quads\n", count);
    return count;
}


void write_dxf(struct rt_bot_internal *bot, char *name)

{
    int num_vertices;
    fastf_t *vertices;
    int num_faces;
    int *faces;

    FILE		*FH;
    int		i;
    char		Value[32];
    int quads;
    int vidx[4];

    num_vertices = bot->num_vertices;
    vertices = bot->vertices;
    num_faces = bot->num_faces;
    faces = bot->faces;



    sprintf(Value,"%s.dxf",name);
    printf("Writing DXF: %s\n",Value);
    FH= fopen(Value,"w");


    /* Write Header */
    fprintf(FH, "0\nSECTION\n2\nHEADER\n0\nENDSEC\n0\nSECTION\n2\n");
    fprintf(FH, "BLOCKS\n0\n");


    /* Write Object Data */
    fprintf(FH, "BLOCK\n2\n%s\n8\nMeshes\n", name);
    fprintf(FH, "70\n");
    fprintf(FH, "64\n");
    fprintf(FH, "10\n0.0\n");
    fprintf(FH, "20\n0.0\n");
    fprintf(FH, "30\n0.0\n");
    fprintf(FH, "3\n");
    sprintf(Value,"%s\n",name);
    fprintf(FH, Value);
    fprintf(FH, "0\n");
    fprintf(FH, "POLYLINE\n");
    fprintf(FH, "66\n");
    fprintf(FH, "1\n");
    fprintf(FH, "8\n");

    fprintf(FH, "Meshes\n");  /* polyline polyface meshes */
    fprintf(FH, "62\n");
    fprintf(FH, "254\n");
    fprintf(FH, "70\n");
    fprintf(FH, "64\n");
    fprintf(FH, "71\n"); /* number of verticies */

    fprintf(FH, "%d\n",num_vertices);

    quads = count_quad_faces(bot);

    fprintf(FH, "72\n"); /* number of faces */
    fprintf(FH, "%d\n",num_faces-quads);
    fprintf(FH, "0\n");


    printf("writing %d verticies\n",num_vertices);
    for (i= 0; i < num_vertices; i++) {
	fprintf(FH, "VERTEX\n");
	fprintf(FH, "8\n");
	fprintf(FH, "Meshes\n");
	fprintf(FH, "10\n");
	fprintf(FH, "%.6f\n",vertices[3*i+0]/1000.0);

	fprintf(FH, "20\n");
	fprintf(FH, "%.6f\n",vertices[3*i+1]/1000.0);

	fprintf(FH, "30\n");
	fprintf(FH, "%.6f\n",vertices[3*i+2]/1000.0);
	fprintf(FH, "70\n");
	fprintf(FH, "192\n");
	fprintf(FH, "0\n");
	/*printf("\t%g %g %g\n",vertices[3*i+0],vertices[3*i+1],vertices[3*i+2]);*/
    }


    printf("writing %d faces (- %d for quads)\n", num_faces, quads);
    for (i= 0; i < num_faces; i++) {

	fprintf(FH, "VERTEX\n");
	fprintf(FH, "8\n");
	fprintf(FH, "Meshes\n");


	/* polyline flags, bitwise OR of following values:
	 *  8 = 3D polyline
	 *  16 = 3D polygon mesh
	 *  32 = polygon mesh closed in the N direction
	 */
	fprintf(FH, "56\n");  

	fprintf(FH, "254\n");
	fprintf(FH, "10\n0.0\n20\n0.0\n30\n0.0\n"); /* WCS origin */
	fprintf(FH, "70\n128\n");/* line type pattern is continuous */
	if (tris_are_planar_quad(bot, i, vidx)) {
	    fprintf(FH, "71\n%d\n",vidx[0]+1); /* vertex 1 */
	    fprintf(FH, "72\n%d\n",vidx[1]+1); /* vertex 2 */
	    fprintf(FH, "73\n%d\n",vidx[2]+1); /* vertex 3 */
	    fprintf(FH, "74\n%d\n",vidx[3]+1); /* vertex 4 */
	    i++;
	} else {
	    fprintf(FH, "71\n%d\n",faces[i*3+0]+1); /* vertex 1 */
	    fprintf(FH, "72\n%d\n",faces[i*3+1]+1); /* vertex 2 */
	    fprintf(FH, "73\n%d\n",faces[i*3+2]+1); /* vertex 3 */
	}
	fprintf(FH, "0\n");
    }


    fprintf(FH, "SEQEND\n");
    fprintf(FH, "0\n");
    fprintf(FH, "ENDBLK\n");
    fprintf(FH, "0\n");

    fprintf(FH, "ENDSEC\n");
    fprintf(FH, "0\n");
    fprintf(FH, "SECTION\n");
    fprintf(FH, "2\n");
    fprintf(FH, "ENTITIES\n");
    fprintf(FH, "0\n");

    fprintf(FH, "INSERT\n");
    fprintf(FH, "8\n");
    fprintf(FH, "1\n");
    fprintf(FH, "2\n");
    sprintf(Value,"%s\n",name);
    fprintf(FH, Value);
    fprintf(FH, "10\n");
    sprintf(Value,"%.6f\n",0.0);
    fprintf(FH, Value);
    fprintf(FH, "20\n");
    sprintf(Value,"%.6f\n",0.0);
    fprintf(FH, Value);
    fprintf(FH, "30\n");
    sprintf(Value,"%.6f\n",0.0);
    fprintf(FH, Value);
    fprintf(FH, "41\n");
    fprintf(FH, "1.000000\n");
    fprintf(FH, "42\n");
    fprintf(FH, "1.000000\n");
    fprintf(FH, "43\n");
    fprintf(FH, "1.000000\n");
    fprintf(FH, "50\n");
    fprintf(FH, "0.000000\n");
    fprintf(FH, "0\n");

    fprintf(FH, "ENDSEC\n");
    fprintf(FH, "0\n");
    fprintf(FH, "EOF\n");
    fclose(FH);
}


/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int main(int ac, char *av[])
{
    int arg_count;
    char idbuf[132];		/* First ID record info */
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    struct rt_i *rtip;

    arg_count = parse_args(ac, av);
	
    if ((ac - arg_count) < 2) {
	fprintf(stderr, "usage: %s geom.g file.dxf bot1 bot2 bot3...\n", progname);
	exit(-1);
    }

    RT_INIT_DB_INTERNAL(&intern);

    if ((rtip=rt_dirbuild(av[arg_count], idbuf, sizeof(idbuf)))==RTI_NULL){
	fprintf(stderr,"rtexample: rt_dirbuild failure\n");
	exit(2);
    }

    arg_count++;

    /* process command line objects */
    for ( ; arg_count < ac ; arg_count++ ) {
	printf("current: %s\n",av[arg_count]);
	struct directory *dirp;

	if (!rt_db_lookup_internal(rtip->rti_dbip, av[arg_count], 
				   &dirp,
				   &intern, 
				   LOOKUP_QUIET,
				   &rt_uniresource)) {
	    fprintf(stderr, "db_lookup failed on %s\n", av[arg_count]);
	    continue;
	}

	if (intern.idb_minor_type != ID_BOT) {
	    fprintf(stderr, "%s is not a BOT\n", av[arg_count]);
	    continue;
	}

	bot = (struct rt_bot_internal *)intern.idb_ptr;

	write_dxf(bot, av[arg_count]);

    }
    return 0;
}


