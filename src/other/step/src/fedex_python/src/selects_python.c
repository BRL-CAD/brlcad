/*******************************************************************
** FedEx parser output module for generating C++  class definitions
** December  5, 1989
** release 2 17-Feb-1992
** release 3 March 1993
** release 4 December 1993
** K. C. Morris
**
** Development of FedEx was funded by the United States Government,
** and is not subject to copyright.

*******************************************************************
The conventions used in this binding follow the proposed specification
for the STEP Standard Data Access Interface as defined in document
N350 ( August 31, 1993 ) of ISO 10303 TC184/SC4/WG7.
*******************************************************************/

extern int multiple_inheritance;

/**************************************************************************
********    The functions in this file generate C++ code for representing
********    EXPRESS SELECT types.
**************************************************************************/
#include <stdlib.h>
#include "classes.h"

int isAggregateType( const Type t );
char * generate_attribute_name( Variable a, char * out );
char * FundamentalType( const Type t, int report_reftypes );
void ATTRsign_access_methods( Variable a, FILE * file );
char * generate_attribute_func_name( Variable a, char * out );
void ATTRprint_access_methods_get_head( const char * classnm, Variable a, FILE * file );
void ATTRprint_access_methods_put_head( const char * entnm, Variable a, FILE * file );

#define BASE_SELECT "SCLP23(Select)"

#define TYPEis_primitive(t) ( !( TYPEis_entity(t)  || \
                                 TYPEis_select (t) || \
                                 TYPEis_aggregate(t) ) )

#define TYPEis_numeric(t)   ( ((t)->u.type->body->type == real_)    || \
                  ((t)->u.type->body->type == integer_) || \
                  ((t)->u.type->body->type == number_) )
#define PRINT_BUG_REPORT  \
     fprintf( f, "   std::cerr << __FILE__ << \":\" << __LINE__ <<  \":  ERROR" \
              " in schema library:  \\n\" \n\t<< _POC_ << \"\\n\\n\";\n");

#define PRINT_SELECTBUG_WARNING(f) \
     fprintf( (f), "\n   severity( SEVERITY_WARNING );\n" ); \
     fprintf( (f), "   std::cerr << __FILE__ << \":\" << __LINE__ <<  \":  "); \
     fprintf( (f), "WARNING:  possible misuse of\"\n        << \" SELECT "); \
     fprintf( (f), "TYPE from schema library.\\n\";\n"); \
     fprintf( (f), "   Error( \"Mismatch in underlying type.\" );\n" );

#define print_error_msg() \
      fprintf( f, "\n  severity( SEVERITY_BUG );\n" ); \
      PRINT_BUG_REPORT  \
      fprintf( f, "  Error( \"This 'argument' is of the incorrect type\" );\n" );

#define TRUE    1
#define FALSE   0

static void initSelItems( const Type, FILE * );

const char *
SEL_ITEMget_enumtype( Type t ) {
    return StrToUpper( TYPEget_name( t ) );
}


/******************************************************************
 ** Procedure:  TYPEget_utype
 ** Parameters:  Type t
 ** Returns:  type used to represent the underlying type in a select class
 ** Description:
 ** Side Effects:
 ** Status:
 ******************************************************************/

const char *
TYPEget_utype( Type t )  {
}

/*******************
LISTmember

determines if the given entity is a member of the list.
RETURNS the member if it is a member; otherwise 0 is returned.
*******************/
Generic
LISTmember( const Linked_List list, Generic e ) {
    Link node;
    for( node = list->mark->next; node != list->mark; node = node->next )
        if( e == node -> data ) {
            return e;
        }
    return ( 0 );
}

/*******************
 compareOrigTypes

 Specialized function to catch if two enumerations, two selects, or two aggrs
 of either, are of the same type.  The issue is that e.g. select B may be a
 rename of sel A (i.e., TYPE B = A;).  Such renamed types are implemented by
 fedex_plus with typedefs, so that they are in fact the same type.  TYPEget_-
 ctype() is used for most type comparisons and does not consider renamed types
 equivalent.  This function is called in instances when they should be consi-
 dered equivalent.  One such case is the generation of duplicate lists.
 *******************/
