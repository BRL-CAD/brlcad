/*****************************************************************************
 * multpass.c                                                                *
 *                                                                           *
 * Description:                                                              *
 *   Adds multi-schema support enhancement to fedex_plus parser.  Allows the *
 *   generation of C++ representations for multiple EXPRESS schemas without  *
 *   creating conflicting header files.  Previously, fedex_plus would gene-  *
 *   rate a single set of files (.h, .cc, .init.cc) for each schema found in *
 *   the processed EXPRESS file.  A number of problem situations occurred:   *
 *                                                                           *
 *   (1) Two schemas which USE or REFERENCE entities from one another.  If   *
 *       e.g. schema A USEs entity 1 from schema B and defines ent 2 subtype *
 *       of 1, and schema B USEs ent 2 from A and defines ents 1 and 3 sub-  *
 *       type of 2, neither include file could be compiled first.            *
 *   (2) An entity which has a select or enumeration attribute which is de-  *
 *       fined in another schema.                                            *
 *   (3) A select type which has a select or enum member item which is de-   *
 *       fined in another schema.
 *   (4) An enumeration type which is a redefinition of an enum defined in   *
 *       another schema.                                                     *
 *                                                                           *
 *   The functions in multpass.c provide for the processing of each schema   *
 *   in multiple passes.  At each pass, only entities and types which are    *
 *   not dependent on undefined objects are processed.  With each pass, more *
 *   entities and types become defined and more dependent entities and types *
 *   will become processable.  At each state of processing, a separate set   *
 *   of C++ files is generated.  Lastly, multpass processes certain aggre-   *
 *   gate and redefined objects after all the fundamental types are defined. *
 *                                                                           *
 * Created by:  David Rosenfeld                                              *
 * Date:        04/09/97                                                     *
 *****************************************************************************/

#include <stdlib.h>
#include "classes.h"

#define FALSE 0
#define TRUE  1

/* Local function prototypes: */
static void initializeMarks( Express );
static void unsetObjs( Schema );
static int checkTypes( Schema );
static int checkEnts( Schema );
static void markDescs( Entity );
static int checkItem( Type, Scope, Schema, int *, int );
static int ENUMcanBeProcessed( Type, Schema );
static int inSchema( Scope, Scope );
static void addRenameTypedefs( Schema, FILE * );
static void addAggrTypedefs( Schema , FILE * );
static void addUseRefNames( Schema, FILE * );

