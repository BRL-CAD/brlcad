#include <stdio.h>

unsigned short ibuf[1024];

int main(void)
{
	int n, i;

	while ((n=fread(ibuf, sizeof(*ibuf), 1024, stdin)) > 0) {
		for (i=0 ; i < n ; ++i)
			printf("%hd\n", ibuf[i]);
	}

	return 0;
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
