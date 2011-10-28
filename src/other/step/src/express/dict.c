static char rcsid[] = "$Id: dict.c,v 1.4 1997/01/21 19:19:51 dar Exp $";

/*
 * This work was supported by the United States Government, and is
 * not subject to copyright.
 *
 * $Log: dict.c,v $
 * Revision 1.4  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.3  1994/11/10  19:20:03  clark
 * Update to IS
 *
 * Revision 1.2  1993/10/15  18:49:55  libes
 * CADDETC certified
 *
 * Revision 1.6  1993/02/22  21:41:39  libes
 * enum fix
 *
 * Revision 1.5  1993/01/19  22:45:07  libes
 * *** empty log message ***
 *
 * Revision 1.4  1992/08/18  17:16:22  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:08:05  libes
 * prettied up interface to print_objects_when_running
 *
 * Revision 1.2  1992/05/31  08:37:18  libes
 * multiple files
 *
 * Revision 1.1  1992/05/28  03:56:55  libes
 * Initial revision
 */

#include "express/dict.h"
#include "express/object.h"
#include "express/expbasic.h"

char DICT_type;	/* set as a side-effect of DICT lookup routines */
		/* to type of object found */

static Error	ERROR_duplicate_decl;
static Error	ERROR_duplicate_decl_diff_file;

void
DICTprint(Dictionary dict)
{
	Element e;
	DictionaryEntry de;

	HASHlistinit(dict,&de);

	while (0 != (e = (HASHlist(&de)))) {
		printf("key <%s>  data <%x>  line <%d>  <\"%c\" %s>  <%s>\n",
			e->key,e->data,e->symbol->line,e->type,
			OBJget_type(e->type),e->symbol->filename);
	}
}

/*
** Procedure:	DICTinitialize
** Parameters:	-- none --
** Returns:	void
** Description:	Initialize the Dictionary module
*/

void
DICTinitialize(void)
{
	ERROR_duplicate_decl = ERRORcreate(
"Redeclaration of %s.  Previous declaration was on line %d.", SEVERITY_ERROR);
	ERROR_duplicate_decl_diff_file = ERRORcreate(
"Redeclaration of %s.  Previous declaration was on line %d in file %s.", SEVERITY_ERROR);
}

/*
** Procedure:	DICTcreate
** Parameters:	int  size	- estimated (initial) max # of entries
** Returns:	Dictionary	- a new dictionary of the specified size
*/

/* now a macro */

/*
** Procedure:	DICTdefine
** Parameters:	Dictionary dictionary	- dictionary to modify
**		Generic    entry	- entry to be added
**		Error*     experrc		- buffer for error code
** Returns:	int failure		- 0 on success, 1 on failure
** Description:	Define anything in a dictionary.  Generates an error
**		directly if there is a duplicate value.
*/