void print_schemas_separate( Express express, void *complexCol, FILES *files )
    /*
     * Generates the C++ files corresponding to a list of schemas.  Does so in
     * multiple passes through the schemas.  In each pass it checks for enti-
     * ties which are subtypes of entites in other schemas which have not yet
     * been processed.  Such entities cannot be processed in that pass until
     * their supertypes have been defined.  It also checks for entities which
     * have enum or select attributes which have not been processed, and for
     * select types which have enum or select items (or entities containing
     * enums) which have not been processed.
     */
{
    int complete = FALSE, val1, val2, suffix;
    DictionaryEntry de;
    Schema schema;

    /* First set all marks we'll be using to UNPROCESSED/NOTKNOWN: */
    initializeMarks( express );

    fprintf (files->create, "    Interface_spec_ptr is;\n    Used_item_ptr ui;\n    Referenced_item_ptr ri;\n    Uniqueness_rule_ptr ur;\n    Where_rule_ptr wr;\n    Global_rule_ptr gr;\n");
    while ( !complete ) {
	complete = TRUE;
	DICTdo_type_init(express->symbol_table,&de,OBJ_SCHEMA);
	while ( (schema = (Scope)DICTdo(&de)) != 0 ) {
	    if ( schema->search_id == UNPROCESSED ) {
		/* i.e., if the schema has more ents/types to process in it */
		unsetObjs( schema );
		/* Unset the ones which had search_id = CANTPROCESS.  We're
		// going to check that again since things may have changed by
		// this pass.  The ones with search_id = PROCESSED do not
		// change since we're done with them.  */
		schema->search_id = PROCESSED;
		/* We assume this is the case unless something goes wrong. */
		val1 = checkTypes( schema );
		val2 = checkEnts( schema );
		/* The check functions recheck all the ents, types, USEd, and
		// REFs which are still NOTKNOWN to see if we can process any
		// more this pass.  If any returns TRUE, we'll process again
		// this round. */
		if ( val1 || val2 ) {
		    if ( schema->search_id == UNPROCESSED ||
			*(int *)schema->clientData > 0 ) {
			/* What we're trying to determine here is if we will
			// need to print multiple files for this schema.  If
			// we're already beyond a first file (2nd condition)
			// or we're at the first but there are more entities
			// which couldn't be processed yet (and thus search_id
			// was still set to UNPROCESSED), this schema will be
			// printed in multiple files.  If so, SCHEMAprint()
			// will create files with the suffixes "_1", "_2", etc.
			// If not, no file suffix will be added. */
			suffix = ++*(int *)schema->clientData;
			SCHEMAprint( schema, files, express, complexCol,
				     suffix );
		    } else {
			SCHEMAprint( schema, files, express, complexCol, 0 );
		    }
		}
		complete = complete && (schema->search_id == PROCESSED);
		/* Job's not complete so long as schema still has entities it
		// had to skip. */
	    }
	}
    }
	
/*******************
*******************/

	DICTdo_type_init(express->symbol_table,&de,OBJ_SCHEMA);
	while ( (schema = (Scope)DICTdo(&de)) != 0 )
	{
	fprintf (files->create,
		"\t//////////////// USE statements\n");
#if 0
	str = REFto_string(schema->u.schema->usedict,schema->u.schema->uselist,"USE",0);
	fprintf (files->create,
		"\t/*\n%s\n\t*/\n", str);
	free(str);
#endif
	USEREFout(schema, schema->u.schema->usedict,schema->u.schema->use_schemas,"USE",files->create);

	fprintf (files->create,
		"\t//////////////// REFERENCE statements\n");
#if 0
	str = REFto_string(schema->u.schema->refdict,schema->u.schema->reflist,"REFERENCE",0);
	fprintf (files->create,
		"\t/*\n%s\n\t*/\n", str);
	free(str);
#endif
	USEREFout(schema,schema->u.schema->refdict,schema->u.schema->ref_schemas,"REFERENCE",files->create);
    }
/*****************
*****************/
    /* Before closing, we have three more situations to deal with (i.e., three
    // types of declarations etc. which could only be printed at the end).
    // Each is explained in the header section of its respective function. */
    DICTdo_type_init(express->symbol_table,&de,OBJ_SCHEMA);
    while ( (schema = (Scope)DICTdo(&de)) != 0 ) {
	/* (These two tasks are totally unrelated but are done in the same loop
	// for efficiency.) */
	addRenameTypedefs( schema, files->classes );
	addUseRefNames( schema, files->create );
    }
    /* Third situation:  (Must be dealt with after first, see header comments
    // of addAggrTypedefs.) */
    DICTdo_type_init(express->symbol_table,&de,OBJ_SCHEMA);
    while ( (schema = (Scope)DICTdo(&de)) != 0 ) {
	addAggrTypedefs( schema, files->classes );
    }

    /* On our way out, print the necessary statements to add support for
    // complex entities.  (The 1st line below is a part of SchemaInit(),
    // which hasn't been closed yet.  (That's done on 2nd line below.)) */
    fprintf( files->initall, "\t reg.SetCompCollect( gencomplex() );\n");
    fprintf( files->initall, "}\n\n");
    fprintf( files->incall,  "\n#include <complexSupport.h>\n");
    fprintf( files->incall,  "ComplexCollect *gencomplex();\n");

    /* Function GetModelContents() is printed at the end of the schema.xx
    // files.  This is done in a separate loop through the schemas, in function
    // below. */
    getMCPrint( express, files->incall, files->initall );
}

