main(argc, argv)
char **argv;
{
	register int low, up, incr;
	register int i;

	if( argc < 3 || argc > 4 )  {
		printf("Usage:  ibar low upper [incr]\n");
		exit(9);
	}
	low = atoi( argv[1] );
	up = atoi( argv[2] );
	incr = 1;
	if( argc == 4 )
		incr = atoi( argv[3] );

	for( i=low; i<=up; i += incr )
		printf("%d\n", i);
}
