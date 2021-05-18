#ifndef QDM_FBSERV_H
#define QDM_FBSERV_H

#include "common.h"
#include "dm/fbserv.h"

__BEGIN_DECLS

extern int
qdm_is_listening(struct fbserv_obj *fbsp);
extern int
qdm_listen_on_port(struct fbserv_obj *fbsp, int available_port);
extern void
qdm_open_server_handler(struct fbserv_obj *fbsp);
extern void
qdm_close_server_handler(struct fbserv_obj *fbsp);
extern void
qdm_open_client_handler(struct fbserv_obj *fbsp, int i, void *data);
extern void
qdm_close_client_handler(struct fbserv_obj *fbsp, int sub);

__END_DECLS

#endif /* QDM_FBSERV_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
