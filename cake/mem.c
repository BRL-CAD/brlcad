/*
**	Memory management module.
*/

static	char
rcs_id[] = "$Header$";

#include	"cake.h"

/*
**	Allocate space if possible. In the future it may record
**	all pointers returned, so that old can do some checks.
*/

Cast
newmem(size)
int	size;
{
	reg	char	*space;

	if ((space = malloc((unsigned) size)) == NULL)
	{
		fprintf(stderr, "cake system error: no more malloc\n");
		exit_cake(FALSE);
	}

#ifdef	EXTRACHECK
	if (((int) space & 03) != 0)
	{
		fprintf(stderr, "cake system error: malloc not aligned\n");
		exit_cake(FALSE);
	}
#endif

	return (Cast) space;
}

/*
**	Return some storage to the free list. This storage must
**	have been obtained from new and malloc.
*/

/*ARGSUSED*/
void
oldmem(ptr)
Cast	ptr;
{
#ifdef	MEMUSED
	free((char *) ptr);
#endif
}
