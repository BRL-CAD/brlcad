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
#define DEBUG_NAMES 1
#define DEBUG_STATS 2
#define DEBUG_ONLY_TRI 4
#define DEBUG_QUAD 8
#define DEBUG_BOTS 0x10
long debug = 0;
int verbose = 0;

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


static int tbl[19][8] = {
	{ 0, 1, 3, 4,	1, 2, 0, 5},
	{ 0, 1, 4, 3,	1, 2, 0, 5},
	{ 0, 1, 3, 5,	1, 2, 0, 4},
	{ 0, 1, 5, 3,	1, 2, 0, 4},
	{ 0, 1, 5, 4,	1, 2, 0, 3},
	{ 0, 1, 4, 5,	1, 2, 0, 3},
	{ 0, 2, 3, 4,	0, 1, 2, 5},
	{ 0, 2, 4, 3,	0, 1, 2, 5},
	{ 0, 2, 3, 5,	0, 1, 2, 4},
	{ 0, 2, 5, 3,	0, 1, 2, 4},
	{ 0, 2, 4, 5,	0, 1, 2, 3},
	{ 0, 2, 5, 4,	0, 1, 2, 3},
	{ 1, 2, 3, 4,	0, 1, 5, 2},
	{ 1, 2, 4, 3,	0, 1, 5, 2},
	{ 1, 2, 3, 5,	0, 1, 4, 2},
	{ 1, 2, 5, 3,	0, 1, 4, 2},
	{ 1, 2, 4, 5,	0, 1, 3, 2},
	{ 1, 2, 5, 4,	0, 1, 3, 2}
};

/*
 *	Compare two successive triangles to see if they are coplanar and
 *	share two verticies.
 *
 */
