#include "./pkg.h"
#include "./pkgtypes.h"

/*
 * Package Handlers.
 */
extern int pkgfoo();	/* foobar message handler */
struct pkg_switch pkg_switch[] = {
	{ MSG_ERROR, pkgfoo, "Error Message" },
	{ MSG_CLOSE, pkgfoo, "Close Connection" }
};
int pkg_swlen = 2;

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
