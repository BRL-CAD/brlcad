/*
 *  			C V . C
 *  
 *  Converts .vg files to .g (latest style, with idents).
 *  
 *  Quick hack to get underway with the new GED.
 *  
 *  Mike Muuss, BRL, 5/25/84.
 */
#include <stdio.h>
#include "ged_types.h"
#include "3d.h"
union record rec;

char line[256];

main(argc, argv)
int argc;
char *argv[];
{
	static int ifd, ofd;
	static int units;
	static int count;

	if( argc != 3 )  {
		printf("Usage: cv file.vg file.g\n");
		exit(11);
	}
	if( (ifd = open( argv[1], 0 )) < 0 )  {
		perror(argv[1]);
		exit(12);
	}
	if( (ofd = creat(argv[2], 0664)) < 0 )  {
		perror(argv[2]);
		exit(13);
	}
	rec.i.i_id = ID_IDENT;
	strcpy( rec.i.i_version, ID_VERSION );
	rec.i.i_units = 100;
	while( rec.i.i_units < ID_NO_UNIT || rec.i.i_units > ID_FT_UNIT )  {
		printf("Units: 0=none, 1=mm, 2=cm, 3=meters, 4=inches, 5=feet\nUnits? ");
		gets( line );
		sscanf( line, "%d", &units );
		rec.i.i_units = units;
		printf("units=%d\n", rec.i.i_units);
	}
	printf("Title? "); fflush(stdout);
	gets( line );
	strncpy( rec.i.i_title, line, 71 );
	rec.i.i_title[71] = '\0';
	printf("Title=%s\n", rec.i.i_title );
	write( ofd, &rec, sizeof rec );
	count = 1;

top:
	while( read( ifd, &rec, sizeof rec ) == sizeof rec )  {
		switch( rec.u_id )  {

		case ID_IDENT:
			printf("Unexpected ID record encountered in input\n");
			printf("Title=%s\n", rec.i.i_title );
			break;
		case ID_FREE:
			goto top;
		case ID_SOLID:
			if( rec.s.s_name[0] == '\0' )
				goto top;
			break;
		case ID_COMB:
			if( rec.c.c_name[0] == '\0' )  {
				/* Skip deleted junk */
				lseek( ifd, (long)rec.c.c_length *
					(long)(sizeof rec), 1 );
				goto top;
			}
			break;
		case ID_ARS_B:
			/* No changes */
			break;
		case ID_ARS_A:
			if( rec.a.a_name[0] == '\0' )  {
				/* Skip deleted junk */
				lseek( ifd, (long)(rec.a.a_totlen) *
					(long)(sizeof rec), 1 );
				goto top;
			}
			break;
		case ID_P_HEAD:
		case ID_P_DATA:
			break;

		default:
			printf("Garbage record in database\n");
			exit(42);
		case ID_MEMB:
			/* Zap rotation part, flip scale */
#define m rec.M.m_mat
/*mat_pr("before", m); */
			m[0] = 1; m[1] = 0; m[2] = 0;
			m[4] = 0; m[5] = 1; m[6] = 0;
			m[8] = 0; m[9] = 0; m[10] = 1;

			/* m[15] we leave alone */
/*mat_pr("after", m); */
			break;
		}
		write( ofd, &rec, sizeof(rec) );
		count++;
	}
	printf("%d database granules written\n", count);
}

mat_pr( title, mp )
char *title;
float *mp;
{
	register int i;

	printf("MATRIX %s:\n  ", title);
	for(i=0; i<16; i++)  {
		printf(" %8.3f", mp[i]);
		if( (i&3) == 3 ) printf("\n  ");
	}
}
