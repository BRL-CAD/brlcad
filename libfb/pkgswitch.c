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
