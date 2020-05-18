/*          R E G R E S S _ P K G _ P R O T O C O L . H
 * BRL-CAD
 *
 * Published in 2020 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file regress_pkg_protocol.h
 *
 * Network protocol and common definitons for the libpkg
 * regression test client and server
 *
 */

#define MAGIC_ID	"RPKG"
#define MSG_HELO	1
#define MSG_DATA	2
#define MSG_CIAO	3
#define MAX_PORT_DIGITS      5

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
