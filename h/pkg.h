/*
 *  			P K G . H
 */
#ifndef PKG_H_SEENYET
#define PKG_H_SEENYET

struct pkg_switch {
	char	pks_type;		/* Type code */
	int	(*pks_handler)();	/* Message Handler */
	char	*pks_title;		/* Description of message type */
};
extern struct pkg_switch pkg_switch[];	/* Array of message handlers */
extern int pkg_swlen;			/* Number of message handlers */

extern int pkg_open();
extern int pkg_server();
extern void pkg_close();
extern int pkg_send();
extern int pkg_waitfor();
extern char *pkg_bwaitfor();
extern int pkg_get();
extern int pkg_block();

#endif PKG_H_SEENYET
