#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "rtstring.h"
#include "raytrace.h"

struct whatsit
{
    long	w_magic;
    double	w_d;
};
#define	WHATSIT_MAGIC	0x12345678

void free_whatsit (struct whatsit *wp, char *s)
{
    RT_CKMAG(wp, WHATSIT_MAGIC, "whatsit");

    rt_free((char *) wp, "a whatsit");
}

main (void)
{
    struct whatsit	*wp;

    rt_log("allocating a whatsit...\n");
    wp = (struct whatsit *) rt_malloc(sizeof(struct whatsit), "the whatsit");

    rt_log("Before initializing, the whatsit = <%x> (%x, %g)\n",
	    wp, wp -> w_magic, wp -> w_d);
    wp -> w_magic = WHATSIT_MAGIC;
    wp -> w_d = 4.96962656372528225336310;
    rt_log("After initializing, the whatsit = <%x> (%x, %g)\n",
	    wp, wp -> w_magic, wp -> w_d);

    free_whatsit(wp, "the whatsit once");
    rt_log("Freed it once\n");

    free_whatsit(wp, "the whatsit twice");
    rt_log("Freed it again\n");
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
