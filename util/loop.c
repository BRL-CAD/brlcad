/*
 *			L O O P . C
 *
 *	Simple program to output integers or floats between 
 *	"start" and "finish", inclusive.  Default is an increment 
 *	of +1 if start < finish or -1 if start > finish.  User may 
 *	specify an alternate increment.  Also, user may left-pad 
 *	output integers with zeros.  There is no attempt to prevent 
 *	"infinite" loops.
 *
 *  Authors -
 *	John Grosh, Phil Dykstra, and Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include <stdio.h>
#include <math.h>

#define	INTEGER 0
#define	REAL	1

main(argc, argv)
int	argc;
char	**argv;
{
	register int	status = INTEGER;

	register int	i;
	register int 	start,  finish, incr;

	register double	d;
	register double	dstart, dfinish,dincr;

	if (argc < 3 || argc > 4) {
		fprintf(stderr, "Usage:  loop start finish [incr]\n");
		exit(9);
	}

	/* determine if any arguments are real */
	for (i = 1; i < argc; i++) {
		if (atof(argv[i]) != atoi(argv[i])) {	
			status = REAL;
			break;
		}
	}

	if (status == REAL) {
		dstart  = atof(argv[1]);
		dfinish = atof(argv[2]);

		if (argc == 4)
			dincr = atof(argv[3]);
		else {
			if (dstart > dfinish)
				dincr = -1.0;
			else
				dincr =  1.0;
		}

		if (dincr >= 0.0)
			for (d = dstart; d <= dfinish; d += dincr)
				printf("%g\n", d);
		else 
			for (d = dstart; d >= dfinish; d += dincr)
				printf("%g\n", d);
	} else {
		/* print out integer output */
		char	*cp;
		char	fmt_string[50];

		int	field_width = 0;

		int	zeros      = 0;  /* leading zeros for output */
		int	zeros_arg1 = 0;  /* leading zeros in arg[1]  */
		int	zeros_arg2 = 0;  /* leading zeros in arg[2]  */
		int	zeros_arg3 = 0;  /* leading zeros in arg[3]  */

 		/* count leading leading zeros in argv[1] */
		for (cp = argv[1]; *cp == '0'; cp++)
			zeros_arg1++;
		if (*cp == '\0') 
			zeros_arg1--;

 		/* count leading leading zeros in argv[2] */
		for (cp = argv[2]; *cp == '0'; cp++)
			zeros_arg2++;
		if (*cp == '\0') 
			zeros_arg2--;

 		/* if argv[3] exists, count leading leading zeros */
		if (argc == 4 ) {
			for (cp = argv[3]; *cp == '0'; cp++)
				zeros_arg3++;
			if (*cp == '\0') 
				zeros_arg3--;
		}

		/* determine field width and leading zeros*/
		if (zeros_arg1 >= zeros_arg2 && zeros_arg1 >= zeros_arg3) {
			field_width = strlen(argv[1]);
			zeros       = zeros_arg1;
		} else if (zeros_arg2 >= zeros_arg1 && zeros_arg2 >= zeros_arg3) {
			field_width = strlen(argv[2]);
			zeros       = zeros_arg2;
		} else {
			field_width = strlen(argv[3]);
			zeros       = zeros_arg3;
		}

		/* printf format string fmt_string */
		if (zeros > 0) 
			sprintf(fmt_string,"%%0%dd\n",field_width); 
		else
			strcpy(fmt_string,"%d\n");

		start  = atoi(argv[1]);
		finish = atoi(argv[2]);

		if (argc == 4)
			incr = atoi(argv[3]);
		else {
			if (start > finish)
				incr = -1;
			else
				incr =  1;
		}

		if (incr >= 0)
			for (i = start; i <= finish; i += incr)
				printf(fmt_string, i);
		else 
			for (i = start; i >= finish; i += incr)
				printf(fmt_string, i);
	}
	exit(0);
}
