/*
 *			I B A R . C
 *
 *  Integer count, by increment.
 */

main(argc, argv)
char **argv;
{
	register int start, finish, incr;
	register int i;

	if( argc < 3 || argc > 4 )  {
		printf("Usage:  ibar start finish [incr]\n");
		exit(9);
	}
	start = atoi( argv[1] );
	finish = atoi( argv[2] );
	incr = 1;
	if( argc == 4 )
		incr = atoi( argv[3] );

	if( incr > 0 )  {
		for( i=start; i<=finish; i += incr )
			printf("%d\n", i);
	} else {
		for( i=start; i>=finish; i += incr )
			printf("%d\n", i);
	}
}
