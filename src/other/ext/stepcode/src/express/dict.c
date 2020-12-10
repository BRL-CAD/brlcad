

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

char DICT_type; /**< set to type of object found, as a side-effect of DICT lookup routines */

void DICTprint(Dictionary dict)
{
    Element e;
    DictionaryEntry de;

    HASHlistinit(dict, &de);

    while(0 != (e = (HASHlist(&de)))) {
        fprintf(stderr, "key <%s>  data <%s>  line <%d>  <\"%c\" %s>  <%s>\n",
                e->key, e->data, e->symbol->line, e->type,
                OBJget_type(e->type), e->symbol->filename);
    }
}

/** Initialize the Dictionary module */
void DICTinitialize(void)
{
}

/** Clean up the Dictionary module */
void DICTcleanup(void)
{
}

/**
 * Define anything in a dictionary.  Generates an
 * error directly if there is a duplicate value.
 * \return 0 on success, 1 on failure
 */
int DICTdefine(Dictionary dict, char *name, void *obj, Symbol *sym, char type)
{
    struct Element_ new, *old;

    new.key = name;
    new.data = obj;
    new.symbol = sym;
    new.type = type;

    if(0 == (old = HASHsearch(dict, &new, HASH_INSERT))) {
        return(0);
    }

    /* allow multiple definitions of an enumeration id in its
     * first scope of visibility.  *don't* allow enum id to be
     * shadowed by another type of symbol in the first scope
     * of visibility.  this changed (back) in the IS.
     *
     * Nov 2011 - Apparently, this changed again; I (MP) am
     * told that it is legal for an enum value and an entity
     * to have the same name. To fix this, I replaced the
     * || with && in the else-if below.
     */
    if((type == OBJ_ENUM) && (old->type == OBJ_ENUM)) {
        /* if we're adding an enum, but we've already seen one */
        /* (and only one enum), mark it ambiguous */
        DICTchange_type(old, OBJ_AMBIG_ENUM);
    } else if((type != OBJ_ENUM) && (!IS_ENUM(old->type))) {
        /* if we're adding a non-enum, and we've  *
         * already added a non-enum, complain     */
        if(sym->filename == old->symbol->filename) {
            ERRORreport_with_symbol(DUPLICATE_DECL, sym, name, old->symbol->line);
        } else {
            ERRORreport_with_symbol(DUPLICATE_DECL_DIFF_FILE, sym, name, old->symbol->line, old->symbol->filename);
        }
        ERRORreport(SUBORDINATE_FAILED);
        return(1);
    }
    return 0;
}

/**
 * This version is used for defining things within an enumeration scope
 * I.e., the only error it would pick up would be an error such as
 * ENUMERATION OF ( A, A ) which has happened!
 * This is the way DICTdefine used to look before enumerations gained
 * their unusual behavior with respect to scoping and visibility rules
 * \sa DICTdefine()
 */
int DICT_define(Dictionary dict, char *name, void *obj, Symbol *sym, char type)
{
    struct Element_ e, *e2;

    e.key = name;
    e.data = obj;
    e.symbol = sym;
    e.type = type;

    if(0 == (e2 = HASHsearch(dict, &e, HASH_INSERT))) {
        return(0);
    }

    if(sym->filename == e2->symbol->filename) {
        ERRORreport_with_symbol(DUPLICATE_DECL, sym, name, e2->symbol->line);
    } else {
        ERRORreport_with_symbol(DUPLICATE_DECL_DIFF_FILE, sym, name, e2->symbol->line, e2->symbol->filename);
    }
    ERRORreport(SUBORDINATE_FAILED);
    return(1);
}

/**
** \param dict dictionary to modify
** \param name name to remove
** \return  the entry removed, NULL if not found
    Changed to return void, since the hash code frees the element, there
    is no way to return (without godawful casting) the generic itself.
*/
void DICTundefine(Dictionary dict, char *name)
{
    struct Element_ e;

    e.key = name;
    HASHsearch(dict, &e, HASH_DELETE);
}

/**
** \param dictionary dictionary to look in
** \param name name to look up
** \return the value found, NULL if not found
*/
void *DICTlookup(Dictionary dictionary, char *name)
{
    struct Element_ e, *ep;

    if(!dictionary) {
        return 0;
    }

    e.key = name;
    ep = HASHsearch(dictionary, &e, HASH_FIND);
    if(ep) {
        DICT_type = ep->type;
        return(ep->data);
    }
    return(NULL);
}

/** like DICTlookup but returns symbol, too
 * \sa DICTlookup()
 */
void *DICTlookup_symbol(Dictionary dictionary, char *name, Symbol **sym)
{
    struct Element_ e, *ep;

    if(!dictionary) {
        return 0;
    }

    e.key = name;
    ep = HASHsearch(dictionary, &e, HASH_FIND);
    if(ep) {
        DICT_type = ep->type;
        *sym = ep->symbol;
        return(ep->data);
    }
    return(NULL);
}

void *DICTdo(DictionaryEntry *dict_entry)
{
    if(0 == HASHlist(dict_entry)) {
        return 0;
    }

    DICT_type = dict_entry->e->type;    /* side-effect! */
    return dict_entry->e->data;
}
