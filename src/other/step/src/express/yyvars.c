static char rcsid[] = "$Id: yyvars.c,v 1.1 1994/05/11 21:36:48 libes dec96 $";

/*
 * $Log: yyvars.c,v $
 * Revision 1.1  1994/05/11 21:36:48  libes
 * Initial revision
 *
 * Revision 1.0.1.1  1992/01/23  03:28:00  libes
 * copied from ~pdes
 *
 * Revision 1.2  1990/09/06  10:59:06  clark
 * BPR 2.1 beta
 *
 * Revision 1.1  90/06/11  16:30:22  clark
 * Initial revision
 * 
 */

#include <stdio.h>

/* FILE *yyin = {stdin}, *yyout = {stdout}; */

int expyylineno = 1;

void
yynewparse()
{
    expyylineno = 1;
}
