/*
 *			T A B I N T E R P . C
 *
 *  This program is the crucial middle step in the key-frame animation
 *  software.
 *
 *  First, one or more files, on different time scales, are read into
 *  internal "channels".
 *
 *  Secondly, a section of those times is interpolated, and
 *  multi-channel output is produced, on uniform time samples.
 *
 *  This multi-channel output is fed to the next stage to generate
 *  control scripts, etc.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

struct chan {
	int	c_ilen;		/* length of input array */
	char	*c_itag;	/* description of input source */
	fastf_t	*c_itime;	/* input time array */
	fastf_t	*c_ival;	/* input value array */
	fastf_t	*c_oval;	/* output value array */
	int	c_interp;	/* linear or spline? */
#define	INTERP_LINEAR	0
#define	INTERP_SPLINE	1
	int	c_cyclic;	/* cyclic end conditions? */
};

int		o_len;		/* length of all output arrays */
fastf_t		*o_time;	/* pointer to output time array */

int		nchans;		/* number of chan[] elements in use */
int		max_chans;	/* current size of chan[] array */
struct chan	*chan;		/* ptr to array of chan structs */

extern int	cm_file();

struct command_tab cmdtab[] = {
	"file", "filename chan_num(s)", "load channels from file",
		cm_file,	3, 999,
	(char *)0, (char *)0, (char *)0,
		0,		0, 0	/* END */
};

/*
 *			M A I N
 */
main( argc, argv )
int	argc;
char	**argv;
{
	register char	*buf;
	register int	ret;

	/*
	 * All the work happens in the functions
	 * called by rt_do_cmd().
	 */
	while( (buf = rt_read_cmd( stdin )) != (char *)0 )  {
		fprintf(stderr,"cmd: %s\n", buf );
		ret = rt_do_cmd( 0, buf, cmdtab );
		rt_free( buf, "cmd buf" );
		if( ret < 0 )
			break;
	}

	pr_chans();

	exit(0);	
}

/*
 *			C M _ F I L E
 */
int
cm_file( argc, argv )
int	argc;
char	**argv;
{
	FILE	*fp;
	char	*file;
	char	buf[512];
	int	*cnum;
	int	i;
	int	line;
	char	**iwords;
	int	nlines;		/* number of lines in input file */
	int	nwords;		/* number of words on each input line */
	fastf_t	*times;
	auto double	d;

	file = argv[1];
	if( (fp = fopen( file, "r" )) == NULL )  {
		perror( file );
		return(0);
	}

	/* First step, count number of lines in file */
	nlines = 0;
	{
		register int	c;

		while( (c = fgetc(fp)) != EOF )  {
			if( c == '\n' )
				nlines++;
		}
	}
	rewind(fp);

	/* Intermediate dynamic memory */
	cnum = (int *)rt_malloc( argc * sizeof(int), "cnum[]");
	nwords = argc - 1;
	iwords = (char **)rt_malloc( (nwords+1) * sizeof(char *), "iwords[]" );

	/* Retained dynamic memory */
	times = (fastf_t *)rt_malloc( nwords * sizeof(fastf_t), "times");

	/* Now, create & allocate memory for each chan */
	for( i = 1; i < nwords; i++ )  {
		sprintf( buf, "File %s, Column %d", file, i );
		if( (cnum[i] = create_chan( argv[i+1], nlines, buf )) < 0 )
			return(-1);	/* abort */
		/* Share array of times */
		chan[cnum[i]].c_itime = times;
	}

	for( line=0; line < nlines; line++ )  {
		buf[0] = '\0';
		(void)fgets( buf, sizeof(buf), fp );
		i = rt_split_cmd( iwords, nwords+1, buf );
		if( i != nwords )  {
			printf("File %s, Line %s:  expected %d columns, got %d\n",
				file, line, nwords, i );
			while( i < nwords )
				iwords[i++] = (char *)0;
		}
		sscanf( iwords[0], "%lf", &d );
		times[line] = d;
		for( i=1; i < nwords; i++ )  {
			d = 2.34;
			if( sscanf( iwords[i], "%lf", &d ) != 1 )  {
			    	printf("File %s, Line %s:  scanf failure on '%s'\n",
			    		file, line, iwords[i] );
				d = 0.0;
			}
		    	chan[cnum[i]].c_ival[line] = d;
		}
	}
	fclose(fp);

	/* Free intermediate dynamic memory */
	rt_free( (char *)cnum, "cnum[]");
	rt_free( (char *)iwords, "iwords[]");

	return(0);
}

int
create_chan( num, len, itag )
char	*num;
int	len;
char	*itag;
{
	int	n;

	n = atoi(num);
	if( n < 0 )  return(-1);

	if( n > max_chans )  {
		if( max_chans <= 0 )  {
			max_chans = 32;
			chan = (struct chan *)rt_malloc(
				max_chans * sizeof(struct chan),
				"chan[]" );
		} else {
			while( n > max_chans )
				max_chans *= 2;
			chan = (struct chan *)rt_realloc( (char *)chan,
				max_chans * sizeof(struct chan),
				"chan[]" );
		}
	}
	for( ; n >= nchans; nchans++ )
		bzero( (char *)&chan[nchans], sizeof(struct chan) );

	chan[n].c_ilen = len;
	chan[n].c_itag = rt_strdup( itag );
	chan[n].c_ival = (fastf_t *)rt_malloc( len * sizeof(fastf_t), "c_ival");
	return(n);
}

pr_chans()
{
	register int		ch;
	register struct chan	*cp;
	register int		i;

	for( ch=0; ch < nchans; ch++ )  {
		cp = &chan[ch];
		printf("--- Channel %d, ilen=%d (%s):\n",
			ch, cp->c_ilen, cp->c_itag );
		for( i=0; i < cp->c_ilen; i++ )  {
			printf(" %g\t%g\n", cp->c_itime[i], cp->c_ival[i]);
		}
		/* Later, c_oval's */
	}
}
