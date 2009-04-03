/*****************************************************************************
 * expressbuild.cc                                                           *
 *                                                                           *
 * Description: Contains all the functions relevant to building a Complex-   *
 *              Collect out of an EXPRESS file.  The complex structures are  *
 *              built out of fedex structures.  Subtype and supertype info   *
 *              are processed, and multi-generation trees (e.g., subtypes of *
 *              subtypes) are built up from the fedex structures.            *
 *                                                                           *
 * Created by:  David Rosenfeld                                              *
 * Date:        01/03/97                                                     *
 *****************************************************************************/

#include "complexSupport.h"

// Local function prototypes:
static void initEnts( Express );
static Entity findEnt( Entity, char * );

ComplexCollect::ComplexCollect( Express express )
    /*
     * Builds a ComplexCollect, a collection of ComplexLists, based on the
     * entities contained in EXPRESS file express.
     */
{
    DictionaryEntry de_sch, de_ent;
    Schema schema;
    Expression exp;
    Linked_List subs;
    ComplexList *cl, *prev = NULL, *nextlist;

    // Some initializing:
    clists = NULL;
    count = 0;

    // Set all ent->search_id's to 0.  Since entities - even ones in different 
    // schemas - may be strongly connected, we must be sure not to process each
    // one more than once.
    initEnts( express );

    // Next loop through all the entities, building ComplexLists:
    DICTdo_type_init(express->symbol_table,&de_sch,OBJ_SCHEMA);
    while ( (schema = (Scope)DICTdo(&de_sch)) != 0 ) {
	SCOPEdo_entities(schema,ent,de_ent)
	    if ( ent->search_id == TRUE ) {
		// we've hit this entity already
		continue;
	    }
#ifdef COMPLEX_INFO
	    cout << "Processing entity " << ENTITYget_name( ent ) << endl;
#endif
	    if ( ent->u.entity->subtypes != NULL ) {
		cl = new ComplexList( ent, this );
		// This constructor will not only create a ComplexList for
		// the ent subtypes, but it will recurse for all of their
		// subtypes.  The entire hierarchy will become a single
		// ComplexList and is now appended to the Collect ("this").
		insert( cl );
	    }
	SCOPEod
    }

    // Remove all Lists corresponding to entities which are subtypes of other
    // entities.  Such Lists should not exist on their own since their supers
    // supercede them.  (They were added in the first place to be available
    // so that any supertype which accessed it would find it.)
    cl = clists;
    while ( cl ) {
	if ( cl->Dependent() ) {
#ifdef COMPLEX_INFO
	    cout << "\nRemoving dependent entity " << cl->supertype() << endl;
#endif
	    remove( cl );
	    if ( prev ) {
		cl = prev->next;
		// prev->next was automatically set to cl->next in remove()
		// when cl was removed.
	    } else {
		// prev may still = NULL (its starting value) if the first CList
		// is dependent, and so we're the 1st time through this loop.
		cl = clists;
	    }
	} else {
	    prev = cl;
	    cl = cl->next;
	}
    }
}

static void initEnts( Express express )
    /*
     * Sets all the search_id's of all the entities to FALSE.  The search_id's
     * will be used to keep track of which entities we've build ComplexLists
     * for already.
     */
{
    DictionaryEntry de_sch, de_ent;
    Schema schema;

    DICTdo_type_init(express->symbol_table,&de_sch,OBJ_SCHEMA);
    while ( (schema = (Scope)DICTdo(&de_sch)) != 0 ) {
	SCOPEdo_entities(schema,ent,de_ent)
	    ent->search_id = FALSE;
	SCOPEod
    }
}

ComplexList::ComplexList( Entity ent, ComplexCollect *col )
    /*
     * Builds a complex list from an entity which contains subtypes.  (All our
     * members are set here or in called functions except next which is set
     * when this is inserted into owning ComplexCollect.)
     */
{
    Expression exp;

    ent->search_id = TRUE;
    list = NULL;
    next = NULL;

    addSuper( ent );
    if ( (exp = ent->u.entity->subtype_expression) != NULL ) {
#ifdef COMPLEX_INFO
	cout << "  Has a sub expression\n";
#endif
	head->processSubExp( exp, ent, col );
	buildList();
    }
    // Check for any subtypes which were not a part of subtype_expr.  Any
    // subtype which was not in sub_exp but is included in subtypes is ANDORed
    // with the rest of the List.
    addImplicitSubs( ENTITYget_subtypes( ent ), col );

    if ( ENTITYget_supertypes( ent ) == NULL ) {
	dependent = FALSE;
	// Rebuild list in case implicit subs were added (we had to build the
	// first time also so addImplicitSubs() would work).
	buildList();
	head->setLevel( 0 );
    } else {
	// If this List has supertypes, we don't really need it as a List -
	// it will ultimately be a part of its super(s)' List(s).  We need it
	// now so its supers will be able to find it.  But mark that this
	// does not stand on its own:
#ifdef COMPLEX_INFO
	cout << "  " << ENTITYget_name( ent ) << " is dependent\n";
#endif
	dependent = TRUE;
    }
}

