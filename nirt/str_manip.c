/*	STR_MANIP.C	*/
#ifndef lint
static char RCSid[] = "$Header$";
#endif

/*	INCLUDES	*/
#include "conf.h"

#include <stdio.h>
#include <ctype.h>

#include "machine.h"
#include "vmath.h"
#include "./nirt.h"

int str_dbl(buf, Result)
char 	*buf;
double	*Result;
{
	int 	status = 0; 		/* 0, function failed - 1, success */
	int	sign = POS;		/* POS or NEG sign		   */
	double	Frac = .1;              /* used in the case of a fraction  */
	double  Value = 0.0;		/* current value of the double	   */
	int	i = 0;			/* current position of *buf        */

	if (*buf == '-')      /* check for a minus sign */  
	{
		sign = NEG;
		++i;
		++buf;
	}
	while (isdigit(*buf)) /* update Value while there is a digit */
	{
		status = 1;
		Value *= 10.0;
		Value += *buf++ - '0';
		++i;
	}
	if (*buf == '.')      /* check for a decimal point */
	{
		++i;
		++buf;
		while (isdigit(*buf))  /* update Value while there is a digit */
		{
			++i;
			status = 1;
			Value += (*buf++ - '0') * Frac;
			Frac *= .1;
		}
	}
	if (status == 0)
		return(0);    /* if function failed return a 0 */
	else
	{
		if (sign == NEG)
			Value *= -1;
		*Result = Value;
		return(i);    /* if function succeeds return position of *buf */
	}

}

char *basename(string)
char	*string;
{
    char	*sp;
    char        *sp2;

    for (sp = string + strlen(string); (sp > string) && (*sp != '/'); --sp)
	;
    if (*sp == '/')
        ++sp;
    sp2 = sp;
    while (*sp2 != '{' && *sp2 != '\0')
    	++sp2;
    *sp2 = '\0';
    return (sp);
}