static void initializeMarks( Express express )
    /*
     * Set all schema->search_id's to UNPROCESSED, meaning we haven't processed
     * all the ents and types in it yet.  Also, put an int=0 in each schema's
     * clientData field.  We'll use it to record what # file we're generating
     * for each schema.  Set all entity and type search_id's to NOTKNOWN mean-
     * we don't know if we can process it yet.  (An ent & select type may have
     * an attribute/item which comes from another schema.  All other types can
     * be processed the first time, but that will be caught in checkTypes().)
     */
{
    DictionaryEntry de_sch, de_ent, de_type;
    Schema schema;

    DICTdo_type_init(express->symbol_table,&de_sch,OBJ_SCHEMA);
    while ( (schema = (Scope)DICTdo(&de_sch)) != 0 ) {
	schema->search_id = UNPROCESSED;
	schema->clientData = (int *)malloc(sizeof(int));
	*(int *)schema->clientData = 0;
	SCOPEdo_entities(schema,ent,de_ent)
	    ent->search_id = NOTKNOWN;
	SCOPEod
	SCOPEdo_types(schema,t,de_type)
	    t->search_id = NOTKNOWN;
	SCOPEod
    }
}

static void unsetObjs( Schema schema )
    /*
     * Resets all the ents & types of schema which had been set to CANTPROCRSS
     * to NOTKNOWN.  This function is called every time print_schemas_separate
     * iterates through the schemas, printing to file what can be printed.  At
     * each pass we re-examine what ents/types can be processed.  Ones which
     * couldn't be processed at the last pass may be processable now.  Ents/
     * types which have already been marked PROCESSED will not have to be
     * revisited, and are not changed.
     */
{
    DictionaryEntry de;
    Rename *r;

    SCOPEdo_types(schema,t,de)
        if ( t->search_id == CANTPROCESS ) {
	    t->search_id = NOTKNOWN;
	}
    SCOPEod
    SCOPEdo_entities(schema,ent,de)
        if ( ent->search_id == CANTPROCESS ) {
	    ent->search_id = NOTKNOWN;
	}
    SCOPEod
}

