/*
 *  			T E X T . C
 *  
 *  Texture map lookup
 */
#include <stdio.h>
#include "../h/machine.h"
#include "text.h"

extern char *rt_malloc();

struct texture txt = {
	"./text.pix",
	512,
	512,
	512,
	0
};
/*
 *  			T E X T _ U V G E T
 *  
 *  Given a u,v coordinate within the texture ( 0 <= u,v <= 1.0 ),
 *  return a pointer to the relevant pixel.
 *
 *  Note that .pix files are stored left-to-right, bottom-to-top,
 *  which works out very naturally for the indexing scheme.
 */
unsigned char *
text_uvget( tp, uvp )
register struct texture *tp;
fastf_t *uvp;
{
tp = &txt;	/* HACK */
	/* If File could not be opened -- give rt_g.debug colors */
top:
	if( tp->tx_file == (char *)0 )  {
		static char ret[3];
		ret[0] = uvp[0]*255;
		ret[1] = 0;
		ret[2] = uvp[1]*255;
		return((unsigned char *)ret);
	}
	/* Dynamic load of file -- don't read until first pixel needed */
	if( tp->tx_pixels == (char *)0 )  {
		char *linebuf;
		register int fd;
		register int i;

		if( (fd = open(tp->tx_file, 0)) < 0 )  {
			perror(tp->tx_file);
			tp->tx_file = (char *)0;
			goto top;
		}
		linebuf = rt_malloc(tp->tx_fw*3,"texture file line");
		tp->tx_pixels = rt_malloc(
			tp->tx_w * tp->tx_l * 3,
			tp->tx_file );
		for( i=0; i<tp->tx_l; i++ )  {
			if( read(fd,linebuf,tp->tx_fw*3) != tp->tx_fw*3 )  {
				rt_log("text_uvget: read error on %s\n", tp->tx_file);
				tp->tx_file = (char *)0;
				(void)close(fd);
				rt_free(linebuf,"file line, error");
				goto top;
			}
			bcopy( linebuf, tp->tx_pixels + i*tp->tx_w*3, tp->tx_w*3 );
		}
		(void)close(fd);
		rt_free(linebuf,"texture file line");
	}
	/* u is left->right index, v is line number */
	return( (unsigned char *)tp->tx_pixels +
		((int) (uvp[1]*(tp->tx_l-1))) * tp->tx_w * 3 +	/* v */
		((int) (uvp[0]*(tp->tx_w-1))) * 3 );
}
