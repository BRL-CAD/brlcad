/*
 * Package Handlers.
 */
void	pkgfoo();	/* foobar message handler */
void	rfbopen(), rfbclose(), rfbclear(), rfbread(), rfbwrite();
void	rfbcursor(), rfbgetcursor();
void	rfbrmap(), rfbwmap();
void	rfbhelp();
void	rfbreadrect(), rfbwriterect();
void	rfbpoll(), rfbflush(), rfbfree();
void	rfbview(), rfbgetview();
void	rfbsetcursor();
/* Old Routines */
void	rfbscursor(), rfbwindow(), rfbzoom();

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
	{ MSG_FBFLUSH,		rfbflush,	"Flush Output" },
	{ MSG_FBFLUSH + MSG_NORETURN, rfbflush, "Flush Output" },
	{ MSG_FBFREE,		rfbfree,	"Free Resources" },
	{ MSG_FBPOLL,		rfbpoll,	"Handle Events" },
	{ MSG_FBSETCURSOR,	rfbsetcursor,	"Set Cursor Shape" },
	{ MSG_FBSETCURSOR + MSG_NORETURN, rfbsetcursor, "Set Cursor Shape" },
	{ 0,			NULL,		NULL }
};
