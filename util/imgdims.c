/*
 *				I M G D I M S
 *
 *		Trivial front-end to bn_common_image_size()
 */
#include <stdio.h>

print_usage ()
{
    fputs("Usage: 'imgdims n'\n", stderr); 
}

main (argc, argv)

int argc;
char *argv[];

{
    int	width;
    int	height;
    int	nm_bytes;
    int	bytes_per_pixel = 3;
    int	nm_pixels;

    if (sscanf(argv[1], "%d", &nm_bytes) != 1)
    {
	print_usage();
	exit (1);
    }
    if (nm_bytes % bytes_per_pixel == 0)
	nm_pixels = nm_bytes / bytes_per_pixel;
    else
    {
	bu_log("Image size (%d bytes) is not a multiple of pixel size (%d bytes)\n", nm_bytes, bytes_per_pixel);
	exit (1);
    }

    if (bn_common_image_size(&width, &height, nm_pixels))
	printf("%d %d\n", width, height);
    else
	printf("Returned 0\n");

    exit (0);
}