int
tris_are_planar_quad(struct rt_bot_internal *bot, int faceidx, int vidx[4])
{
    int *vnum = &bot->faces[faceidx*3];  /* get the individual face */
    point_t A, B, C, N1, N2;
    vect_t vAB, vAC;
    fastf_t *v = bot->vertices;
    int i, tmp;

    RT_BOT_CK_MAGIC(bot);

    if (debug&DEBUG_ONLY_TRI) return 0;

    if (faceidx >= bot->num_faces-1) return 0;

    if (debug&DEBUG_QUAD) {
	fprintf(stderr, "tris_are_planar_quad()\n");
	for (i=0 ; i < 6 ;i++) {
	    fprintf(stderr, "%d: %d   %7g %7g %7g\n", i, vnum[i], 
		    V3ARGS(&v[3* vnum[i]]));
	}
    }
    /* compare the surface normals */

    /* if the first vertex is greater than the number of verticies
     * this is probably a bogus face, and something bad has happened
     */
    if (vnum[0] > bot->num_vertices-1) {
	fprintf(stderr, "Bounds error %d > %d\n",
		vnum[0], bot->num_vertices-1);	abort();
    }


    /* look to see if the normals are within tolerance */

    VMOVE(A, &v[3*vnum[0]]);
    VMOVE(B, &v[3*vnum[1]]);
    VMOVE(C, &v[3*vnum[2]]);
    
    VSUB2(vAB, B, A);
    VSUB2(vAC, C, A);
    VCROSS(N1, vAB, vAC);
    VUNITIZE(N1);

    VMOVE(A, &v[3*vnum[3]]);
    VMOVE(B, &v[3*vnum[4]]);
    VMOVE(C, &v[3*vnum[5]]);
    
    VSUB2(vAB, B, A);
    VSUB2(vAC, C, A);
    VCROSS(N2, vAB, vAC);
    VUNITIZE(N2);


    if (debug&DEBUG_QUAD) {
	VPRINT("N1", N1);
	VPRINT("N2", N2);
    }

    /* if the normals are out of tolerance, simply give up */
    if ( ! VAPPROXEQUAL(N1, N2, 0.005)) {
	if (debug&DEBUG_QUAD)
	    fprintf(stderr, "normals don't match  %g %g %g   %g %g %g\n",
		    V3ARGS(N1), V3ARGS(N2));

	return 0;
    }

    if (debug&DEBUG_QUAD) {
	fprintf(stderr, "numv %d\n", bot->num_vertices);
	fprintf(stderr, "possible quad %d %d %d  %d %d %d\n",
		vnum[0], vnum[1], vnum[2], vnum[3], vnum[4], vnum[5]);
    }

    /* find adjacent/matching verticies */

    for (i=0 ; i < 18 ; i++) {
	int *t;

	t = &tbl[i][0];

	if (vnum[t[0]] == vnum[t[2]] && vnum[t[1]] == vnum[t[3]]) {
	    if (debug&DEBUG_QUAD)
		fprintf(stderr, "matched %d,%d and %d,%d,  (%d/%d %d/%d)\n",
			vnum[t[0]], vnum[t[2]], vnum[t[1]], vnum[t[3]],
			t[0], t[2], t[1], t[3]);

	    vidx[0] = vnum[t[4]];
	    vidx[1] = vnum[t[5]];
	    vidx[2] = vnum[t[6]];
	    vidx[3] = vnum[t[7]];
	    if (debug&DEBUG_QUAD) {
		fprintf(stderr, "%d %d %d %d\n", V4ARGS(vidx));
		for (tmp=0 ; tmp < 4 ; tmp++)
		    fprintf(stderr, "%d vidx:%d %7g %7g %7g\n", tmp, vidx[tmp],
			    V3ARGS(&v[3*vidx[tmp]]));
	    }
	    return 1;
	}
    }
    /* No match, so we print both triangles */
    return 0;
}
int count_quad_faces(struct rt_bot_internal *bot)
{
    int i;
    int count = 0;
    int vidx[4];

    for (i=0 ; i < bot->num_faces ; i++) {
	if (tris_are_planar_quad(bot, i, vidx))
	    ++count;
    }

    if (debug&DEBUG_STATS)
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
    if (debug&DEBUG_NAMES) fprintf(stderr, "Writing DXF: %s\n",Value);
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


    if (debug&DEBUG_STATS)
	fprintf(stderr, "writing %d verticies\n",num_vertices);
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

    if (debug&DEBUG_STATS)
	fprintf(stderr, "writing %d faces (- %d for quads)\n", num_faces, quads);
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
	if (  tris_are_planar_quad(bot, i, vidx) ) {
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

int r_start (
		struct db_tree_state * tsp,
		struct db_full_path * pathp,
		const struct rt_comb_internal * combp,
		genptr_t client_data )
{
    int i;
    if (debug&DEBUG_NAMES) {
	bu_log("r_start %d ", ((struct rt_bot_internal *)client_data)->num_vertices);
	for (i=0 ; i < pathp->fp_len ; i++) {
	    if (pathp->fp_len - (i+1)) {
		bu_log("%s/", pathp->fp_names[i]->d_namep);
	    } else {
		bu_log("%s\n", pathp->fp_names[i]->d_namep);
	    }
	}
    }
    return 0;
}

union tree *r_end (
		struct db_tree_state * tsp,
		struct db_full_path * pathp,
		union tree * curtree,
		genptr_t client_data )
{
    int i;
    if (debug&DEBUG_NAMES) {
	bu_log("r_end %d ", ((struct rt_bot_internal *)client_data)->num_vertices);
	for (i=0 ; i < pathp->fp_len ; i++) {
	    if (pathp->fp_len - (i+1)) {
		bu_log("%s/", pathp->fp_names[i]->d_namep);
	    } else {
		bu_log("%s\n", pathp->fp_names[i]->d_namep);
	    }
	}
    }

    return curtree;
}

void add_bots(struct rt_bot_internal *bot_dest, 
	      struct rt_bot_internal *bot_src) 
{
    int i = bot_dest->num_vertices + bot_src->num_vertices;
    int sz = sizeof(fastf_t) * 3;
    int *ptr;

    if (debug&DEBUG_BOTS)
    bu_log("adding bots v:%d f:%d  v:%d f:%d\n",
	   bot_dest->num_vertices, bot_dest->num_faces,
	   bot_src->num_vertices, bot_src->num_faces);

    /* allocate space for extra vertices */
    bot_dest->vertices = 
	bu_realloc(bot_dest->vertices, i * sz, "new vertices");

    /* copy new vertices */
    memcpy(&bot_dest->vertices[bot_dest->num_vertices],
	   bot_src->vertices, 
	   bot_src->num_vertices * sz);

    /* allocate space for new faces */
    i = bot_dest->num_faces + bot_src->num_faces;
    sz = sizeof(int) * 3;
    bot_dest->faces = bu_realloc(bot_dest->faces, i * sz, "new faces");
    
    /* copy new faces, making sure that we update the vertex indices to
     * point to their new locations
     */
    ptr = &bot_src->faces[bot_src->num_faces*3];
    int limit = bot_src->num_faces * 3;
    for (i=0 ; i < limit ; i++)
	ptr[i] = bot_src->faces[i] + bot_dest->num_vertices;

    bot_dest->num_vertices += bot_src->num_vertices;
    bot_dest->num_faces += bot_src->num_faces;

    if (debug&DEBUG_BOTS)
    bu_log("...new bot v:%d f:%d\n",
	   bot_dest->num_vertices, bot_dest->num_faces);
}

union tree * l_func (
		struct db_tree_state * tsp,
		struct db_full_path * pathp,
		struct rt_db_internal * ip,
		genptr_t client_data )
{
    int i;
    struct rt_bot_internal *bot;

    if (debug&DEBUG_NAMES) {
	for (i=0 ; i < pathp->fp_len ; i++) {
	    if (pathp->fp_len - (i+1)) {
		bu_log("%s/", pathp->fp_names[i]->d_namep);
	    } else {
		bu_log("%s", pathp->fp_names[i]->d_namep);
	    }
	}
    }


    if (ip->idb_minor_type != ID_BOT) {
	if (debug&DEBUG_NAMES)
	    bu_log(" is not a bot\n");
	return (union tree *)NULL;
    }
    if (debug&DEBUG_NAMES)
	bu_log("\n");

    bot = ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot);
	
    add_bots((struct rt_bot_internal *)client_data, bot);
    return (union tree *)NULL;
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
    struct directory *dp;

    arg_count = parse_args(ac, av);
	
    if ((ac - arg_count) < 1) {
	fprintf(stderr, "usage: %s geom.g [file.dxf] [bot1 bot2 bot3...]\n", progname);
	exit(-1);
    }

    RT_INIT_DB_INTERNAL(&intern);

    if ((rtip=rt_dirbuild(av[arg_count], idbuf, sizeof(idbuf)))==RTI_NULL){
	fprintf(stderr,"rtexample: rt_dirbuild failure\n");
	exit(2);
    }

    arg_count++;

    /* process command line objects */
    if (arg_count < ac) {
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

	    if (intern.idb_minor_type == ID_BOT) {
		/* write a single file */
		bot = (struct rt_bot_internal *)intern.idb_ptr;
		write_dxf(bot, av[arg_count]);
	    } else {
		/* user has asked for a tree.  Write 1 DXF file for 
		 * all the bots in that tree.
		 */
		char **sub_av;
		struct db_tree_state init_state = rt_initial_tree_state;
		struct rt_bot_internal my_bot;

		my_bot.vertices = (fastf_t *)NULL;
		my_bot.faces = (int *)NULL;
		my_bot.num_vertices = 0;
		my_bot.num_faces = 0;
		my_bot.magic = RT_BOT_INTERNAL_MAGIC;
		RT_BOT_CK_MAGIC(&my_bot);

		sub_av = &dirp->d_namep;

		db_walk_tree(rtip->rti_dbip, 1, (const char **)sub_av, 1,
			     &init_state, r_start, r_end, l_func, &my_bot);

		RT_BOT_CK_MAGIC(&my_bot);
		write_dxf(&my_bot, av[arg_count]);
		continue;
	    }
	}
    } else {
	mat_t mat;
	int i;

	mat_idn(mat);

	/* dump all the bots */
	FOR_ALL_DIRECTORY_START(dp, rtip->rti_dbip)

        /* we only dump BOT primitives, so skip some obvious exceptions */
        if (dp->d_major_type != DB5_MAJORTYPE_BRLCAD) continue;
	if (dp->d_flags & DIR_COMB) continue;

	if (debug&DEBUG_NAMES)
	    fprintf(stderr, "%s\n", dp->d_namep);

	/* get the internal form */
	i=rt_db_get_internal(&intern, dp, rtip->rti_dbip, mat,&rt_uniresource);

	if (i < 0) {
	    fprintf(stderr, "rt_get_internal failure %d on %s\n", i, dp->d_namep);
	    continue;
	}
	
	if (i != ID_BOT) {
	    if (debug&DEBUG_NAMES)
		fprintf(stderr, "skipping \"%s\" (not a BOT)\n", dp->d_namep);
	    continue;
	}

	bot = (struct rt_bot_internal *)intern.idb_ptr;

	write_dxf(bot, dp->d_namep);
	    
FOR_ALL_DIRECTORY_END
    }
    return 0;
}


