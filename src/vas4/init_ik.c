/*
 * init_ik.c - initialize Ikonas to RS170
 */

#include <stdio.h>

init_ik(void)
{
	char cmd[100];
	FILE *popen(const char *, const char *), *pipe;


	sprintf(cmd,"fbi > /dev/null");
	pipe = popen(cmd,"w");

/*	fprintf(pipe,"1\n6,7760\n9,456\n10,1015\n14,1\n\n-1\n"); /* */
	fprintf(pipe,"1\n6,4083\n14,1\n\n-1\n");

	pclose(pipe);
	fprintf(stderr,"Ikonas set for RS170 and External SYNC\n");
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
