/*
 *  			D E B U G . H
 *  
 *  Various definitions permitting segmented debugging to
 *  be independently controled.
 *  
 *  Author -
 *  	Mike Muuss, BRL
 *  
 *  $Revision$
 */
extern int debug;

#define DEBUG_OFF	0	/* No debugging */

/* These definitions are each for one bit */
#define DEBUG_SOLIDS	0x01	/* Print prep'ed solids */
#define DEBUG_HITS	0x02	/* Print hits used by view() */
#define DEBUG_ALLRAYS	0x04	/* Print all attempted shots */
#define DEBUG_QUICKIE	0x08	/* Just do 8x8 centered on origin */

#define DEBUG_TESTING	0x80	/* One-shot debugging flag */
