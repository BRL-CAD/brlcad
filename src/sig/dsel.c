/*
 *  select some number of doubles
 */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>

double	buf[1024];

static char usage[] = "\
Usage: dsel num\n\
       dsel skip keep ...\n";

#define INTEGER_MAX ( ((int) ~0) >> 1 )

void	skip(register int num);
void	keep(register int num);

int main(int argc, char **argv)
{
	int	nskip;	/* number to skip */
	int	nkeep;	/* number to keep */

	if( argc < 1 || isatty(fileno(stdin)) || isatty(fileno(stdout)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	if( argc == 2 ) {
		keep( atoi(argv[1]) );
		exit( 0 );
	}

	while( argc > 1 ) {
		nskip = atoi(argv[1]);
		argc--;
		argv++;
		if( nskip > 0 )
			skip( nskip );

		if( argc > 1 ) {
			nkeep = atoi(argv[1]);
			argc--;
			argv++;
		} else {
			nkeep = INTEGER_MAX;
		}

		if( nkeep <= 0 )
			exit( 0 );
		keep( nkeep );
	}
	return 0;
}

void
skip(register int num)
{
	register int	n, m;

	while( num > 0 ) {
		n = num > 1024 ? 1024 : num;
		if( (m = fread( buf, sizeof(*buf), n, stdin )) == 0 )
			exit( 0 );
		num -= m;
	}
}

void
keep(register int num)
{
	register int	n, m;

	while( num > 0 ) {
		n = num > 1024 ? 1024 : num;
		if( (m = fread( buf, sizeof(*buf), n, stdin )) == 0 )
			exit( 0 );
		fwrite( buf, sizeof(*buf), m, stdout );
		num -= n;
	}
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
