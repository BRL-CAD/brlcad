/*	I P U S T A T . C --- print the status of a Canon CLC500 IPU-10
 *
 *	Options
 *	h	help
 */
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./canon.h"

/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int main(ac,av)
int ac;
char *av[];
{
#if __sgi
	int i;
	struct dsreq *dsp;
	static char *scsi_device = "/dev/scsi/sc0d6l3";
	char *p;

	if ((i=parse_args(ac, av)) < ac) {
		scsi_device = av[i];
	}
	
	if ((dsp = dsopen(scsi_device, O_RDWR)) == NULL) {
		perror(av[i]);
		usage("Cannot open device\n");
	}

	
	printf("%s\n", ipu_inquire(dsp));
	ipu_get_conf(dsp);

	ipu_remote(dsp);

	printf("%s\n", p=ipu_list_files(dsp));

	free(p);

	dsclose(dsp);

	return(0);
#else
	fprintf(stderr,
		"%s only works on SGI(tm) systems with dslib support\n",
		*av);
	return(-1);
#endif
}

