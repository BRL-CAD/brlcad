#include <stdio.h>

FILE	*out1;
FILE	*out2;

char	*buf;
int	width = 720;

main(argc, argv)
int	argc;
char	*argv[];
{
	if( (out1 = fopen("even.pix", "w")) == NULL )  {
		perror("even.pix");
		exit(1);
	}
	if( (out2 = fopen("odd.pix", "w")) == NULL )  {
		perror("odd.pix");
		exit(2);
	}

	buf = (char *)malloc( (width+1)*3 );

	for(;;)  {
		/* Even line */
		if( fread( buf, 3, width, stdin ) != width )
			break;
		if( fwrite( buf, 3, width, out1 ) != width )  {
			perror("fwrite even");
			exit(1);
		}
		/* Odd line */
		if( fread( buf, 3, width, stdin ) != width )
			break;
		if( fwrite( buf, 3, width, out2 ) != width )  {
			perror("fwrite odd");
			exit(1);
		}
	}
	exit(0);
}