static int checkTypes( Schema schema )
    /*
     * Goes through the types contained in this schema checking for ones which
     * can't be processed.  This may be the case if:  (1) We have a select type
     * which has enumeration or select items which have not yet been defined
     * (are defined in a different schema we haven't processed yet).  (2) We
     * have a select which has an entity item which contains unprocessed enums.
     * (Unproc'ed selects, however, do not pose a problem here for reasons
     * beyond the scope.)  (3) We have an enum type which is a redefinition of
     * an enum not yet defined.  If we find any such type, we set its mark to
     * CANTPROCESS.  If some types in schema *can* be processed now, we return
     * TRUE.  (See relevant header comments of checkEnts() below.)
     */
{
    DictionaryEntry de;
    int retval = FALSE, unknowncnt;
    Type i;
    Entity ent;
    Linked_List attribs;

    do {
	unknowncnt = 0;
	SCOPEdo_types( schema, type, de )
	    if ( type->search_id != NOTKNOWN ) continue;
	    /* We're only interested in the ones which haven't been processed
	    // already or accepted (set to CANPROCESS in a previous pass thru
	    // the do loop) already. */
	    type->search_id = CANPROCESS;
	    /* Assume this until disproven. */

	    if ( TYPEis_enumeration(type) && TYPEget_head(type) ) {
		i = TYPEget_ancestor(type);
		if ( !sameSchema( i, type ) && i->search_id != PROCESSED ) {
		    /* Note - if, however, i is in same schema, we're safe: We
		    // know it'll be processed this pass because enum's are
		    // always processed on the first pass.  (We do have to take
		    // care to process the original enum before the redefined.
		    // This is done in SCOPEPrint, in classes_wrapper.cc.) */
		    type->search_id = CANTPROCESS;
		    schema->search_id = UNPROCESSED;
		}
	    } else if ( TYPEis_select(type) ) {
		LISTdo ( SEL_TYPEget_items(type), i, Type )
		    if ( !TYPEis_entity(i) ) {
			if ( checkItem( i, type, schema, &unknowncnt, 0 ) ) {
			    break;
			}
			/* checkItem does most of the work of determining if
			// an item of a select will make the select type un-
			// processable.  It checks for conditions which would
			// make this true and sets values in type, schema, and
			// unknowncnt accordingly.  (See checkItem's commenting
			// below.)  It also return TRUE if i has made type un-
			// processable.  If so, we break - there's no point
			// checking the other items of type any more. */
		    } else {
			/* Check if our select has an entity item which itself
			// has unprocessed selects or enums. */
			ent = ENT_TYPEget_entity(i);
			if ( ent->search_id == PROCESSED ) continue;
			/* If entity has been processed already, things must be
			// okay. (Note - but if it hasn't been processed yet we
			// may still be able to process type.  This is because
			// a sel type will only contain a pointer to an entity-
			// item (and we can create a pointer to a not-yet-pro-
			// cessed object), while it will contain actual objects
			// for the enum and select attributes of ent.) */
			attribs = ENTITYget_all_attributes( ent );
			LISTdo( attribs, attr, Variable )
			    if ( checkItem( attr->type, type, schema,
					    &unknowncnt, 1 ) ) {
				break;
			    }
			LISTod
			LISTfree( attribs );
		    }
		LISTod
		/* One more condition - if we're a select which is a rename of
		// another select - we must also make sure the original select
		// is in this schema or has been processed.  Since a rename-
		// select is defined with typedef's to the original, we can't
		// do that if the original hasn't been defined. */
		if (    ( type->search_id == CANPROCESS )
		     && ( (i = TYPEget_ancestor(type)) != NULL )
		     && ( !sameSchema( i, type ) )
		     && ( i->search_id != PROCESSED ) ) {
		    type->search_id = CANTPROCESS;
		    schema->search_id = UNPROCESSED;
		}
	    }

	    if ( type->search_id == CANPROCESS ) {
		/* NOTE - This condition will be met if type isn't a select or
		// enum at all and above if was never entered (and it's our
		// first pass so type hasn't been processed).  So for non-enums
		// and selects, checkTypes() will simply check the type off and
		// go on. */
		retval = TRUE;
	    }
	SCOPEod
    } while ( unknowncnt > 0 );
    /* We loop to deal with the following situation:  Say sel A contains enum B
    // as an item, but A appears earlier in the EXPRESS file than B.  In such a
    // case, we really can process A now since it doesn't depend on anything
    // which won't be processed till later.  But when we first reach A, item B
    // will have value NOTKNOWN, and at the time we won't know if B will be set
    // to CANPROCESS when we get to it.  To deal with this, checkItem() below
    // increments unknowncnt for every item which is in this schema and still
    // has search_id = NOTKNOWN.  (Not if id = CANTPROCESS, which means we've
    // tried already this pass and B still can't be processed.)  Here, we keep
    // looping until all the unknowns are resolved.  (It may take may passes if
    // selX has item selY which has item selZ.)  (I take all the trouble with
    // this so as not to generate mult files in unnecessary cases where e.g.
    // the EXPRESS is in a single schema.) */

    return retval;
}

