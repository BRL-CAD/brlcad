#include "fb.h"
#include "pkg.h"

extern FBIO X24_interface;
#ifdef DM_OGL
extern FBIO ogl_interface;
#endif

#define NET_LONG_LEN	4	/* # bytes to network long */
#define MAX_CLIENTS 32
#define MAX_PORT_TRIES 100
#define FBS_CALLBACK_NULL	(void (*)())NULL

struct fbserv_listener {
  int			fbsl_fd;			/* socket to listen for connections */
  int			fbsl_port;			/* port number to listen on */
  int			fbsl_listen;			/* !0 means listen for connections */
  struct fbserv_obj	*fbsl_fbsp;			/* points to its fbserv object */
};

struct fbserv_client {
  int			fbsc_fd;
  struct pkg_conn	*fbsc_pkg;
  struct fbserv_obj	*fbsc_fbsp;			/* points to its fbserv object */
};

struct fbserv_obj {
  FBIO				*fbs_fbp;			/* framebuffer pointer */
  struct fbserv_listener	fbs_listener;			/* data for listening */
  struct fbserv_client		fbs_clients[MAX_CLIENTS];	/* connected clients */
  void				(*fbs_callback)();		/* callback function */
  genptr_t			fbs_clientData;
};

extern int fbs_open();
extern int fbs_close();

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