void ComplexList::addSuper( Entity ent )
    /*
     * Sets our supertype.  Assumes supertype was previously unset.
     */
{
    head = new AndList;
    head->supertype = TRUE;
    // (Although this supertype may itself be a subtype of other supertypes,
    // we call this a supertype.  We only need this info during the list-
    // creation stage (see MultList::processSubExp()).)
    head->childList = new SimpleList( ENTITYget_name( ent ) );
    head->numchildren = 1;
}

void MultList::processSubExp( Expression exp, Entity super,
			      ComplexCollect *col )
    /*
     * Recursive function which builds an EntList hierarchy from an entity's
     * subtype expression.  First called with this = the ComplexList->head and
     * exp = entity->subtype_expression.  Recurses with different arguments to
     * process the subexpressions.
     */
{
    struct Op_Subexpression *oe = &exp->e;
    Entity ent;
    MultList *mult;
    int supertype = 0;

    switch (TYPEis(exp->type)) {
      case entity_:
	ent = findEnt( super, exp->type->symbol.name );
#ifdef COMPLEX_INFO
	cout << "    Adding subtype " << ENTITYget_name(ent) << endl;
#endif
	addSimpleAndSubs( ent, col );
	break;
      case op_:
	if ( join == AND ) {
	    supertype = ((AndList *)this)->supertype;
	}
	if ( ! supertype &&
	     (    ( oe->op_code == OP_AND && join == AND )
	       || ( oe->op_code == OP_ANDOR && join == ANDOR ) ) ) {
	    // If the subexp is of the same type as we, process its op's at
	    // the same level (add them on to our childList).  1st cond says
	    // we don't do this if this is the supertype.  In that case, the
	    // supertype is AND'ed with its subtypes.  The subtypes must be at
	    // a lower level.  One reason for this is in case we find implicit
	    // subtypes, we'll want to ANDOR them with the rest of the subs.
	    // So we'll want the subs at a distinct lower level.
	    processSubExp( oe->op1, super, col );
	    processSubExp( oe->op2, super, col );
	} else {
	    if ( oe->op_code == OP_AND ) {
#ifdef COMPLEX_INFO
		cout << "    Processing AND\n";
#endif
		mult = new AndList;
	    } else {
#ifdef COMPLEX_INFO
		cout << "    Processing ANDOR\n";
#endif
		mult = new AndOrList;
	    }
	    appendList( mult );
	    mult->processSubExp( oe->op1, super, col );
	    mult->processSubExp( oe->op2, super, col );
	}
	break;
      case oneof_:
#ifdef COMPLEX_INFO
	cout << "    Processing ONEOF\n";
#endif
	mult = new OrList;
	appendList( mult );
	LISTdo(exp->u.list,arg,Expression)
	    mult->processSubExp( arg, super, col );
	LISTod
	break;
      default:
	// shouldn't happen
	break;
    }
}

static Entity findEnt( Entity ent0, char *name )
    /*
     * Returns an entity named name.  The desired entity is likely to be in the
     * same schema as ent0.  findEnt first searches the schema which contains
     * ent, and then searches the other schemas in the express file.
     */
{
    Schema schema = ENTITYget_schema( ent0 ), sch;
    DictionaryEntry de_ent, de_sch;
    Express express;

    // First look through the entities in the same schema as ent0:
    SCOPEdo_entities(schema,ent,de_ent)
        if ( !strcmp( ENTITYget_name( ent ), name ) ) {
	    return ent;
	}
    SCOPEod

    // If we still haven't found it, look through all the entities in the
    // express file:
    express = schema->superscope;
    DICTdo_type_init(express->symbol_table,&de_sch,OBJ_SCHEMA);
    while ( (sch = (Scope)DICTdo(&de_sch)) != 0 ) {
	if ( sch == schema ) {
	    // Don't redo the schema which contains ent0 - we did it already.
	    continue;
	}
	SCOPEdo_entities(sch,ent,de_ent)
	    if ( !strcmp( ENTITYget_name( ent ), name ) ) {
		return ent;
	    }
	SCOPEod
    }
    return NULL;
    // Shouldn't happen - if the entity didn't exist, fedex would have
    // complained already.
}