static int checkEnts( Schema schema )
    /*
     * Goes through the entities contained in this schema checking for ones
     * which can't be processed.  It checks for two situations:  (1) If we find
     * an entity which is a subtype of a not-yet-processed entity in another
     * schema.  (2) If an entity has an attribute which is an enumeration or
     * select type (which is implemented by fedex_plus as a C++ class), and the
     * enum or select class has not yet been defined.  For each entity which
     * satisfies one of the above conditions, we set its mark and the marks of
     * all its subtype descendents to CANTPROCESS.  Later, when C++ files are
     * generated for this schema, the ents marked CANTPROCESS will be skipped.
     * checkEnts() will return TRUE if there are some ents/types which *can* be
     * processed now.  This will tell later functions that there are some pro-
     * cessable entities at this pass and C++ files should be generated.  (Some
     * of the inline commenting of checkTypes() is applicable here and is not
     * repeated.)
     */
{
    Entity super;
    DictionaryEntry de;
    int retval = FALSE, ignore = 0;

    /* Loop through schema's entities: */
    SCOPEdo_entities( schema, ent, de )
        if ( ent->search_id != NOTKNOWN ) continue;
        /* ent->search_id may = CANTPROCESS signifying we've already determined
	// that this ent is dependent on an undefined external one.  (It got
	// its mark already because it was the descendent of a parent entity we
	// already checked and rejected.)  ent->search_id may = PROCESSED.  In
	// such a case there of course is also nothing to check now. */
        ent->search_id = CANPROCESS;

        /* First traverse ent's supertypes.  If any is from a different schema
	// and is not yet defined, ent will have to wait. */
        LISTdo( ENTITYget_supertypes( ent ), super, Entity )
	    if (    ( !sameSchema( ent, super ) )
		 && ( super->search_id != PROCESSED ) ) {
		markDescs( ent );
		schema->search_id = UNPROCESSED;
		break;
		/* Exit the LISTdo loop.  Since we found an unprocessed
		// parent, we're done with this entity. */
	    }
        LISTod

	/* Next traverse ent's attributes, looking for attributes which are
	// not yet defined (more explanation in checkItem()). */
	if ( ent->search_id == CANPROCESS ) {
	    /* Only do next test if ent hasn't already failed the 1st. */
	    LISTdo( ENTITYget_attributes( ent ), attr, Variable )
	        if ( checkItem( attr->type, ent, schema, &ignore, 0 ) ) {
		    markDescs( ent );
		    break;
		}
	    LISTod
	}

        if ( ent->search_id == CANPROCESS ) {
	    /* If ent's mark still = CANPROCESS and not CANTPROCESS, it
	    // must still be processable.  Set retval to TRUE signifying
	    // that there are ent's we'll be able to process. */
	    retval = TRUE;
	}
    SCOPEod
    /* NOTE - We don't have to loop here as in checkTypes() (see long comment
    // there).  It was necessary there because we may have processed type
    // sel1 before its member enum1.  Whereas here, since all types are done
    // before selects (checkTypes() is called before checkEnts()), there is
    // no such concern. */

    return retval;
}

static void markDescs( Entity ent )
    /*
     * Sets the mark value of ent and all its subtypes to CANTPROCESS.  This
     * function is called if we've determined that ent is a subtype of an
     * entity defined in a different schema which has not yet been processed.
     */
{
    ent->search_id = CANTPROCESS;
    LISTdo( ENTITYget_subtypes( ent ), sub, Entity )
        markDescs( sub );
    LISTod
}

