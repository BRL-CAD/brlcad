/*
 *			I K C L E A R . C
 *
 * This program is intended to be used to clear the screen of
 * the Ikonas display, quickly.
 *
 * The special FBC clear-screen command is used, for speed.
 *
 * Mike Muuss, BRL, 10/27/83.
 */
int ikfd;
int ikhires = 0;

main(argc, argv)
char	**argv;
int	argc;
{
	int	red, green, blue;

	ikopen();

	ikclear();
	load_map(1,0,0,0);		/* standard color map */

	if (argc > 1 && strcmp (argv[1], "-h") == 0) {
		ikhires++;
		argv++;
		argc--;
	}
	if (argc == 4) {
		red = atoi(argv[1]);
		blue = atoi(argv[2]);
		green = atoi(argv[3]);

		if (red < 0 || red > 255
		  || blue < 0 || blue > 255
		  || green < 0 || green > 255) {
		  	printf ("ikclear: bad color value\n");
		  	exit (99);
		}
		fillscreen(red, green, blue);
	} else if (argc > 1) {
		printf ("Usage:  ikclear [r g b]\n");
		exit (9);
	}
	exit (0);
}

fillscreen(r, g, b)
int	r, g, b;
{
	char	buf[1024*4*4];		/* 4 lines hires, 8 lines lores */
	char	*buflim;
	register char *cp;
	register int ycnt;

	cp = buf;
	buflim = buf + sizeof(buf);
	while (cp < buflim) {
		*cp++ = r;
		*cp++ = b;
		*cp++ = g;
		*cp++ = 0;
	}
	lseek (ikfd, 0L, 0);
	ycnt = (ikhires ? (1024/4) : (512/8));
	for (; ycnt > 0; --ycnt) {
		if (write (ikfd, buf, sizeof (buf)) < 0) {
			perror ("ikclear: write");
			exit (98);
		}
	}
}
