/*	I P U S T A T . C --- print the status of a Canon CLC500 IPU-10
 *
 *	Options
 *	h	help
 */
#include <stdio.h>
#include <string.h>
#if defined(__sgi) || defined(sgi)
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include "./canon.h"
#endif
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
#if defined(__sgi) || defined(sgi)
	int i;
	struct dsreq *dsp;
	char *p;

	if ((i=parse_args(ac, av)) < ac)
		fprintf(stderr,
			"%s: Excess command line arguments ignored\n",
			progname);

	
	if ((dsp = dsopen(scsi_device, O_RDWR)) == NULL) {
		perror(scsi_device);
		fprintf(stderr, "%s: Cannot open device \"%s\"\n", progname, scsi_device);
		usage(NULL);
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

