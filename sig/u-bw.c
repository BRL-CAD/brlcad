/*	U - B W . C -- convert unsigned shorts to unsigned char
 */
#include <unistd.h>
#include <stdio.h>

unsigned short ibuf[512];
unsigned char obuf[512];

int main(int ac, char **av)
{
	int num, i;

	if (isatty(fileno(stdin)) || isatty(fileno(stdout))) {
		(void)fprintf(stderr, "Usage: %s < u_shorts > bwfile\n",
			*av);
		return(-1);
	}

	if (ac > 1 && *av[1] == '-' && *av[2] == 'l') 
	    while ((num = fread(&ibuf[0], sizeof(*ibuf), 512, stdin)) > 0 ){
		for (i=0 ; i < num ; i++)
			obuf[i] = (unsigned char)ibuf[i];

		if (fwrite(&obuf[0], sizeof(*obuf),num,stdout)!=num) {
			(void)fprintf(stderr, "%s: error writing output\n", *av);
			return(-1);
		}
	    }
	else
	    while ((num = fread(&ibuf[0], sizeof(*ibuf), 512, stdin)) > 0 ){
		for (i=0 ; i < num ; i++)
			obuf[i] = (unsigned char)(ibuf[i] >> 8);

		if (fwrite(&obuf[0], sizeof(*obuf),num,stdout)!=num) {
			(void)fprintf(stderr, "%s: error writing output\n", *av);
			return(-1);
		}
	    }
	return 0;
}