static int checkItem( Type t, Scope parent, Schema schema, int *unknowncnt,
		      int noSel )
    /*
     * Function with a lot of side effects: Checks if type t, a member of
     * `parent' makes parent unprocessable.  parent may be an entity and t is
     * its attribute.  parent may be a select type and t is one of its items.
     * parent may be a select type and t an attribute of one of its entity
     * items.  Lastly, parent may be an aggregate and t its base type.  t will
     * make parent unprocessable if it is an enum or sel which hasn't been
     * processed yet, see inline commenting.  If so, parent->search_id is set
     * to CANTPROCESS, and schema->search_id is set to UNPROCESSED (i.e., we're
     * not done yet).  Also, if one of parent's items is still NOTKNOWN (caused
     * if it's in our schema and we haven't gotten to it yet - see comment, end
     * of checkTypes()), parent is set to NOTKNOWN and unknowncnt is incremen-
     * ted to tell us that we'll have to try parent again after its item has
     * been processed (i.e., we have to repeat the do-loop in checkTypes()).
     * checkItem returns TRUE if parent became unprocessable; FALSE otherwise.
     *
     * NOTE:  noSel deals with the following:  Say selA has member entX.  If
     * entX has attribute enumP, it creates a problem, while if entX has attr
     * selB, it is not.  When using checkItem for an ent member of a select,
     * noSel is set to 1 to tell it to worry about t if it's an enum but not
     * if it's a select.
     */
{
    Type i = t;

    if ( isAggregateType(t) ) {
	i = TYPEget_base_type(t);
	/* NOTE - If t is a 2D aggregate or higher, we do not go down to its
	// lowest base type.  An item which is a higher dimension aggregates
	// does not make its parent unprocessable.  All an e.g. entity needs
	// defined to have a 2D aggr attribute is GenericAggregate (built in) 
	// and a typedef for a pointer to the type name, which is declared in
	// Sdaiclasses.h. */
    }

    if ( TYPEis_enumeration(i) && !ENUMcanBeProcessed( i, schema ) ) {
	/* Enum's are usually processed on the first try.  ENUMcanBeProcessed()
	// checks for cases of renamed enum's, which must wait for the enum i
	// is a rename of. */
	if ( parent->search_id == NOTKNOWN ) {
	    /* We had thought parent's val was going to be NOTKNOWN - i.e.,
	    // dependent on other selects in this schema which haven't been
	    // processed.  When we set it to NOTKNOWN we also incremented
	    // unknowncnt.  Now we see it's not going to be unknown so we
	    // decrement the count: */
	    (*unknowncnt)--;
	}
	parent->search_id = CANTPROCESS;
	schema->search_id = UNPROCESSED;
	return TRUE;
    } else if ( TYPEis_select(i) && !noSel ) {
	if ( !sameSchema( i, parent ) ) {
	    if ( i->search_id != PROCESSED ) {
		if ( parent->search_id == NOTKNOWN ) {
		    (*unknowncnt)--;
		}
		parent->search_id = CANTPROCESS;
		schema->search_id = UNPROCESSED;
		return TRUE;
	    }
	} else {
	    /* We have another sel in the same schema.  This gets complicated -
	    // it may be processable but we just haven't gotten to it yet.  So
	    // we may have to wait on parent. */
	    if ( i->search_id == CANTPROCESS ) {
		/* We *have* checked i already and it can't be processed. */
		if ( parent->search_id == NOTKNOWN ) {
		    (*unknowncnt)--;
		}
		parent->search_id = CANTPROCESS;
		schema->search_id = UNPROCESSED;
		return TRUE;
	    } else if ( i->search_id == NOTKNOWN ) {
		/* We haven't processed i this pass. */
		if ( parent->search_id != NOTKNOWN ) {
		    parent->search_id = NOTKNOWN;
		    /* We lower parent's value.  But don't return TRUE.  That
		    // would tell checkTypes() that there's nothing more to
		    // check.  But checkTypes should keep looping thru the re-
		    // maining items of parent - maybe one of them will tell us
		    // that parent definitely can't be processed this pass. */
		    (*unknowncnt)++;
		}
	    }
	}
    }
    return FALSE;
}

static int ENUMcanBeProcessed( Type e, Schema s )
    /*
     * Tells us if an enumeration type has been processed already, or if not
     * will be processed this pass through schema s.  As always, I take great
     * pains to avoid breaking up a schema into >1 file unnecessarily.  In
     * cases where a user is building a library based on a single schema, we
     * shouldn't cause him to suffer side-effects of the mult schema support,
     * because of a case that "looks like" it may require multiple files.
     */
{
    Type a;

    if ( !inSchema( e, s ) ) {
	/* If e is not in s - the schema we're processing now - things are
	// fairly simple.  Nothing is going to change by the time we finish
	// with this schema.  Base the return val on whether or not e *was*
	// processed already. */
	return ( e->search_id == PROCESSED );
    }

    if ( e->search_id != NOTKNOWN ) {
	/* Next case: e is in our schema, but either it's been processed
	// already, or we've determined that it can or can't be processed.
	// This case is also relatively simple - we have nothing more to
	// figure out here. */
	return ( e->search_id >= CANPROCESS );
	/* PROC/CANPROC - TRUE; UNPROC'ED/CANTPROC - FALSE */
    }

    /* Remaining case: e is in our schema and still = NOTKNOWN.  I.e., we
    // haven't gotten to e this pass and don't yet know whether it'll be
    // processable.  Figure that out now: */
    if ( (a = TYPEget_ancestor(e)) == NULL ) {
	/* If e is not a rename of anything, it should be processed now. */
	return TRUE;
    }
    if ( inSchema( a, s ) || a->search_id == PROCESSED ) {
	/* If e's ancestor (the one it's a rename of) is in our schema it will
	// be processed now.  If not, it must have been processed already. */
	return TRUE;
    }
    return FALSE;
}

