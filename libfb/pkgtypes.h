/*
 *		P K G T Y P E S . H
 *
 *  Types of packages used for the remote frame buffer
 *  communication between libfb/if_remote.c and rfbd/rfbd.c.
 *  This file is shared by those two modules.
 */

#define	MSG_FBOPEN	1
#define	MSG_FBCLOSE	2
#define	MSG_FBCLEAR	3
#define	MSG_FBREAD	4
#define	MSG_FBWRITE	5
#define	MSG_FBCURSOR	6
#define	MSG_FBWINDOW	7
#define	MSG_FBZOOM	8
#define	MSG_FBRMAP	12
#define	MSG_FBWMAP	13
#define	MSG_FBHELP	14
#define	MSG_FBREADRECT	15
#define	MSG_FBWRITERECT	16

#define	MSG_DATA	20
#define	MSG_RETURN	21
#define	MSG_CLOSE	22
#define	MSG_ERROR	23

#define	MSG_NORETURN	100