static int
compareOrigTypes( Type a, Type b ) {
    Type t, u;

    if( ( TYPEis_select( a ) && TYPEis_select( b ) )
            || ( TYPEis_enumeration( a ) && TYPEis_enumeration( b ) ) ) {
        t = a;
        u = b;
    } else if( TYPEis_aggregate( a ) && TYPEis_aggregate( b ) ) {
        t = TYPEget_base_type( a );
        u = TYPEget_base_type( b );
        if( !( ( TYPEis_select( t ) && TYPEis_select( u ) )
                || ( TYPEis_enumeration( t ) && TYPEis_enumeration( u ) ) ) ) {
            return FALSE;
            /* Only go further with 1D aggregates of sels or enums.  Note that
            // for 2D aggrs and higher we do not continue.  These are all recog-
            // nized to be the same type ("GenericAggregate") by TYPEget_ctype(),
            // and do not have to be handled specially here. */
        }
    } else {
        return FALSE;
    }

    if( TYPEget_head( t ) ) {
        t = TYPEget_ancestor( t );
    }
    if( TYPEget_head( u ) ) {
        u = TYPEget_ancestor( u );
    }
    return ( !strcmp( TYPEget_name( t ), TYPEget_name( u ) ) );
}

/*******************
 utype_member

 determines if the given "link's" underlying type is a member of the list.
        RETURNS the underlying type if it is a member; otherwise 0 is returned.

 If "rename" is TRUE, we also consider check to match in certain cases where
 list already has an item that check is a renaming of (see header comments to
 compareOrigTypes() above).
 *******************/
const char *
utype_member( const Linked_List list, const Type check, int rename ) {
    static char r [BUFSIZ];

    LISTdo( list, t, Type )
    strncpy( r, TYPEget_utype( t ), BUFSIZ );
    if( strcmp( r, TYPEget_utype( check ) ) == 0 ) {
        return r;
    }
    if( rename && compareOrigTypes( check, t ) ) {
        return r;
    }
    LISTod;
    return 0;
}

/**
***  SELgetnew_dmlist (const Type type)
***  Returns a list of types which have unique underlying types
***  The returned list includes all the types which have a data members
***  in the select type.
***
***  The list that is returned needs to be freed by the caller.
***/


Linked_List
SELgetnew_dmlist( const Type type ) {
    Linked_List complete = SEL_TYPEget_items( type );
    Linked_List newlist = LISTcreate();

    LISTdo( complete, t, Type )

    /*     if t\'s underlying type is not already in newlist, */
    if( ! utype_member( newlist, t, 0 ) ) {
        LISTadd_last( newlist, t );
    }

    LISTod;

    return newlist;

}

const char *
SEL_ITEMget_dmtype( Type t, const Linked_List l ) {
    const char * r = utype_member( l, t, 0 );
    return StrToLower( r ? r : TYPEget_utype( t ) );

}


/*******************
duplicate_in_express_list

determines if the given "link's" underlying type is a multiple member
of the list.
    RETURNS 1 if true, else 0.
*******************/
int
duplicate_in_express_list( const Linked_List list, const Type check ) {
    if( TYPEis_entity( check ) ) {
        return FALSE;
    }
    /*  entities are never the same  */

    LISTdo( list, t, Type )
    if( t == check ) {
        ;    /*  don\'t compare check to itself  */
    } else {
        return TRUE;    /* other things in the list conflict  */
    }
    LISTod;
    return FALSE;
}

/*******************
unique_types ( const Linked_List list )

determines if any of the types in a select type resolve to the same
underlying Express type.
RETURNS 1 if true, else 0.
*******************/
int
unique_types( const Linked_List list ) {
    LISTdo( list, t, Type )
    if( duplicate_in_express_list( list, t ) ) {
        return FALSE;
    }
    LISTod;
    return TRUE;
}


/*******************
duplicate_utype_member

determines if the given "link's" C++ representation is used again in the list.
    RETURNS 1 if true, else 0.
*******************/
int
duplicate_utype_member( const Linked_List list, const Type check ) {
    char b [BUFSIZ];

    if( TYPEis_entity( check ) ) {
        return FALSE;
    }
    /*  entities are never the same  */

    LISTdo( list, t, Type )
    if( t == check ) {
        ;
    }
    /*  don\'t compare check to itself  */
    else {   /*  continue looking  */
        strncpy( b, TYPEget_utype( t ), BUFSIZ );
        if( ( !strcmp( b, TYPEget_utype( check ) ) )
                || ( compareOrigTypes( t, check ) ) )
            /*  if the underlying types are the same  */
        {
            return TRUE;
        }
        if( ! strcmp( b, "SCLP23(Integer)" ) &&
                ( ! strcmp( TYPEget_utype( check ), "SCLP23(Real)" ) ) )
            /*  integer\'s and real\'s are not unique  */
        {
            return TRUE;
        }
    }
    LISTod;
    return FALSE;
}