int sameSchema( Scope sc1, Scope sc2 )
    /*
     * Checks if sc1 and sc2 are in the same superscope.  Normally called for
     * two types to see if they're in the same schema.
     */
{
    return ( !strcmp( SCOPEget_name(sc1->superscope),
		      SCOPEget_name(sc2->superscope) ) );
}

static int inSchema( Scope scope, Scope super )
    /*
     * Checks if scope is contained in super's scope.
     */
{
    return ( !strcmp( SCOPEget_name(scope->superscope),
		      SCOPEget_name(super) ) );
}

static void addRenameTypedefs( Schema schema, FILE *classes )
    /*
     * Prints typedefs at the end of Sdaiclasses.h for enumeration or select
     * types which are renamed from other enum/sel's.  Since the original e/s
     * may be in any schema, this must be done at the end of all the schemas.
     * (Actually, for the enum only the aggregate class name is written in
     * Sdaiclasses.h (needs to have forward declarations here).)
     */
{
    DictionaryEntry de;
    Type i;
    char nm[BUFSIZ], basenm[BUFSIZ];
    static int firsttime = TRUE;

    SCOPEdo_types(schema,t,de)
        if (    ( TYPEis_enumeration(t) || TYPEis_select(t) )
	     && ( (i = TYPEget_ancestor(t)) != NULL ) ) {
	    /* I.e., t is a renamed enum/sel type.  i is set to the orig enum/
	    // sel t is based on (in case it's a rename of a rename etc). */
	    if ( firsttime ) {
		fprintf( classes, "\n// Renamed enum and select" );
		fprintf( classes, " types (from all schemas):\n" );
		firsttime = FALSE;
	    }
	    if ( TYPEis_enumeration(t) ) {
		strncpy( nm, TYPEget_ctype(t), BUFSIZ-1 );
		strncpy( basenm, TYPEget_ctype(i), BUFSIZ-1 );
		fprintf( classes, "typedef %ss\t%ss;\n", basenm, nm );
	    } else {
		strncpy( nm, SelectName (TYPEget_name(t)), BUFSIZ-1 );
		strncpy( basenm, SelectName( TYPEget_name(i) ), BUFSIZ-1 );
		fprintf (classes, "typedef %s %s;\n", basenm, nm);
		fprintf (classes, "typedef %s * %s_ptr;\n", nm, nm);
		fprintf (classes, "typedef %ss %ss;\n", basenm, nm);
		fprintf (classes, "typedef %ss * %ss_ptr;\n", nm, nm);
	    }
	}
    SCOPEod
}

static void addAggrTypedefs( Schema schema, FILE *classes )
    /*
     * Print typedefs at the end of Sdiaclasses.h for aggregates of enum's and
     * selects.  Since the underlying enum/sel may appear in any schema, this
     * must be done at the end of all the schemas.  Note that this function is
     * called after addRenameTypedefs() since an aggregate may also be based on
     * one of the renamed enum/sel's defined there.
     */
{
    DictionaryEntry de;
    Type i;
    static int firsttime = TRUE;
    char nm[BUFSIZ];

    SCOPEdo_types(schema,t,de)
        if ( TYPEis_aggregate(t) ) {
	    i = TYPEget_base_type(t);
	    if ( TYPEis_enumeration(i) || TYPEis_select(i) ) {
		/* This if will pass if t was a 1D aggregate only.  They are
		// the only types which had to wait for their underlying type.
		// 2D aggr's and higher only need type GenericAggr defined
		// which is built-in. */
		if ( firsttime ) {
		    fprintf( classes, "\n// Aggregate types (from all sche" );
		    fprintf( classes, "mas) which depend on other types:\n" );
		    firsttime = FALSE;
		}
		strncpy( nm, ClassName( TYPEget_name(t) ), BUFSIZ );
		fprintf( classes, "typedef %s\t%s;\n",
			 TYPEget_ctype(t), nm );
		fprintf( classes, "typedef %s *\t%sH;\n", nm, nm );
		fprintf( classes, "typedef %s *\t%s_ptr;\n", nm, nm );
	    }
	}
    SCOPEod
}

