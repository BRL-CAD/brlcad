/*
 * status.c - To get vas status
 *
 * Author: Steve Satterfield
 *
 */

#include <stdio.h>
#include "vas4.h"

#define M(s,c)	fprintf(stderr,s,c)

status(void)
{
	char c;

	vas_open();

	/* Request activity */
	vas_putc(C_ACTIVITY);

	/* Read result code */
	c = vas_getc();

	/* Print appropropriate message */
	/* See page c-6 */
	switch(c) {
		case '`':
			M("Power-on condition; no recording programmed (%c)\n",c);
			break;
		case 'b':
			M("Accepting programming for a recording (%c)\n",c);
			break;
		case 'c':
			M("Accepting programming for an edit recording (%c)\n",c);
			break;
		case 'd':
			M("Flashing E/E switch; ready to search for frame (%c)\n",c);
			break;
		case 'e':
			M("Checking for position on correct scene (%c)\n",c);
			break;
		case 'j':
			M("Searching for frame preceding next to record (%c)\n",c);
			break;
		case 'm':
			M("Displaying a warning message (%c)\n",c);
			break;
		case 'n':
			M("Ready to record first recording on old scene (%c)\n",c);
			break;
		case 'f':
			M("Ready to record next recording or TITLE (%c)\n",c);
			break;
		case 'a':
			M("Register function is active (%c)\n",c);
			break;
		case 'g':
			M("Prerolling, about to make recording (%c)\n",c);
			break;
		case 'h':
			M("Recording in progress (%c)\n",c);
			break;
		case 'i':
			M("Backspacing for next preroll and recording (%c)\n",c);
			break;
		case 'o':
			M("Holding momentarily before allowing to RECORD (%c)\n",c);
			break;
		case 'k':
			M("Accepting programming for Frame Change (%c)\n",c);
			break;
		case 'l':
			M("Accepting programming for HOLD (%c)\n",c);
			break;
	default:
			fprintf(stderr,"Unknow activity code: '%c' (%c)\n",c);
	}

	vas_close();
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
