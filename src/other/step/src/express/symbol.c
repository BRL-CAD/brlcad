#if !(defined(__CENTERLINE__) || defined(lint))
static char rcsid[] = "$Id: symbol.c,v 1.6 1997/01/21 19:19:51 dar Exp $";
#endif

/************************************************************************
** Module:	Symbol
** Description:	This module implements the Symbol abstraction.  
** Constants:
**	SYMBOL_NULL	- the null Symbol
**
************************************************************************/

/*
 * This code was developed with the support of the United States Government,
 * and is not subject to copyright.
 *
 * $Log: symbol.c,v $
 * Revision 1.6  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.5  1994/05/11  19:51:24  libes
 * numerous fixes
 *
 * Revision 1.4  1993/10/15  18:48:48  libes
 * CADDETC certified
 *
 * Revision 1.3  1992/08/18  17:23:43  libes
 * no change
 *
 * Revision 1.2  1992/05/31  23:32:26  libes
 * implemented ALIAS resolution
 *
 * Revision 1.1  1992/05/28  03:55:04  libes
 * Initial revision
 */

#define	SYMBOL_C
#include "express/symbol.h"

/*
** Procedure:	SYMBOLinitialize
** Parameters:	-- none --
** Returns:	void
** Description:	Initialize the Symbol module
*/

void
SYMBOLinitialize()
{
	MEMinitialize(&SYMBOL_fl,sizeof(struct Symbol_),100,100);
}

Symbol *
SYMBOLcreate(name,line,filename)
char *name;
int line;
char *filename;
{
	Symbol *sym = SYMBOL_new();
	sym->name = name;
	sym->line = line;
	sym->filename = current_filename;
	return sym;
}
