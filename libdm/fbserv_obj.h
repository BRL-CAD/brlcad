#include "fb.h"
#include "pkg.h"

extern FBIO X24_interface;
#ifdef DM_OGL
extern FBIO ogl_interface;
#endif

#define NET_LONG_LEN	4	/* # bytes to network long */
#define MAX_CLIENTS 32
#define MAX_PORT_TRIES 100

struct listener {
  int			l_fd;			/* socket to listen for connections */
  int			l_port;			/* port number to listen on */
  int			l_listen;		/* !0 means listen for connections */
  struct fbserv_obj	*l_fbsp;		/* points to its fbserv object */
};

struct client {
  int			c_fd;
  struct pkg_conn	*c_pkg;
  struct fbserv_obj	*c_fbsp;		/* points to its fbserv object */
};

struct fbserv_obj {
  FBIO			*fbs_fbp;			/* framebuffer pointer */
  struct listener	fbs_listener;			/* data for listening */
  struct client		fbs_clients[MAX_CLIENTS];	/* connected clients */
};

extern int fbs_open();
extern int fbs_close();