int
DICTdefine(Dictionary dict, char *name, Generic obj, Symbol *sym,char type)
{
	struct Element_ new, *old;

	new.key = name;
	new.data = obj;
	new.symbol = sym;
	new.type = type;

	if (0 == (old = HASHsearch(dict,&new,HASH_INSERT))) return(0);

	/* allow multiple definitions of an enumeration id in its */
	/* first scope of visibility.  *don't* allow enum id to be */
	/* shadowed by another type of symbol in the first scope */
	/* of visibility.  this changed (back) in the IS. */
	if ((type == OBJ_ENUM) && (old->type == OBJ_ENUM)) {
		/* if we're adding an enum, but we've already seen one */
		/* (and only one enum), mark it ambiguous */
		DICTchange_type(old,OBJ_AMBIG_ENUM);
	} else if ((type != OBJ_ENUM) || (!IS_ENUM(old->type))) {
		/* if we're adding a non-enum, or we've already added a */
		/* non-enum, complain */
		if (sym->filename == old->symbol->filename) {
			ERRORreport_with_symbol(ERROR_duplicate_decl,sym,name,old->symbol->line);
		} else {
			ERRORreport_with_symbol(ERROR_duplicate_decl_diff_file,sym,name,old->symbol->line,old->symbol->filename);
		}
		experrc = ERROR_subordinate_failed;
		return(1);
	}
	return 0;

#if 0
	/* following code is a little tricky and accounts for the fact */
	/* that enumerations are entered into their first scope of */
	/* visibility but may be shadowed by other objects that are declared */
	/* in that scope */
	if (type == OBJ_ENUM) {
		if (old->type == OBJ_ENUM)
			DICTchange(old,obj,sym,OBJ_AMBIG_ENUM);
	} else if (IS_ENUM(old->type)) {
		DICTchange(old,obj,sym,type);
	} else {
		if (sym->filename == old->symbol->filename) {
			ERRORreport_with_symbol(ERROR_duplicate_decl,sym,name,old->symbol->line);
		} else {
			ERRORreport_with_symbol(ERROR_duplicate_decl_diff_file,sym,name,old->symbol->line,old->symbol->filename);
		}
		experrc = ERROR_subordinate_failed;
		return(1);
	}
	return 0;
#endif
}

/* this version is used for defining things within an enumeration scope */
/* I.e., the only error it would pick up would be an error such as */
/* ENUMERATION OF ( A, A ) which has happened! */
/* This is the way DICTdefine used to look before enumerations gained */
/* their unusual behavior with respect to scoping and visibility rules */
int
DICT_define(Dictionary dict, char *name, Generic obj, Symbol *sym,char type)
{
	struct Element_ e, *e2;

	e.key = name;
	e.data = obj;
	e.symbol = sym;
	e.type = type;

	if (0 == (e2 = HASHsearch(dict,&e,HASH_INSERT))) return(0);

	if (sym->filename == e2->symbol->filename) {
		ERRORreport_with_symbol(ERROR_duplicate_decl,sym,name,e2->symbol->line);
	} else {
		ERRORreport_with_symbol(ERROR_duplicate_decl_diff_file,sym,name,e2->symbol->line,e2->symbol->filename);
	}
	experrc = ERROR_subordinate_failed;
	return(1);
}

/*
** Procedure:	DICTundefine
** Parameters:	Dictionary dictionary	- dictionary to modify
**		char *     name		- name to remove
** Returns:	Generic			- the entry removed, NULL if not found
	Changed to return void, since the hash code frees the element, there
	is no way to return (without godawful casting) the generic itself.
*/

void
DICTundefine(Dictionary dict, char * name)
{
    struct Element_ e;

    e.key = name;
    HASHsearch(dict,&e,HASH_DELETE);
}

/*
** Procedure:	DICTlookup
** Parameters:	Dictionary dictionary	- dictionary to look in
**		char *     name		- name to look up
** Returns:	Generic			- the value found, NULL if not found
*/

Generic
DICTlookup(Dictionary dictionary, char * name)
{
	struct Element_ e, *ep;

	if (!dictionary) return 0;

	e.key = name;
	ep = HASHsearch(dictionary,&e,HASH_FIND);
	if (ep) {
		DICT_type = ep->type;
		return(ep->data);
	}
		return(NULL);
}

/* like DICTlookup but returns symbol, too */
Generic
DICTlookup_symbol(Dictionary dictionary, char * name, Symbol **sym)
{
	struct Element_ e, *ep;

	if (!dictionary) return 0;

	e.key = name;
	ep = HASHsearch(dictionary,&e,HASH_FIND);
	if (ep) {
		DICT_type = ep->type;
		*sym = ep->symbol;
		return(ep->data);
	}
		return(NULL);
}

Generic
DICTdo(DictionaryEntry *dict_entry)
{
	if (0 == HASHlist(dict_entry)) return 0;

	DICT_type = dict_entry->e->type;	/* side-effect! */
	return dict_entry->e->data;
}
