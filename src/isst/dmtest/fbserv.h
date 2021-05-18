#ifndef QDM_TCL_PRIVATE_H
#define QDM_TCL_PRIVATE_H

#include "common.h"
#include "dm/fbserv.h"

__BEGIN_DECLS

extern int
tclcad_is_listening(struct fbserv_obj *fbsp);
extern int
tclcad_listen_on_port(struct fbserv_obj *fbsp, int available_port);
extern void
tclcad_open_server_handler(struct fbserv_obj *fbsp);
extern void
tclcad_close_server_handler(struct fbserv_obj *fbsp);
extern void
tclcad_open_client_handler(struct fbserv_obj *fbsp, int i, void *data);
extern void
tclcad_close_client_handler(struct fbserv_obj *fbsp, int sub);

__END_DECLS

#endif /* QDM_TCL_PRIVATE_H */

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
