/*
 * Package Handlers.
 */
void	pkgfoo(struct pkg_conn *pcp, char *buf);	/* foobar message handler */
void	rfbopen(struct pkg_conn *pcp, char *buf), rfbclose(struct pkg_conn *pcp, char *buf), rfbclear(struct pkg_conn *pcp, char *buf), rfbread(struct pkg_conn *pcp, char *buf), rfbwrite(struct pkg_conn *pcp, char *buf);
void	rfbcursor(struct pkg_conn *pcp, char *buf), rfbgetcursor(struct pkg_conn *pcp, char *buf);
void	rfbrmap(struct pkg_conn *pcp, char *buf), rfbwmap(struct pkg_conn *pcp, char *buf);
void	rfbhelp(struct pkg_conn *pcp, char *buf);
void	rfbreadrect(struct pkg_conn *pcp, char *buf), rfbwriterect(struct pkg_conn *pcp, char *buf);
void	rfbbwreadrect(struct pkg_conn *pcp, char *buf), rfbbwwriterect(struct pkg_conn *pcp, char *buf);
void	rfbpoll(struct pkg_conn *pcp, char *buf), rfbflush(struct pkg_conn *pcp, char *buf), rfbfree(struct pkg_conn *pcp, char *buf);
void	rfbview(struct pkg_conn *pcp, char *buf), rfbgetview(struct pkg_conn *pcp, char *buf);
void	rfbsetcursor(struct pkg_conn *pcp, char *buf);
/* Old Routines */
void	rfbscursor(struct pkg_conn *pcp, char *buf), rfbwindow(struct pkg_conn *pcp, char *buf), rfbzoom(struct pkg_conn *pcp, char *buf);

static struct pkg_switch pkg_switch[] = {
	{ MSG_FBOPEN,		rfbopen,	"Open Framebuffer" },
	{ MSG_FBCLOSE,		rfbclose,	"Close Framebuffer" },
	{ MSG_FBCLEAR,		rfbclear,	"Clear Framebuffer" },
	{ MSG_FBREAD,		rfbread,	"Read Pixels" },
	{ MSG_FBWRITE,		rfbwrite,	"Write Pixels" },
	{ MSG_FBWRITE + MSG_NORETURN,	rfbwrite,	"Asynch write" },
	{ MSG_FBCURSOR,		rfbcursor,	"Cursor" },
	{ MSG_FBGETCURSOR,	rfbgetcursor,	"Get Cursor" },	   /*NEW*/
	{ MSG_FBSCURSOR,	rfbscursor,	"Screen Cursor" }, /*OLD*/
	{ MSG_FBWINDOW,		rfbwindow,	"Window" },	   /*OLD*/
	{ MSG_FBZOOM,		rfbzoom,	"Zoom" },	   /*OLD*/
	{ MSG_FBVIEW,		rfbview,	"View" },	   /*NEW*/
	{ MSG_FBGETVIEW,	rfbgetview,	"Get View" },	   /*NEW*/
	{ MSG_FBRMAP,		rfbrmap,	"R Map" },
	{ MSG_FBWMAP,		rfbwmap,	"W Map" },
	{ MSG_FBHELP,		rfbhelp,	"Help Request" },
	{ MSG_ERROR,		pkgfoo,		"Error Message" },
	{ MSG_CLOSE,		pkgfoo,		"Close Connection" },
	{ MSG_FBREADRECT, 	rfbreadrect,	"Read Rectangle" },
	{ MSG_FBWRITERECT,	rfbwriterect,	"Write Rectangle" },
	{ MSG_FBWRITERECT + MSG_NORETURN, rfbwriterect,"Write Rectangle" },
	{ MSG_FBBWREADRECT, 	rfbbwreadrect,"Read BW Rectangle" },
	{ MSG_FBBWWRITERECT,	rfbbwwriterect,"Write BW Rectangle" },
	{ MSG_FBBWWRITERECT+MSG_NORETURN, rfbbwwriterect,"Write BW Rectangle" },
	{ MSG_FBFLUSH,		rfbflush,	"Flush Output" },
	{ MSG_FBFLUSH + MSG_NORETURN, rfbflush, "Flush Output" },
	{ MSG_FBFREE,		rfbfree,	"Free Resources" },
	{ MSG_FBPOLL,		rfbpoll,	"Handle Events" },
	{ MSG_FBSETCURSOR,	rfbsetcursor,	"Set Cursor Shape" },
	{ MSG_FBSETCURSOR + MSG_NORETURN, rfbsetcursor, "Set Cursor Shape" },
	{ 0,			NULL,		NULL }
};
