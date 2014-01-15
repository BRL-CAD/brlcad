#include "common.h"

#include <stdlib.h>
#include <stdio.h>

#include "bio.h"
#include "bu.h"
#include "icv.h"

void usage()
{
    bu_log("[-h] [squaresize] [-w width] [-n height] [-W out_width ] [-N out_height] \n\
		    [-b -p -d -m] \n\
			[-S out_squaresize] [-o out_file] [file] \n");

    bu_log("#Image Options\n\
	    \t -b for bw image\n\
	    \t -d for dpix image\n\
	    \t -m for b image\n\
	    \t -p for pix image\n");
}

int main(int argc, char* argv[])
{
    char *out_file = NULL;
    char *in_file = NULL;
    int c;
    int inx=0, iny=0;
    int outx=0, outy=0;
    icv_image_t *bif;
    ICV_IMAGE_FORMAT format=ICV_IMAGE_AUTO;
    int urx, ury, ulx, uly, llx, lly, lrx, lry;
    int ret;

    if (argc<2) {
	    usage();
	    return 1;
    }

    while ((c = bu_getopt(argc, argv, "s:W:w:N:n:s:S:o:bpdmh?")) != -1) {
	    switch (c) {
		case 's':
			inx = iny = atoi(bu_optarg);
			break;
		case 'W':
			outx = atoi(bu_optarg);
			break;
		case 'w':
			inx = atoi(bu_optarg);
			break;
		case 'N':
			outy = atoi(bu_optarg);
			break;
		case 'n':
			iny = atoi(bu_optarg);
			break;
		case 'S':
			outy = outx = atoi(bu_optarg);
			break;
		case 'o':
			out_file = bu_optarg;
			break;
		case 'b' :
			format = ICV_IMAGE_BW;
			break;
		case 'p' :
			format = ICV_IMAGE_PIX;
			break;
		case 'd' :
			format = ICV_IMAGE_DPIX;
			break;
		case 'm' :
			format = ICV_IMAGE_PPM;
			break;
		case 'h':
		default:
			usage();
			return 1;

	    }
    }

    if (bu_optind < argc) {
	in_file = argv[bu_optind];
    } else {
	usage();
	return 1;
    }

    bu_log("\t          (ulx,uly)         (urx,ury)\n\
	    \t           __________________\n\
	    \t          /                 |\n\
	    \t         /                  |\n\
	    \t        /                   |\n\
	    \t       /                    |\n\
	    \t      /                     |\n\
	    \t     /______________________|\n\
	    \t   (llx,lly)             (lrx,lry)\n");


    bu_log("Prompting Input Parameters\n");

    bu_log("\tUpper left corner in input file (ulx, uly)?: ");
	ret = scanf("%d%d", &ulx, &uly);
	if (ret != 2)
	    perror("scanf");

	bu_log("\tUpper right corner (urx, ury)?: ");
	ret = scanf("%d%d", &urx, &ury);
	if (ret != 2)
	    perror("scanf");

	bu_log("\tLower right (lrx, lry)?: ");
	ret = scanf("%d%d", &lrx, &lry);
	if (ret != 2)
	    perror("scanf");

	bu_log("\tLower left (llx, lly)?: ");
	ret = scanf("%d%d", &llx, &lly);
	if (ret != 2)
	    perror("scanf");


    bif = icv_read(in_file, format, inx, iny);
    icv_crop(bif, ulx, uly, urx, ury, lrx, lry, llx, lly, outy, outx);
    icv_write(bif,out_file, format);
    icv_destroy(bif);
    return 0;
}
