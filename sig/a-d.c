/* Ascii to double */
#include <stdio.h>
#include <math.h>

static char usage[] = "\
Usage: a-d [values] < ascii > doubles\n";

main( argc, argv )
int	argc;
char	**argv;
{
	char	s[80];
	double	d;
	int	i;

	if( isatty(fileno(stdout)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	if( argc > 1 ) {
		/* get them from the command line */
		for( i = 1; i < argc; i++ ) {
			d = atof( argv[i] );
			fwrite( &d, sizeof(d), 1, stdout );
		}
	} else {
		/* get them from stdin */
#if 0
		while( fgets(s, 80, stdin) != NULL ) {
			d = atof( s );
#else
		/* XXX This one is slower but allows more than 1 per line */
		while( scanf("%f", &d) == 1 ) {
#endif
			fwrite( &d, sizeof(d), 1, stdout );
		}
	}
}
