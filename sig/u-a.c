#include <stdio.h>

unsigned short ibuf[1024];

int main()
{
	int n, i;

	while ((n=fread(ibuf, sizeof(*ibuf), 1024, stdin)) > 0) {
		for (i=0 ; i < n ; ++i)
			printf("%hd\n", ibuf[i]);
	}

	return 0;
}