/*******************
any_duplicates_in_select

determines if any of the types in a select type resolve to the same
C++ representation for the underlying Express type.
RETURNS 1 if true, else 0.
*******************/
int
any_duplicates_in_select( const Linked_List list ) {
    LISTdo( list, t, Type )
    if( duplicate_utype_member( list, t ) ) {
        return TRUE;
    }
    LISTod;
    return FALSE;
}

/*******************
find_duplicate_list

finds an instance of each kind of duplicate type found in the given list.
This list is returned as dup_list.  If a duplicate exists, the function
returns TRUE, else FALSE.
list should be unbound before calling, and freed afterwards.
*******************/
int
find_duplicate_list( const Type type, Linked_List * duplicate_list ) {
    Linked_List temp; /** temporary list for comparison **/

    *duplicate_list = LISTcreate();
    if( any_duplicates_in_select( SEL_TYPEget_items( type ) ) ) {
        /**  if there is a dup somewhere  **/
        temp = LISTcreate();
        LISTdo( SEL_TYPEget_items( type ), u, Type )
        if( !utype_member( *duplicate_list, u, 1 ) ) {
            /**  if not already a duplicate  **/
            if( utype_member( temp, u, 1 ) ) {
                LISTadd_first( *duplicate_list, u );
            } else {
                LISTadd_first( temp, u );
            }
        }
        LISTod;
        LISTfree( temp );
        return TRUE;
    }
    return FALSE;
}

/*******************
non_unique_types_string ( const Type type )

returns a string containing the non-unique EXPRESS types deriveable
from a select.  the returned string is in the form (TYPE | TYPE |...)
*******************/
/* In the functions below, we use a vector of ints to count paths in the
   select-graph to base types.  The names in this enum correspond to the
   indices in the vector, i.e., tvec[treal] == tvec[1], and contains the
   number of paths to REAL in the graph
*/

enum __types {
    tint = 0,  /* INTEGER */
    treal,     /* REAL */
    tstring,   /* STRING */
    tbinary,   /* BINARY */
    tenum,     /* ENUMERATION, also LOGICAL, BOOLEAN */
    tselect,   /* SELECT */
    tentity,   /* ENTITY */
    taggr,     /* AGGREGATE, also ARRAY, BAG, SET, LIST */
    tnumber    /* NUMBER */
};

/* This function gets called recursively, to follow a select-graph to its
   leaves.  It passes around the vector described above, to track paths to
   the leaf nodes.
*/
void
non_unique_types_vector( const Type type, int * tvec ) {
    LISTdo( SEL_TYPEget_items( type ), t, Type )
    switch( TYPEget_body( t )->type ) {
        case integer_:
            tvec[tint]++;
            break;
        case real_:
            tvec[treal]++;
            break;
        case string_:
            tvec[tstring]++;
            break;
        case binary_:
            tvec[tbinary]++;
            break;
        case enumeration_:
        case logical_:
        case boolean_:
            tvec[tenum]++;
            break;
        case select_:
            /* SELECT, ergo recurse! */
            non_unique_types_vector( t, tvec );
            break;
        case entity_:
            tvec[tentity]++;
            break;
        case aggregate_:
        case array_:
        case list_:
        case bag_:
        case set_:
            tvec[taggr]++;
            break;
        case number_:
            tvec[tnumber]++;
            break;
        default:
            printf( "Error in %s, line %d: type %d not handled by switch statement.", __FILE__, __LINE__, TYPEget_body( t )->type );
            abort();
    }
    LISTod;
}