static void addUseRefNames( Schema schema, FILE *create )
    /*
     * Checks the USE and REFERENCE dicts contained in schema.  If either dict
     * contains items (types or entities) which are renamed in this schema,
     * code is written to add another member to the "altNames" list of the
     * corresponding TypeDescriptor or EntityDescriptor object in the SCL.  The
     * list will be used in the SCL to use the correct name of this type or
     * entity when reading and writing files.
     */
{
    Dictionary useRefDict;
    DictionaryEntry de;
    Rename *rnm;
    char *oldnm, schNm[BUFSIZ];
    static int firsttime = TRUE;

    if ( (useRefDict = schema->u.schema->usedict) != NULL ) {
	DICTdo_init( useRefDict, &de );
	while ( (rnm = (Rename *)DICTdo(&de)) != 0 ) {
	    oldnm = ((Scope)rnm->object)->symbol.name;
	    if ( (strcmp( oldnm, rnm->nnew->name ) ) ) {
		/* strcmp != 0, so old and new names different.
		// Note: can't just check if nnew != old.  That wouldn't
		// catch following:  schema C USEs obj Y from schema B
		// (not renamed).  B USEd it from schema A and renamed it
		// from X.  nnew would = old, but name would not be same
		// as rnm->object's name. */
		if ( firsttime ) {
		    fprintf( create, "\t// Alternate names for types and " );
		    fprintf( create, "entities when used in other schemas:\n" );
		    firsttime = FALSE;
		}
		if ( rnm->type == OBJ_TYPE ) {
		    fprintf( create, "\t%s", TYPEtd_name((Type)rnm->object ) );
		} else {
		    /* must be an entity */
		    fprintf( create, "\t%s%s%s",
			     SCOPEget_name(((Entity)rnm->object)->superscope),
			     ENT_PREFIX, ENTITYget_name((Entity)rnm->object) );
		}
		strcpy( schNm, PrettyTmpName( SCHEMAget_name(schema) ) );
		fprintf( create, "->addAltName( \"%s\", \"%s\" );\n",
			 schNm, PrettyTmpName( rnm->nnew->name ) );
	    }
	}
    }
    if ( (useRefDict = schema->u.schema->refdict) != NULL ) {
	DICTdo_init( useRefDict, &de );
	while ( (rnm = (Rename *)DICTdo(&de)) != 0 ) {
	    oldnm = ((Scope)rnm->object)->symbol.name;
	    if ( (strcmp( oldnm, rnm->nnew->name ) ) ) {
		if ( firsttime ) {
		    fprintf( create, "\t// Alternate names for types and " );
		    fprintf( create, "entities when used in other schemas:\n" );
		    firsttime = FALSE;
		}
		if ( rnm->type == OBJ_TYPE ) {
		    fprintf( create, "\t%s", TYPEtd_name((Type)rnm->object ) );
		} else {
		    fprintf( create, "\t%s%s%s",
			     SCOPEget_name(((Entity)rnm->object)->superscope),
			     ENT_PREFIX, ENTITYget_name((Entity)rnm->object) );
		}
		strcpy( schNm, PrettyTmpName( SCHEMAget_name(schema) ) );
		fprintf( create, "->addAltName( \"%s\", \"%s\" );\n",
			 schNm, PrettyTmpName( rnm->nnew->name ) );
	    }
	}
    }
}
