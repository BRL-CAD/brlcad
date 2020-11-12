#include <alloca.h>
int
main ()
{
	char *p = (char *) alloca (2 * sizeof (int));
	if (p) return 0;
	;
	return 0;
}

