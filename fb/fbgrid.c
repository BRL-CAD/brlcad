/*
 *			G R I D
 *
 * This program computes an image for the frame buffer.
 *
 * Mike Muuss, 7/19/82
 */

struct pixel {
	char R;
	char G;
	char B;
	char XXx;
} line[512];

extern ikfd;

main()  {
	register int x, y;
	static int val;
	static struct pixel *lp;

	ikopen();

	for( y = 256; y < 512; y++ )  {
		for( x = 256; x < 512; x++ )  {
			if(
				x == y  ||
				(x % 8) == 0  ||
				(y % 8) == 0
			)
				val = 255;
			else
				val = 0;

			lp = &line[x];
			lp->R = lp->G = lp->B = val;
			lp = &line[511-x];
			lp->R = lp->G = lp->B = val;
		}

		lseek( ikfd, ((long) y) << 11, 0 );
		if( write( ikfd, line, sizeof line ) != sizeof line )  {
			printf("Write failed\n");
		}

		lseek( ikfd, ((long) 511-y) << 11, 0 );
		if( write( ikfd, line, sizeof line ) != sizeof line )  {
			printf("Write failed\n");
		}
	}
}