/* Uses non_unique_types_vector on the select to get a vector of base-type
   reference counts, then uses that to make a string of types, of the form
   (FOO_TYPE | BAR_TYPE | BAZ_TYPE), where FOO, BAR, and BAZ are EXPRESS
   types.  If all types are unique, the string (0) is generated.
*/
char *
non_unique_types_string( const Type type ) {
    int tvec[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    char * typestr;
    int first = 1;
    int i;

    non_unique_types_vector( type, tvec );

    /* build type string from vector */
    typestr = ( char * )malloc( BUFSIZ );
    typestr[0] = '\0';
    strcat( typestr, ( char * )"(" );
    for( i = 0; i <= tnumber; i++ ) {
        if( tvec[i] < 2 ) {
            continue;    /* skip, this one is unique */
        }

        if( !first ) {
            strcat( typestr, ( char * )" | " );
        } else {
            first = 0;
        }
        switch( i ) {
            case tint   :
                strcat( typestr, ( char * )"sdaiINTEGER" );
                break;
            case treal  :
                strcat( typestr, ( char * )"sdaiREAL" );
                break;
            case tstring:
                strcat( typestr, ( char * )"sdaiSTRING" );
                break;
            case tbinary:
                strcat( typestr, ( char * )"sdaiBINARY" );
                break;
            case tenum  :
                strcat( typestr, ( char * )"sdaiENUMERATION" );
                break;
            case tentity:
                strcat( typestr, ( char * )"sdaiINSTANCE" );
                break;
            case taggr  :
                strcat( typestr, ( char * )"sdaiAGGR" );
                break;
            case tnumber:
                strcat( typestr, ( char * )"sdaiNUMBER" );
                break;
        }
    }
    if( first ) {
        strcat( typestr, ( char * )"0" );
    }
    strcat( typestr, ( char * )")" );
    return typestr;
}



/******************************************************************
 ** Procedure:  ATTR_LISTmember
 ** Parameters:  Linked_List l, Variable check
 ** Returns:  the attribute if an attribute with the same name as "check"
 **  is on the list, 0 otherwise
 ** Description:  checks to see if an attribute is a member of the list
 ** Side Effects:
 ** Status:  26-Oct-1993 done
 ******************************************************************/

Variable
ATTR_LISTmember( Linked_List l, Variable check ) {
    char nm [BUFSIZ];
    char cur [BUFSIZ];

    generate_attribute_name( check, nm );
    LISTdo( l, a, Variable )
    generate_attribute_name( a, cur );
    if( ! strcmp( nm, cur ) ) {
        return check;
    }
    LISTod;
    return ( 0 );
}


/******************************************************************
 ** Procedure:  SEL_TYPEgetnew_attribute_list
 ** Parameters:  const Type type
 ** Returns:  Returns a list of all the attributes for a select type.
 **   The list is the union of all the attributes of the underlying types.
 ** Description:
 ** Side Effects:
***  The list that is returned needs to be freed by the caller.
 ** Status:
 ******************************************************************/

Linked_List
SEL_TYPEgetnew_attribute_list( const Type type ) {
    Linked_List complete = SEL_TYPEget_items( type );
    Linked_List newlist = LISTcreate();
    Linked_List attrs;
    Entity cur;

    LISTdo( complete, t, Type )
    if( TYPEis_entity( t ) ) {
        cur = ENT_TYPEget_entity( t );
        attrs = ENTITYget_all_attributes( cur );
        LISTdo( attrs, a, Variable )
        if( ! ATTR_LISTmember( newlist, a ) ) {
            LISTadd_first( newlist, a );
        }
        LISTod;
    }
    LISTod;
    return newlist;
}

/*******************
TYPEselect_inc_print_vars prints the class 'definition', that is, the objects
    and the constructor(s)/destructor for a select class.
********************/
void
TYPEselect_inc_print_vars( const Type type, FILE * f, Linked_List dups ) {
 
}

/*******************
TYPEselect_inc_print prints the class member function declarations of a select
class.
*******************/
void
TYPEselect_inc_print( const Type type, FILE * f ) {
   }


/*******************
TYPEselect_lib_print_part_one prints constructor(s)/destructor of a select
class.
*******************/
void
TYPEselect_lib_print_part_one( const Type type, FILE * f, Schema schema,
                               Linked_List dups, char * n ) {
}

static void
initSelItems( const Type type, FILE * f )
/*
 * Creates initialization functions for the select items of a select.  The
 * selects must have their typedescriptors set properly.  If a select is a
 * renaming of another select ("TYPE selB = selA") its td would default to
 * selA's, so it must be set specifically.
 */
{
}

Linked_List
ENTITYget_expanded_entities( Entity e, Linked_List l ) {
    Linked_List supers;
    int super_cnt = 0;
    Entity super;

    if( ! LISTmember( l, e ) ) {
        LISTadd_first( l, e );
    }

    if( multiple_inheritance ) {
        supers = ENTITYget_supertypes( e );
        LISTdo( supers, s, Entity )
        /* ignore the more than one supertype
           since multiple inheritance isn\'t implemented  */
        if( super_cnt == 0 ) {
            ENTITYget_expanded_entities( s, l );
        }
        ++ super_cnt;
        LISTod;
    } else {
        /* ignore the more than one supertype
        since multiple inheritance isn\'t implemented  */
        super = ENTITYget_superclass( e );
        ENTITYget_expanded_entities( super, l );
    }
    return l;
}

Linked_List
SELget_entity_itemlist( const Type type ) {
    Linked_List complete = SEL_TYPEget_items( type );
    Linked_List newlist = LISTcreate();
    Entity cur;

    LISTdo( complete, t, Type )
    if( TYPEis_entity( t ) ) {
        cur = ENT_TYPEget_entity( t );
        ENTITYget_expanded_entities( cur, newlist );
    }
    LISTod;
    return newlist;

}

static int
memberOfEntPrimary( Entity ent, Variable uattr )
/*
 * Specialized function used in function TYPEselect_lib_print_part_three
 * below.  Calls a function to check if an attribute of an entity belongs
 * to its primary path (is its own attr, that of its first super, that of
 * its first super's first super etc), and does necessary housekeeping.
 */
{
    Linked_List attrlist = LISTcreate();
    int result;

    ENTITYget_first_attribs( ent, attrlist );
    result = ( LISTmember( attrlist, uattr ) != 0 );
    LIST_destroy( attrlist );
    return result;
}

/*******************
TYPEselect_lib_print_part_three prints part 3) of the SDAI C++ binding for
a select class -- access functions for the data members of underlying entity
types.
*******************/
void
TYPEselect_lib_print_part_three( const Type type, FILE * f, Schema schema,
                                 char * classnm ) {
}

/*******************
TYPEselect_lib_print_part_four prints part 4 of the SDAI document of a select
class.
*******************/
void
TYPEselect_lib_print_part_four( const Type type, FILE * f, Schema schema,
                                Linked_List dups, char * n ) {
 }


/*******************
TYPEselect_init_print prints the types that belong to the select type
*******************/

void
TYPEselect_init_print( const Type type, FILE * f, Schema schema ) {
#define schema_name SCHEMAget_name(schema)
    LISTdo( SEL_TYPEget_items( type ), t, Type )

    fprintf( f, "\t%s -> Elements ().AddNode",
             TYPEtd_name( type ) );
    fprintf( f, " (%s);\n",
             TYPEtd_name( t ) );
    LISTod;
#undef schema_name
}

void
TYPEselect_lib_part21( const Type type, FILE * f, Schema schema ) {
}


void
TYPEselect_lib_StrToVal( const Type type, FILE * f, Schema schema ) {
}

void
TYPEselect_lib_virtual( const Type type, FILE * f, Schema schema ) {
    TYPEselect_lib_part21( type, f,  schema );
    TYPEselect_lib_StrToVal( type, f,  schema );
}

void
SELlib_print_protected( const Type type,  FILE * f, const Schema schema ) {
}

/*******************
TYPEselect_lib_print prints the member functions (definitions) of a select
class.
*******************/
void
TYPEselect_lib_print( const Type type, FILE * f, Schema schema ) {
    int nbr_select = 0;
    int num = 0;

    fprintf( f, "# SELECT TYPE %s_\n", TYPEget_name(type) );
    // writes the variable with strings
    LISTdo( SEL_TYPEget_items( type ), t, Type )
        if (is_python_keyword(TYPEget_name(t))) {
            fprintf(f,"if (not '%s_' in globals().keys()):\n",TYPEget_name(t));
            fprintf( f, "%s_ = '%s_'\n",TYPEget_name(t),TYPEget_name(t));
        }
        else {
            fprintf(f,"if (not '%s' in globals().keys()):\n",TYPEget_name(t));
            fprintf( f, "\t%s = '%s'\n",TYPEget_name(t),TYPEget_name(t));
        }
    LISTod;
    
    // create the SELECT
    if (is_python_keyword(TYPEget_name(type))) {    
        fprintf( f, "%s_ = SELECT(",TYPEget_name(type));
    }
    else {
        fprintf( f, "%s = SELECT(",TYPEget_name(type));
    }
    
    // first compute the number of types (necessary to insert commas)
    nbr_select = 0;
    LISTdo( SEL_TYPEget_items( type ), t, Type )
        nbr_select++;
    LISTod;
    // then write types
    num = 0;
    LISTdo( SEL_TYPEget_items( type ), t, Type )
        if (is_python_keyword(TYPEget_name(t))) {
            fprintf( f, "\n\t'%s_'",TYPEget_name(t));
        }
        else {
            fprintf( f, "\n\t'%s'",TYPEget_name(t));
        }
        if (num < nbr_select -1 ) fprintf(f,",");
        num++;
    LISTod;
    fprintf(f,")\n");
}

void
TYPEselect_print( Type t, FILES * files, Schema schema ) {
}
#undef BASE_SELECT


/**************************************************************************
********        END  of SELECT TYPE
**************************************************************************/