void ComplexList::addImplicitSubs( Linked_List subs, ComplexCollect *col )
    /*
     * Checks if there are any subtypes of entity this->supertype() which were
     * not in the entity's subtype_expression.  (subs is the entity's subtypes
     * list.)  If any are found they are ANDORed with the other subtypes.
     */
{
    EntNode node;
    // Temp var - used to check if this already contains certain values.
    int none_yet = TRUE;
    AndOrList *ao;
    SimpleList *simple;

    LISTdo( subs, subEnt, Entity )
        strcpy( node.name, ENTITYget_name( subEnt ) );
        if ( !contains( &node ) ) {
	    // We've found an implicit subtype.
#ifdef COMPLEX_INFO
	    cout << "  Adding implicit subtype " << ENTITYget_name( subEnt )
	         << endl;
#endif
	    if ( none_yet ) {
		// If this is the first one, replace the previous subtype list
		// with an ANDOR.
		none_yet = FALSE;
		ao = new AndOrList;
		// Make the previous sub exp a child of ao:
		ao->childList = head->childList->next;
		if ( ao->childList ) {
		    ao->childList->prev = NULL;
		    ao->numchildren = 1;
		} else {
		    ao->numchildren = 0;
		}
		head->childList->next = ao;
		ao->prev = head->childList;
		head->numchildren = 2;
	    }
	    // Add the new entity to the end of ao.  In case it has its own
	    // subtype list, call addSimpleAndSubs().
	    ao->addSimpleAndSubs( subEnt, col );
	}
    LISTod
}    

void MultList::addSimpleAndSubs( Entity newEnt, ComplexCollect *col )
    /*
     * Called whenever we have a SimpleList (to be built from newEnt) to add
     * to our ComplexList.  The purpose of this function is to check if the
     * Simple is itself a supertype of its own List.  If so, we will build a
     * ComplexList to be placed beneath this.  If not, we will simply create a
     * SimpleList corresponding to newEnt.
     */
{
    ComplexList *sublist;
    SimpleList *simple;
    EntList *newlist;
    OrList *olist;
	
    // First get the easy case out of the way.  If newEnt has no subtypes
    // just create a corresponding SimpleList:
    if ( ENTITYget_subtypes( newEnt ) == NULL ) {
	newEnt->search_id = TRUE;
	simple = new SimpleList( ENTITYget_name( newEnt ) );
	appendList( simple );
	return;
    }

    // If newEnt has subtypes, find or build the corresponding ComplexList:
#ifdef COMPLEX_INFO
    cout << "    Subtype is a supertype ...\n";
#endif
    if ( newEnt->search_id == TRUE ) {
	// We've processed child already, find its ComplexList in col:
#ifdef COMPLEX_INFO
	cout << "      was built already ... finding it\n";
#endif
	sublist = col->find( ENTITYget_name( newEnt ) );
	// Make a copy and append to this:
	newlist = copyList( sublist->head );
    } else {
	// If this subtype has never been visited, we build a ComplexList out
	// of it and add it to our ComplexCollect.  Even though it only exists
	// as a subtype of supertypes, we temporarily treat it as a List.  We
	// do this because it may be referenced again - by the current List
	// in another way, or by another supertype.  As a separate List and
	// member of Collect, we'll be able to find it easily.  After all the
	// entities are processed, we'll remove this.
#ifdef COMPLEX_INFO
	cout << "      never built before ... building it now\n";
#endif
	sublist = new ComplexList( newEnt, col );
	col->insert( sublist );
	// Since this is the first time we're creating this list, we don't need
	// to copy it and append to this, as above.  We'll use the same Lists
	// again and also point to them from this.
	appendList( sublist->head );
	newlist = sublist->head;
    }

    // If the sub-list is not abstract, one more task:
    if ( ! newEnt->u.entity->abstract ) {
	// Since the subtype is not abstract, it can be instantiated without
	// its subtypes.  Create an OrList OR'ing the supertype alone and its
	// entire List:
	olist = new OrList;
	simple = new SimpleList( (char *)sublist->supertype() );
	olist->appendList( simple );
	// We just added "newlist" to the end of this.  We now replace it with
	// our new or, and place it underneath the or.  This OR's the new
	// subtype alone with the subtype + its own subtypes - just what we
	// want for a non-abstract subtype.
	olist->appendList( newlist );
	numchildren--;
	// (Slightly ugly:  Since we just grabbed newlist from this to or, we
	// had the side effect of making this's numcount incorrect.  I could
	// have done this more elegantly, but was lazy.)
	appendList( olist );
    }
}
