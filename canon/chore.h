/*
 *			C H O R E . H
 *
 *  Header file for multi-threaded scanner support.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 *
 *  $Header$
 */

struct chore {
	int		todo;
	int		buflen;
	int		pix_y;
	int		canon_y;
	unsigned char	*cbuf;			/* ptr to canon buffer */
	unsigned char	obuf[255*1024];
};


#define GET(item, queue)	{\
	while( queue == NULL )  sginap(1); \
	item = queue; \
	queue = NULL; }

#define PUT(queue, item)	{ \
	while( queue != NULL )  sginap(1); \
	queue = item; \
	item = NULL;  }
