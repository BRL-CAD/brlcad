/*
** Fed-x parser output module for generating C++  class definitions
** December  5, 1989
** release 2 17-Feb-1992
** release 3 March 1993
** release 4 December 1993
** K. C. Morris
**
** Development of Fed-x was funded by the United States Government,
** and is not subject to copyright.

*******************************************************************
The conventions used in this binding follow the proposed specification
for the STEP Standard Data Access Interface as defined in document
N350 ( August 31, 1993 ) of ISO 10303 TC184/SC4/WG7.
*******************************************************************/

/**************************************************************//**
 * \file classes.c
***  The functions in this file generate the C++ code for ENTITY **
***  classes, TYPEs, and TypeDescriptors.                       ***
 **                                                             **/


/* this is used to add new dictionary calls */
/* #define NEWDICT */

#include <sc_memmgr.h>
#include <stdlib.h>
#include <assert.h>
#include "classes.h"
#include <ordered_attrs.h>

#include <sc_trace_fprintf.h>


int isAggregateType( const Type t );
int isAggregate( Variable a );
Variable VARis_type_shifter( Variable a );

int multiple_inheritance = 1;
int print_logging = 0;
int old_accessors = 0;

static int attr_count;  /**< number each attr to avoid inter-entity clashes
                            several classes use attr_count for naming attr dictionary entry
                            variables.  All but the last function generating code for a particular
                            entity increment a copy of it for naming each attr in the entity.
                            Here are the functions:
                            ENTITYhead_print (Entity entity, FILE* file,Schema schema)
                            LIBdescribe_entity (Entity entity, FILE* file, Schema schema)
                            LIBcopy_constructor (Entity ent, FILE* file)
                            LIBstructor_print (Entity entity, FILE* file, Schema schema)
                            LIBstructor_print_w_args (Entity entity, FILE* file, Schema schema)
                            ENTITYincode_print (Entity entity, FILES* files,Schema schema)
                            DAS
                        */
static int type_count;  ///< number each temporary type for same reason as \sa attr_count

extern int any_duplicates_in_select( const Linked_List list );
extern int unique_types( const Linked_List list );
extern char * non_unique_types_string( const Type type );
static void printEnumCreateHdr( FILE *, const Type );
static void printEnumCreateBody( FILE *, const Type );
static void printEnumAggrCrHdr( FILE *, const Type );
static void printEnumAggrCrBody( FILE *, const Type );
int TYPEget_RefTypeVarNm( const Type t, char * buf, Schema schema );
void TypeBody_Description( TypeBody body, char * buf );

/**
 * Turn the string into a new string that will be printed the same as the
 * original string. That is, turn backslash into a quoted backslash and
 * turn \n into "\n" (i.e. 2 chars).
 *
 * Mostly replaced by format_for_std_stringout, below. This function is
 * still used in one place in ENTITYincode_print().
 */
char * format_for_stringout( char * orig_buf, char * return_buf ) {
    char * optr  = orig_buf;
    char * rptr  = return_buf;
    while( *optr ) {
        if( *optr == '\n' ) {
            *rptr = '\\';
            rptr++;
            *rptr = 'n';
        } else if( *optr == '\\' ) {
            *rptr = '\\';
            rptr++;
            *rptr = '\\';
        } else {
            *rptr = *optr;
        }
        rptr++;
        optr++;
    }
    *rptr = '\0';
    return return_buf;
}

/**
 * Like format_for_stringout above, but splits the static string up
 * into numerous small ones that are appended to a std::string named
 * 'str'. It is assumed that this string already exists and is empty.
 *
 * This version takes a file pointer and eliminates use of the temp buffer.
 */
void format_for_std_stringout( FILE * f, const char * orig_buf ) {
    const char * optr  = orig_buf;
    char * s_end = "\\n\" );\n";
    char * s_begin = "    str.append( \"";
    fprintf( f, "%s", s_begin );
    while( *optr ) {
        if( *optr == '\n' ) {
            if( * ( optr + 1 ) == '\n' ) { // skip blank lines
                optr++;
                continue;
            }
            fprintf( f, "%s", s_end );
            fprintf( f, "%s", s_begin );
        } else if( *optr == '\\' ) {
            fprintf( f, "\\\\" );
        } else {
            fprintf( f, "%c", *optr );
        }
        optr++;
    }
    fprintf( f,  s_end );
    sc_free( orig_buf );
}

void USEREFout( Schema schema, Dictionary refdict, Linked_List reflist, char * type, FILE * file ) {
    Dictionary dict;
    DictionaryEntry de;
    struct Rename * r;
    Linked_List list;
    char td_name[BUFSIZ];
    char sch_name[BUFSIZ];

    strncpy( sch_name, PrettyTmpName( SCHEMAget_name( schema ) ), BUFSIZ );

    LISTdo( reflist, s, Schema ) {
        fprintf( file, "        // %s FROM %s; (all objects)\n", type, s->symbol.name );
        fprintf( file, "        is = new Interface_spec(\"%s\",\"%s\");\n", sch_name, PrettyTmpName( s->symbol.name ) );
        fprintf( file, "        is->all_objects_(1);\n" );
        if( !strcmp( type, "USE" ) ) {
            fprintf( file, "        %s::schema->use_interface_list_()->Append(is);\n", SCHEMAget_name( schema ) );
        } else {
            fprintf( file, "        %s::schema->ref_interface_list_()->Append(is);\n", SCHEMAget_name( schema ) );
        }
    }
    LISTod

    if( !refdict ) {
        return;
    }
    dict = DICTcreate( 10 );

    /* sort each list by schema */

    /* step 1: for each entry, store it in a schema-specific list */
    DICTdo_init( refdict, &de );
    while( 0 != ( r = ( struct Rename * )DICTdo( &de ) ) ) {
        Linked_List list;

        list = ( Linked_List )DICTlookup( dict, r->schema->symbol.name );
        if( !list ) {
            list = LISTcreate();
            DICTdefine( dict, r->schema->symbol.name, ( Generic ) list,
                        ( Symbol * )0, OBJ_UNKNOWN );
        }
        LISTadd( list, ( Generic ) r );
    }

    /* step 2: for each list, print out the renames */
    DICTdo_init( dict, &de );
    while( 0 != ( list = ( Linked_List )DICTdo( &de ) ) ) {
        bool first_time = true;
        LISTdo( list, r, struct Rename * )

        /* note: SCHEMAget_name(r->schema) equals r->schema->symbol.name) */
        if( first_time ) {
            fprintf( file, "        // %s FROM %s (selected objects)\n", type, r->schema->symbol.name );
            fprintf( file, "        is = new Interface_spec(\"%s\",\"%s\");\n", sch_name, PrettyTmpName( r->schema->symbol.name ) );
            if( !strcmp( type, "USE" ) ) {
                fprintf( file, "        %s::schema->use_interface_list_()->Append(is);\n", SCHEMAget_name( schema ) );
            } else {
                fprintf( file, "        %s::schema->ref_interface_list_()->Append(is);\n", SCHEMAget_name( schema ) );
            }
        }

        if( first_time ) {
            first_time = false;
        }
        if( r->type == OBJ_TYPE ) {
            sprintf( td_name, "%s", TYPEtd_name( ( Type )r->object ) );
        } else if( r->type == OBJ_FUNCTION ) {
            sprintf( td_name, "/* Function not implemented */ 0" );
        } else if( r->type == OBJ_PROCEDURE ) {
            sprintf( td_name, "/* Procedure not implemented */ 0" );
        } else if( r->type == OBJ_RULE ) {
            sprintf( td_name, "/* Rule not implemented */ 0" );
        } else if( r->type == OBJ_ENTITY ) {
            sprintf( td_name, "%s%s%s",
                     SCOPEget_name( ( ( Entity )r->object )->superscope ),
                     ENT_PREFIX, ENTITYget_name( ( Entity )r->object ) );
        } else {
            sprintf( td_name, "/* %c from OBJ_? in expbasic.h not implemented */ 0", r->type );
        }
        if( r->old != r->nnew ) {
            fprintf( file, "        // object %s AS %s\n", r->old->name,
                     r->nnew->name );
            if( !strcmp( type, "USE" ) ) {
                fprintf( file, "        ui = new Used_item(\"%s\", %s, \"%s\", \"%s\");\n", r->schema->symbol.name, td_name, r->old->name, r->nnew->name );
                fprintf( file, "        is->explicit_items_()->Append(ui);\n" );
                fprintf( file, "        %s::schema->interface_().explicit_items_()->Append(ui);\n", SCHEMAget_name( schema ) );
            } else {
                fprintf( file, "        ri = new Referenced_item(\"%s\", %s, \"%s\", \"%s\");\n", r->schema->symbol.name, td_name, r->old->name, r->nnew->name );
                fprintf( file, "        is->explicit_items_()->Append(ri);\n" );
                fprintf( file, "        %s::schema->interface_().explicit_items_()->Append(ri);\n", SCHEMAget_name( schema ) );
            }
        } else {
            fprintf( file, "        // object %s\n", r->old->name );
            if( !strcmp( type, "USE" ) ) {
                fprintf( file, "        ui = new Used_item(\"%s\", %s, \"\", \"%s\");\n", r->schema->symbol.name, td_name, r->nnew->name );
                fprintf( file, "        is->explicit_items_()->Append(ui);\n" );
                fprintf( file, "        %s::schema->interface_().explicit_items_()->Append(ui);\n", SCHEMAget_name( schema ) );
            } else {
                fprintf( file, "        ri = new Referenced_item(\"%s\", %s, \"\", \"%s\");\n", r->schema->symbol.name, td_name, r->nnew->name );
                fprintf( file, "        is->explicit_items_()->Append(ri);\n" );
                fprintf( file, "        %s::schema->interface_().explicit_items_()->Append(ri);\n", SCHEMAget_name( schema ) );
            }
        }
        LISTod
    }
    HASHdestroy( dict );
}

const char * IdlEntityTypeName( Type t ) {
    static char name [BUFSIZ];
    strcpy( name, TYPE_PREFIX );
    if( TYPEget_name( t ) ) {
        strcpy( name, FirstToUpper( TYPEget_name( t ) ) );
    } else {
        return TYPEget_ctype( t );
    }
    return name;
}

const char * GetAggrElemType( const Type type ) {
    Class_Of_Type class;
    Type bt;
    static char retval [BUFSIZ];

    if( isAggregateType( type ) ) {
        bt = TYPEget_nonaggregate_base_type( type );
        if( isAggregateType( bt ) ) {
            strcpy( retval, "ERROR_aggr_of_aggr" );
        }

        class = TYPEget_type( bt );

        /*      case TYPE_INTEGER:  */
        if( class == integer_ ) {
            strcpy( retval, "long" );
        }

        /*      case TYPE_REAL:
            case TYPE_NUMBER:   */
        if( ( class == number_ ) || ( class == real_ ) ) {
            strcpy( retval, "double" );
        }

        /*      case TYPE_ENTITY:   */
        if( class == entity_ ) {
            strcpy( retval, IdlEntityTypeName( bt ) );
        }

        /*      case TYPE_ENUM:     */
        /*  case TYPE_SELECT:   */
        if( ( class == enumeration_ )
                || ( class == select_ ) )  {
            strcpy( retval, TYPEget_ctype( bt ) );
        }

        /*  case TYPE_LOGICAL:  */
        if( class == logical_ ) {
            strcpy( retval, "Logical" );
        }

        /*  case TYPE_BOOLEAN:  */
        if( class == boolean_ ) {
            strcpy( retval, "Bool" );
        }

        /*  case TYPE_STRING:   */
        if( class == string_ ) {
            strcpy( retval, "string" );
        }

        /*  case TYPE_BINARY:   */
        if( class == binary_ ) {
            strcpy( retval, "binary" );
        }
    }
    return retval;
}

const char * TYPEget_idl_type( const Type t ) {
    Class_Of_Type class;
    static char retval [BUFSIZ];

    /*  aggregates are based on their base type
    case TYPE_ARRAY:
    case TYPE_BAG:
    case TYPE_LIST:
    case TYPE_SET:
    */

    if( isAggregateType( t ) ) {
        strcpy( retval, GetAggrElemType( t ) );

        /*  case TYPE_ARRAY:    */
        if( TYPEget_type( t ) == array_ ) {
            strcat( retval, "__array" );
        }
        /*  case TYPE_LIST: */
        if( TYPEget_type( t ) == list_ ) {
            strcat( retval, "__list" );
        }
        /*  case TYPE_SET:  */
        if( TYPEget_type( t ) == set_ ) {
            strcat( retval, "__set" );
        }
        /*  case TYPE_BAG:  */
        if( TYPEget_type( t ) == bag_ ) {
            strcat( retval, "__bag" );
        }
        return retval;
    }

    /*  the rest is for things that are not aggregates  */

    class = TYPEget_type( t );

    /*    case TYPE_LOGICAL:    */
    if( class == logical_ ) {
        return ( "Logical" );
    }

    /*    case TYPE_BOOLEAN:    */
    if( class == boolean_ ) {
        return ( "Boolean" );
    }

    /*      case TYPE_INTEGER:  */
    if( class == integer_ ) {
        return ( "SDAI_Integer" );
    }

    /*      case TYPE_REAL:
        case TYPE_NUMBER:   */
    if( ( class == number_ ) || ( class == real_ ) ) {
        return ( "SDAI_Real" );
    }

    /*      case TYPE_STRING:   */
    if( class == string_ ) {
        return ( "char *" );
    }

    /*      case TYPE_BINARY:   */
    if( class == binary_ ) {
        return ( AccessType( t ) );
    }

    /*      case TYPE_ENTITY:   */
    if( class == entity_ ) {
        /* better do this because the return type might go away */
        strcpy( retval, IdlEntityTypeName( t ) );
        strcat( retval, "_ptr" );
        return retval;
    }
    /*    case TYPE_ENUM:   */
    /*    case TYPE_SELECT: */
    if( class == enumeration_ ) {
        strncpy( retval, EnumName( TYPEget_name( t ) ), BUFSIZ - 2 );

        strcat( retval, " /*" );
        strcat( retval, IdlEntityTypeName( t ) );
        strcat( retval, "*/ " );
        return retval;
    }
    if( class == select_ )  {
        return ( IdlEntityTypeName( t ) );
    }

    /*  default returns undefined   */
    return ( "SCLundefined" );
}

int Handle_FedPlus_Args( int i, char * arg ) {
    if( ( ( char )i == 's' ) || ( ( char )i == 'S' ) ) {
        multiple_inheritance = 0;
    }
    if( ( ( char )i == 'a' ) || ( ( char )i == 'A' ) ) {
        old_accessors = 1;
    }
    if( ( ( char )i == 'l' ) || ( ( char )i == 'L' ) ) {
        print_logging = 1;
    }
    return 0;
}

/**************************************************************//**
 ** Procedure:  generate_attribute_name
 ** Parameters:  Variable a, an Express attribute; char *out, the C++ name
 ** Description:  converts an Express name into the corresponding C++ name
 **       see relation to generate_dict_attr_name() DAS
 ** Side Effects:
 ** Status:  complete 8/5/93
 ******************************************************************/
char * generate_attribute_name( Variable a, char * out ) {
    char * temp, *p, *q;
    int i;

    temp = EXPRto_string( VARget_name( a ) );
    p = StrToLower( temp );
    if( ! strncmp( p, "self\\", 5 ) ) {
        p += 5;
    }
    /*  copy p to out  */
    /* DAR - fixed so that '\n's removed */
    for( i = 0, q = out; *p != '\0' && i < BUFSIZ; p++ ) {
        /* copy p to out, 1 char at time.  Skip \n's and spaces, convert
         *  '.' to '_', and convert to lowercase. */
        if( ( *p != '\n' ) && ( *p != ' ' ) ) {
            if( *p == '.' ) {
                *q = '_';
            } else {
                *q = *p;
            }
            i++;
            q++;
        }
    }
    *q = '\0';
    sc_free( temp );
    return out;
}

char * generate_attribute_func_name( Variable a, char * out ) {
    generate_attribute_name( a, out );
    strncpy( out, StrToLower( out ), BUFSIZ );
    if( old_accessors ) {
        out[0] = toupper( out[0] );
    } else {
        out[strlen( out )] = '_';
    }
    return out;
}

/**************************************************************//**
 ** \fn  generate_dict_attr_name
 ** \param a, an Express attribute
 ** \param out, the C++ name
 ** Description:  converts an Express name into the corresponding SCL
 **       dictionary name.  The difference between this and the
 **           generate_attribute_name() function is that for derived
 **       attributes the name will have the form <parent>.<attr_name>
 **       where <parent> is the name of the parent containing the
 **       attribute being derived and <attr_name> is the name of the
 **       derived attribute. Both <parent> and <attr_name> may
 **       contain underscores but <parent> and <attr_name> will be
 **       separated by a period.  generate_attribute_name() generates
 **       the same name except <parent> and <attr_name> will be
 **       separated by an underscore since it is illegal to have a
 **       period in a variable name.  This function is used for the
 **       dictionary name (a string) and generate_attribute_name()
 **       will be used for variable and access function names.
 ** Side Effects:
 ** Status:  complete 8/5/93
 ******************************************************************/
char * generate_dict_attr_name( Variable a, char * out ) {
    char * temp, *p, *q;
    int j;

    temp = EXPRto_string( VARget_name( a ) );
    p = temp;
    if( ! strncmp( StrToLower( p ), "self\\", 5 ) ) {
        p = p + 5;
    }
    /*  copy p to out  */
    strncpy( out, StrToLower( p ), BUFSIZ );
    /* DAR - fixed so that '\n's removed */
    for( j = 0, q = out; *p != '\0' && j < BUFSIZ; p++ ) {
        /* copy p to out, 1 char at time.  Skip \n's, and convert to lc. */
        if( *p != '\n' ) {
            *q = tolower( *p );
            j++;
            q++;
        }
    }
    *q = '\0';

    sc_free( temp );
    return out;
}

/**************************************************************//**
 ** Procedure:  TYPEget_express_type (const Type t)
 ** Parameters:  const Type t --  type for attribute
 ** Returns:  a string which is the type as it would appear in Express
 ** Description:  supplies the type for error messages
                  and to register the entity
          - calls itself recursively to create a description of
          aggregate types
 ** Side Effects:
 ** Status:  new 1/24/91
 ******************************************************************/
char * TYPEget_express_type( const Type t ) {
    Class_Of_Type class;
    Type bt;
    char retval [BUFSIZ];
    char * n, * permval, * aggr_type;


    /*  1.  "DEFINED" types */
    /*    case TYPE_ENUM:   */
    /*    case TYPE_ENTITY: */
    /*  case TYPE_SELECT:       */

    n = TYPEget_name( t );
    if( n ) {
        PrettyTmpName( n );
    }

    /*   2.   "BASE" types  */
    class = TYPEget_type( t );

    /*    case TYPE_LOGICAL:    */
    if( ( class == boolean_ ) || ( class == logical_ ) ) {
        return ( "Logical" );
    }

    /*      case TYPE_INTEGER:  */
    if( class == integer_ ) {
        return ( "Integer " );
    }

    /*      case TYPE_REAL:
        case TYPE_NUMBER:   */
    if( ( class == number_ ) || ( class == real_ ) ) {
        return ( "Real " );
    }

    /*      case TYPE_STRING:   */
    if( class == string_ ) {
        return ( "String " )      ;
    }

    /*      case TYPE_BINARY:   */
    if( class == binary_ ) {
        return ( "Binary " )      ;
    }

    /*  AGGREGATES
    case TYPE_ARRAY:
    case TYPE_BAG:
    case TYPE_LIST:
    case TYPE_SET:
    */
    if( isAggregateType( t ) ) {
        bt = TYPEget_nonaggregate_base_type( t );
        class = TYPEget_type( bt );

        /*  case TYPE_ARRAY:    */
        if( TYPEget_type( t ) == array_ ) {
            aggr_type = "Array";
        }
        /*  case TYPE_LIST: */
        if( TYPEget_type( t ) == list_ ) {
            aggr_type = "List";
        }
        /*  case TYPE_SET:  */
        if( TYPEget_type( t ) == set_ ) {
            aggr_type = "Set";
        }
        /*  case TYPE_BAG:  */
        if( TYPEget_type( t ) == bag_ ) {
            aggr_type = "Bag";
        }

        sprintf( retval, "%s of %s",
                 aggr_type, TYPEget_express_type( bt ) );

        /*  this will declare extra memory when aggregate is > 1D  */

        permval = ( char * )sc_malloc( strlen( retval ) * sizeof( char ) + 1 );
        strcpy( permval, retval );
        return permval;

    }

    /*  default returns undefined   */

    printf( "WARNING2:  type  %s  is undefined\n", TYPEget_name( t ) );
    return ( "SCLundefined" );

}

/**************************************************************//**
 ** Procedure:  ATTRsign_access_method
 ** Parameters:  const Variable a --  attribute to print
                                      access method signature for
 ** FILE* file  --  file being written to
 ** Returns:  nothing
 ** Description:  prints the signature for an access method
 **               based on the attribute type
 **       DAS i.e. prints the header for the attr. access functions
 **       (get and put attr value) in the entity class def in .h file
 ** Side Effects:
 ** Status:  complete 17-Feb-1992
 ******************************************************************/
void ATTRsign_access_methods( Variable a, FILE * file ) {

    Type t = VARget_type( a );
    char ctype [BUFSIZ];
    char attrnm [BUFSIZ];

    generate_attribute_func_name( a, attrnm );

    strncpy( ctype, AccessType( t ), BUFSIZ );
    ctype[BUFSIZ-1] = '\0';
    fprintf( file, "        %s %s() const;\n", ctype, attrnm );
    fprintf( file, "        void %s (const %s x);\n\n", attrnm, ctype );
    return;
}

/**************************************************************//**
 ** Procedure:  ATTRprint_access_methods_get_head
 ** Parameters:  const Variable a --  attribute to find the type for
 ** FILE* file  --  file being written
 ** Type t - type of the attribute
 ** Class_Of_Type class -- type name of the class
 ** const char *attrnm -- name of the attribute
 ** char *ctype -- (possibly returned) name of the attribute c++ type
 ** Returns:  name to be used for the type of the c++ access functions
 ** Description:  prints the access method get head based on the attribute type
 **     DAS which being translated is it prints the function header
 **     for the get attr value access function defined for an
 **     entity class. This is the .cc file version.
 ** Side Effects:
 ** Status:  complete 7/15/93       by DDH
 ******************************************************************/
void ATTRprint_access_methods_get_head( const char * classnm, Variable a,
                                        FILE * file ) {
    Type t = VARget_type( a );
    char ctype [BUFSIZ];   /*  return type of the get function  */
    char funcnm [BUFSIZ];  /*  name of member function  */

    generate_attribute_func_name( a, funcnm );

    /* ///////////////////////////////////////////////// */

    strncpy( ctype, AccessType( t ), BUFSIZ );
    ctype[BUFSIZ-1] = '\0';
    fprintf( file, "\n%s %s::%s() const ", ctype, classnm, funcnm );
    return;
}

/**************************************************************//**
 ** Procedure:  ATTRprint_access_methods_put_head
 ** Parameters:  const Variable a --  attribute to find the type for
 ** FILE* file  --  file being written to
 ** Type t - type of the attribute
 ** Class_Of_Type class -- type name of the class
 ** const char *attrnm -- name of the attribute
 ** char *ctype -- name of the attribute c++ type
 ** Returns:  name to be used for the type of the c++ access functions
 ** Description:  prints the access method put head based on the attribute type
 **     DAS which being translated is it prints the function header
 **     for the put attr value access function defined for an
 **     entity class. This is the .cc file version.
 ** Side Effects:
 ** Status:  complete 7/15/93       by DDH
 ******************************************************************/
void ATTRprint_access_methods_put_head( CONST char * entnm, Variable a, FILE * file ) {

    Type t = VARget_type( a );
    char ctype [BUFSIZ];
    char funcnm [BUFSIZ];

    generate_attribute_func_name( a, funcnm );

    strncpy( ctype, AccessType( t ), BUFSIZ );
    ctype[BUFSIZ-1] = '\0';
    fprintf( file, "\nvoid %s::%s( const %s x ) ", entnm, funcnm, ctype );

    return;
}

void AGGRprint_access_methods( CONST char * entnm, Variable a, FILE * file, Type t,
                               char * ctype, char * attrnm ) {
    ATTRprint_access_methods_get_head( entnm, a, file );
    fprintf( file, "{\n" );
    fprintf( file, "    return ( %s ) %s_%s;\n}\n", ctype, ( ( a->type->u.type->body->base ) ? "" : "& " ), attrnm );
    ATTRprint_access_methods_put_head( entnm, a, file );
    fprintf( file, "{\n    _%s%sShallowCopy( * x );\n}\n", attrnm, ( ( a->type->u.type->body->base ) ? "->" : "." ) );
    return;
}

/**************************************************************//**
 ** Procedure:  ATTRprint_access_method
 ** Parameters:  const Variable a --  attribute to find the type for
 ** FILE* file  --  file being written to
 ** Returns:  name to be used for the type of the c++ access functions
 ** Description:  prints the access method based on the attribute type
 **       i.e. get and put value access functions defined in a class
 **       generated for an entity.
 ** Side Effects:
 ** Status:  complete 1/15/91
 **     updated 17-Feb-1992 to print to library file instead of header
 ** updated 15-July-1993 to call the get/put head functions by DDH
 ******************************************************************/
void ATTRprint_access_methods( CONST char * entnm, Variable a, FILE * file ) {
    Type t = VARget_type( a );
    Class_Of_Type class;
    char ctype [BUFSIZ];  /*  type of data member  */
    char attrnm [BUFSIZ];
    char membernm[BUFSIZ];
    char funcnm [BUFSIZ];  /*  name of member function  */

    char nm [BUFSIZ];
    /* I believe nm has the name of the underlying type without Sdai in
       front of it */
    if( TYPEget_name( t ) ) {
        strncpy( nm, FirstToUpper( TYPEget_name( t ) ), BUFSIZ - 1 );
    }

    generate_attribute_func_name( a, funcnm );
    generate_attribute_name( a, attrnm );
    strcpy( membernm, attrnm );
    membernm[0] = toupper( membernm[0] );
    class = TYPEget_type( t );
    strncpy( ctype, AccessType( t ), BUFSIZ );

    if( isAggregate( a ) ) {
        AGGRprint_access_methods( entnm, a, file, t, ctype, attrnm );
        return;
    }
    ATTRprint_access_methods_get_head( entnm, a, file );

    /*      case TYPE_ENTITY:   */
    if( class == entity_ )  {

        fprintf( file, "{\n" );
        if( print_logging ) {
            fprintf( file, "#ifdef SC_LOGGING\n" );
            fprintf( file, "    if(*logStream)\n    {\n" );
            fprintf( file, "        logStream->open(SCLLOGFILE,ios::app);\n" );
            fprintf( file, "        if(! (_%s == S_ENTITY_NULL) )\n        {\n", attrnm );
            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "            *logStream << \"reference to Sdai%s entity #\" << _%s->STEPfile_id << std::endl;\n",
                     nm, attrnm );
            fprintf( file, "        }\n        else\n        {\n" );
            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "            *logStream << \"null entity\" << std::endl;\n        }\n" );
            fprintf( file, "        logStream->close();\n" );
            fprintf( file, "    }\n" );
            fprintf( file, "#endif\n" );
        }
        fprintf( file, "    return (%s) _%s;\n}\n", ctype, attrnm );

        ATTRprint_access_methods_put_head( entnm, a, file );
        fprintf( file, "{\n" );
        if( print_logging ) {
            fprintf( file, "\n" );
            fprintf( file, "#ifdef SC_LOGGING\n" );
            fprintf( file, "    if(*logStream)\n    {\n" );
            fprintf( file, "        logStream->open(SCLLOGFILE,ios::app);\n" );

            fprintf( file, "        if(! (x == S_ENTITY_NULL) )\n        {\n" );

            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() assigned: \";\n",
                     entnm, funcnm );

            fprintf( file,
                     "            *logStream << \"reference to Sdai%s entity #\" << x->STEPfile_id << std::endl;\n",
                     nm );

            fprintf( file, "        }\n        else\n        {\n" );
            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() assigned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "            *logStream << \"null entity\" << std::endl;\n        }\n" );
            fprintf( file, "        logStream->close();\n" );
            fprintf( file, "    }\n" );
            fprintf( file, "#endif\n" );
        }
        fprintf( file, "    _%s = x;\n}\n", attrnm );

        return;
    }
    /*    case TYPE_LOGICAL:    */
    if( ( class == boolean_ ) || ( class == logical_ ) )  {

        fprintf( file, "{\n" );
        if( print_logging ) {
            fprintf( file, "#ifdef SC_LOGGING\n" );
            fprintf( file, "    if(*logStream)\n    {\n" );
            fprintf( file, "        logStream->open(SCLLOGFILE,ios::app);\n" );
            fprintf( file, "        if(!_%s.is_null())\n        {\n", attrnm );
            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "            *logStream << _%s.element_at(_%s.asInt()) << std::endl;\n",
                     attrnm, attrnm );
            fprintf( file, "        }\n        else\n        {\n" );
            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "            *logStream << \"unset\" << std::endl;\n        }\n" );
            fprintf( file, "            logStream->close();\n" );
            fprintf( file, "    }\n" );
            fprintf( file, "#endif\n" );
        }
        fprintf( file, "    return (%s) _%s;\n}\n", ctype, attrnm );

        ATTRprint_access_methods_put_head( entnm, a, file );
        fprintf( file, "{\n" );
        if( print_logging ) {
            fprintf( file, "#ifdef SC_LOGGING\n" );
            fprintf( file, "    if(*logStream)\n    {\n" );
            fprintf( file, "        *logStream << time(NULL) << \" SDAI %s::%s() assigned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "        *logStream << _%s.element_at(x) << std::endl;\n", attrnm );
            fprintf( file, "    }\n" );
            fprintf( file, "#endif\n" );
        }
        fprintf( file, "    _%s.put (x);\n}\n", attrnm );
        return;
    }
    /*    case TYPE_ENUM:   */
    if( class == enumeration_ )  {
        fprintf( file, "{\n" );
        if( print_logging ) {
            fprintf( file, "#ifdef SC_LOGGING\n" );
            fprintf( file, "    if(*logStream)\n    {\n" );
            fprintf( file, "        if(!_%s.is_null())\n        {\n", attrnm );
            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "            *logStream << _%s.element_at(_%s.asInt()) << std::endl;\n",
                     attrnm, attrnm );
            fprintf( file, "        }\n        else\n        {\n" );
            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "            *logStream << \"unset\" << std::endl;\n        }\n    }\n" );
            fprintf( file, "#endif\n" );
        }
        fprintf( file, "    return (%s) _%s;\n}\n",
                 EnumName( TYPEget_name( t ) ), attrnm );

        ATTRprint_access_methods_put_head( entnm, a, file );
        fprintf( file, "{\n" );
        if( print_logging ) {
            fprintf( file, "#ifdef SC_LOGGING\n" );
            fprintf( file, "    if(*logStream)\n    {\n" );
            fprintf( file, "        *logStream << time(NULL) << \" SDAI %s::%s() assigned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "        *logStream << _%s.element_at(x) << std::endl;\n", attrnm );
            fprintf( file, "    }\n" );
            fprintf( file, "#endif\n" );
        }
        fprintf( file, "    _%s.put (x);\n}\n", attrnm );
        return;
    }
    /*    case TYPE_SELECT: */
    if( class == select_ )  {
        fprintf( file, "        { return (const %s) &_%s; }\n",  ctype, attrnm );
        ATTRprint_access_methods_put_head( entnm, a, file );
        fprintf( file, "        { _%s = x; }\n", attrnm );
        return;
    }
    /*    case TYPE_AGGRETATES: */
    /* handled in AGGRprint_access_methods(entnm, a, file, t, ctype, attrnm) */


    /*  case STRING:*/
    /*      case TYPE_BINARY:   */
    if( ( class == string_ ) || ( class == binary_ ) )  {
        fprintf( file, "{\n" );
        if( print_logging ) {
            fprintf( file, "#ifdef SC_LOGGING\n" );
            fprintf( file, "    if(*logStream)\n    {\n" );
            fprintf( file, "        if(!_%s.is_null())\n        {\n", attrnm );
            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "            *logStream << _%s << std::endl;\n", attrnm );
            fprintf( file, "        }\n        else\n        {\n" );
            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "            *logStream << \"unset\" << std::endl;\n        }\n    }\n" );
            fprintf( file, "#endif\n" );

        }
        fprintf( file, "    return (const %s) _%s;\n}\n", ctype, attrnm );
        ATTRprint_access_methods_put_head( entnm, a, file );
        fprintf( file, "{\n" );
        if( print_logging ) {
            fprintf( file, "#ifdef SC_LOGGING\n" );
            fprintf( file, "    if(*logStream)\n    {\n" );
            fprintf( file, "        if(!x)\n        {\n" );
            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "            *logStream << x << std::endl;\n" );
            fprintf( file, "        }\n        else\n        {\n" );
            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "            *logStream << \"unset\" << std::endl;\n        }\n    }\n" );
            fprintf( file, "#endif\n" );
        }
        fprintf( file, "    _%s = x;\n}\n", attrnm );
        return;
    }
    /*      case TYPE_INTEGER:  */
    if( class == integer_ ) {
        fprintf( file, "{\n" );
        if( print_logging ) {
            fprintf( file, "#ifdef SC_LOGGING\n" );
            fprintf( file, "    if(*logStream)\n    {\n" );
            fprintf( file, "        if(!(_%s == S_INT_NULL) )\n        {\n", attrnm );
            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "            *logStream << _%s << std::endl;\n", attrnm );
            fprintf( file, "        }\n        else\n        {\n" );
            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "            *logStream << \"unset\" << std::endl;\n        }\n    }\n" );
            fprintf( file, "#endif\n" );
        }
        /*  default:  INTEGER   */
        /*  is the same type as the data member  */
        fprintf( file, "    return (const %s) _%s;\n}\n", ctype, attrnm );
        ATTRprint_access_methods_put_head( entnm, a, file );
        fprintf( file, "{\n" );
        if( print_logging ) {
            fprintf( file, "#ifdef SC_LOGGING\n" );
            fprintf( file, "    if(*logStream)\n    {\n" );
            fprintf( file, "        if(!(x == S_INT_NULL) )\n        {\n" );
            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "            *logStream << x << std::endl;\n" );
            fprintf( file, "        }\n        else\n        {\n" );
            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "            *logStream << \"unset\" << std::endl;\n        }\n    }\n" );
            fprintf( file, "#endif\n" );
            /*  default:  INTEGER   */
            /*  is the same type as the data member  */
        }
        fprintf( file, "    _%s = x;\n}\n", attrnm );
    }

    /*      case TYPE_REAL:
        case TYPE_NUMBER:   */
    if( ( class == number_ ) || ( class == real_ ) ) {
        fprintf( file, "{\n" );
        if( print_logging ) {
            fprintf( file, "#ifdef SC_LOGGING\n" );
            fprintf( file, "    if(*logStream)\n    {\n" );
            fprintf( file, "        if(!(_%s == S_REAL_NULL) )\n        {\n", attrnm );
            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "            *logStream << _%s << std::endl;\n", attrnm );
            fprintf( file, "        }\n        else\n        {\n" );
            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "            *logStream << \"unset\" << std::endl;\n        }\n    }\n" );
            fprintf( file, "#endif\n" );
        }
        fprintf( file, "    return (const %s) _%s;\n}\n", ctype, attrnm );
        ATTRprint_access_methods_put_head( entnm, a, file );
        fprintf( file, "{\n" );
        if( print_logging ) {
            fprintf( file, "#ifdef SC_LOGGING\n" );
            fprintf( file, "    if(*logStream)\n    {\n" );
            fprintf( file, "        if(!(_%s == S_REAL_NULL) )\n        {\n", attrnm );
            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "            *logStream << _%s << std::endl;\n", attrnm );
            fprintf( file, "        }\n        else\n        {\n" );
            fprintf( file, "            *logStream << time(NULL) << \" SDAI %s::%s() returned: \";\n",
                     entnm, funcnm );
            fprintf( file,
                     "            *logStream << \"unset\" << std::endl;\n        }\n    }\n" );
            fprintf( file, "#endif\n" );
        }
        fprintf( file, "    _%s = x;\n}\n", attrnm );
    }
}

/******************************************************************
**      Entity Generation                */

/**
 * print entity descriptors and attrdescriptors to the namespace in files->names
 * hopefully this file can be safely included everywhere, eliminating use of 'extern'
 *
 * Nov 2011 - MAP - This function was split out of ENTITYhead_print to enable
 *                  use of a separate header with a namespace.
 */
void ENTITYnames_print( Entity entity, FILE * file, Schema schema ) {
    char attrnm [BUFSIZ];
    //Linked_List list;
    int attr_count_tmp = attr_count;
    Entity super = 0;

    fprintf( file, "    extern EntityDescriptor *%s%s;\n", ENT_PREFIX, ENTITYget_name( entity ) );

    /* DAS print all the attr descriptors and inverse attr descriptors for an
     *       entity as defs in the .h file. */
    LISTdo( ENTITYget_attributes( entity ), v, Variable )
    generate_attribute_name( v, attrnm );
    fprintf( file, "    extern %s *%s%d%s%s;\n",
             ( VARget_inverse( v ) ? "Inverse_attribute" : ( VARis_derived( v ) ? "Derived_attribute" : "AttrDescriptor" ) ),
             ATTR_PREFIX, attr_count_tmp++,
             ( VARis_derived( v ) ? "D" : ( VARis_type_shifter( v ) ? "R" : ( VARget_inverse( v ) ? "I" : "" ) ) ),
             attrnm );
    LISTod;
}


/**************************************************************//**
 ** Procedure:  ENTITYhead_print
 ** Parameters:  const Entity entity
 **   FILE* file  --  file being written to
 ** Returns:
 ** Description:  prints the beginning of the entity class definition for the
 **               c++ code and the declaration of attr descriptors for
 **       the registry.  In the .h file
 ** Side Effects:  generates c++ code
 ** Status:  good 1/15/91
 **          added registry things 12-Apr-1993
 **          remove extern keyword - MAP - Nov 2011
 **          split out stuff in namespace to ENTITYdesc_print - MAP - Nov 2011
 ******************************************************************/
void ENTITYhead_print( Entity entity, FILE * file, Schema schema ) {
    char entnm [BUFSIZ];
    Linked_List list;
    Entity super = 0;

    strncpy( entnm, ENTITYget_classname( entity ), BUFSIZ );
    entnm[BUFSIZ-1] = '\0';

    fprintf( file, "\nclass %s  :  ", entnm );

    /* inherit from either supertype entity class or root class of
       all - i.e. SDAI_Application_instance */

    if( multiple_inheritance ) {
        list = ENTITYget_supertypes( entity );
        if( ! LISTempty( list ) ) {
            super = ( Entity )LISTpeek_first( list );
        }
    } else { /* the old way */
        super = ENTITYput_superclass( entity );
    }

    if( super ) {
        fprintf( file, "  public %s  {\n ", ENTITYget_classname( super ) );
    } else {
        fprintf( file, "  public SDAI_Application_instance {\n" );
    }
}

/**************************************************************//**
 ** Procedure:  DataMemberPrintAttr
 ** Parameters:  Entity entity  --  entity being processed
 **              Variable a -- attribute being processed
 **              FILE* file  --  file being written to
 ** Returns:
 ** Description:  prints out the current attribute for an entity's c++ class
 **               definition
 ******************************************************************/
void DataMemberPrintAttr( Entity entity, Variable a, FILE * file ) {
    char attrnm [BUFSIZ];
    const char * ctype, * etype;
    if( VARget_initializer( a ) == EXPRESSION_NULL ) {
        ctype = TYPEget_ctype( VARget_type( a ) );
        generate_attribute_name( a, attrnm );
        if( !strcmp( ctype, "SCLundefined" ) ) {
            printf( "WARNING:  in entity %s, ", ENTITYget_name( entity ) );
            printf( " the type for attribute  %s is not fully implemented\n", attrnm );
        }
        if( TYPEis_entity( VARget_type( a ) ) ) {
            fprintf( file, "        SDAI_Application_instance_ptr _%s;", attrnm );
        } else if( TYPEis_aggregate( VARget_type( a ) ) ) {
            fprintf( file, "        %s_ptr _%s ;", ctype, attrnm );
        } else {
            fprintf( file, "        %s _%s ;", ctype, attrnm );
        }
        if( VARget_optional( a ) ) {
            fprintf( file, "    //  OPTIONAL" );
        }
        if( isAggregate( a ) )        {
            /*  if it's a named type, comment the type  */
            if( ( etype = TYPEget_name
                          ( TYPEget_nonaggregate_base_type( VARget_type( a ) ) ) ) ) {
                fprintf( file, "          //  of  %s\n", etype );
            }
        }

        fprintf( file, "\n" );
    }
}

/**************************************************************//**
 ** Procedure:  DataMemberPrint
 ** Parameters:  const Entity entity  --  entity being processed
 **   FILE* file  --  file being written to
 ** Returns:
 ** Description:  prints out the data members for an entity's c++ class
 **               definition
 ** Side Effects:  generates c++ code
 ** Status:  ok 1/15/91
 ******************************************************************/
void DataMemberPrint( Entity entity, Linked_List neededAttr, FILE * file, Schema schema ) {
    Linked_List attr_list;
    char entnm [BUFSIZ];
    strncpy( entnm, ENTITYget_classname( entity ), BUFSIZ ); /*  assign entnm  */

    /*  print list of attributes in the protected access area   */
    fprintf( file, "   protected:\n" );

    attr_list = ENTITYget_attributes( entity );
    LISTdo( attr_list, attr, Variable ) {
        DataMemberPrintAttr( entity, attr, file );
    }
    LISTod;

    // add attributes for parent attributes not inherited through C++ inheritance.
    if( multiple_inheritance ) {
        LISTdo( neededAttr, attr, Variable ) {
            DataMemberPrintAttr( entity, attr, file );
        }
        LISTod;
    }
}

/**************************************************************//**
 ** Procedure:  collectAttributes
 ** Parameters:  Linked_List curList  --  current list to store the
 **  attributes
 **   Entity curEntity -- current Entity being processed
 **   int flagParent -- flag control
 ** Returns:
 ** Description:  Retrieve the list of inherited attributes of an
 ** entity
 ******************************************************************/
enum CollectType { ALL, ALL_BUT_FIRST, FIRST_ONLY };

static void collectAttributes( Linked_List curList, const Entity curEntity, enum CollectType collect ) {
    Linked_List parent_list = ENTITYget_supertypes( curEntity );

    if( ! LISTempty( parent_list ) ) {
        if( collect != FIRST_ONLY ) {
            // collect attributes from parents and their supertypes
            LISTdo( parent_list, e, Entity ) {
                if( collect == ALL_BUT_FIRST ) {
                    // skip first and collect from the rest
                    collect = ALL;
                } else {
                    // collect attributes of this parent and its supertypes
                    collectAttributes( curList, e, ALL );
                }
            }
            LISTod;
        } else {
            // collect attributes of only first parent and its supertypes
            collectAttributes( curList, ( Entity ) LISTpeek_first( parent_list ), ALL );
        }
    }
    // prepend this entity's attributes to the result list
    LISTdo( ENTITYget_attributes( curEntity ), attr, Variable ) {
        LISTadd_first( curList, ( Generic ) attr );
    }
    LISTod;
}

/**************************************************************//**
 ** Procedure:  MemberFunctionSign
 ** Parameters:  Entity *entity --  entity being processed
 **     FILE* file  --  file being written to
 ** Returns:
 ** Description:  prints the signature for member functions
                  of an entity's class definition
 **       DAS prints the end of the entity class def and the creation
 **       function that the EntityTypeDescriptor uses.
 ** Side Effects:  prints c++ code to a file
 ** Status:  ok 1/1/5/91
 **  updated 17-Feb-1992 to print only the signature
             and not the function definitions
 ******************************************************************/
void MemberFunctionSign( Entity entity, Linked_List neededAttr, FILE * file ) {

    Linked_List attr_list;
    static int entcode = 0;
    char entnm [BUFSIZ];

    strncpy( entnm, ENTITYget_classname( entity ), BUFSIZ ); /*  assign entnm  */
    entnm[BUFSIZ-1] = '\0';

    fprintf( file, "    public: \n" );

    /*  put in member functions which belong to all entities    */
    /*  constructors:    */
    fprintf( file, "        %s();\n", entnm );
    fprintf( file, "        %s( SDAI_Application_instance *se, bool addAttrs = true );\n", entnm );
    /*  copy constructor */
    fprintf( file, "        %s( %s & e );\n", entnm, entnm );
    /*  destructor: */
    fprintf( file, "        ~%s();\n", entnm );

    fprintf( file, "        int opcode() {\n            return %d;\n        }\n", entcode++ );

    /*  print signature of access functions for attributes      */
    attr_list = ENTITYget_attributes( entity );
    LISTdo( attr_list, a, Variable ) {
        if( VARget_initializer( a ) == EXPRESSION_NULL ) {

            /*  retrieval  and  assignment  */
            ATTRsign_access_methods( a, file );
        }
    }
    LISTod;

    /* //////////////// */
    if( multiple_inheritance ) {
        // add the EXPRESS inherited attributes which are non
        // inherited in C++
        LISTdo( neededAttr, attr, Variable ) {
            if( ! VARis_derived( attr ) && ! VARis_overrider( entity, attr ) ) {
                ATTRsign_access_methods( attr, file );
            }
        }
        LISTod;

    }
    /* //////////////// */
    fprintf( file, "};\n" );

    /*  print creation function for class   */
    fprintf( file, "inline %s * create_%s() {\n    return  new %s;\n}\n", entnm, entnm, entnm );
}

/**************************************************************//**
 ** Procedure:    LIBdescribe_entity (entity, file, schema)
 ** Parameters:  Entity entity --  entity being processed
 **     FILE* file  --  file being written to
 **     Schema schema -- schema being processed
 ** Returns:
 ** Description:  declares the global pointer to the EntityDescriptor
                  representing a particular entity
 **       DAS also prints the attr descs and inverse attr descs
 **       This function creates the storage space for the externs defs
 **       that were defined in the .h file. These global vars go in
 **       the .cc file.
 ** Side Effects:  prints c++ code to a file
 ** Status:  ok 12-Apr-1993
 ******************************************************************/
void LIBdescribe_entity( Entity entity, FILE * file, Schema schema ) {
    Linked_List list;
    int attr_count_tmp = attr_count;
    char attrnm [BUFSIZ];

    fprintf( file, "EntityDescriptor * %s::%s%s = 0;\n", SCHEMAget_name( schema ), ENT_PREFIX, ENTITYget_name( entity ) );

    LISTdo( ENTITYget_attributes( entity ), v, Variable )
    generate_attribute_name( v, attrnm );
    fprintf( file, "%s * %s::%s%d%s%s = 0;\n",
             ( VARget_inverse( v ) ? "Inverse_attribute" : ( VARis_derived( v ) ? "Derived_attribute" : "AttrDescriptor" ) ),
             SCHEMAget_name( schema ), ATTR_PREFIX, attr_count_tmp++,
             ( VARis_derived( v ) ? "D" : ( VARis_type_shifter( v ) ? "R" : ( VARget_inverse( v ) ? "I" : "" ) ) ), attrnm );
    LISTod

}

/**************************************************************//**
 ** Procedure:  LIBmemberFunctionPrint
 ** Parameters:  Entity *entity --  entity being processed
 **     FILE* file  --  file being written to
 ** Returns:
 ** Description:  prints the member functions for the class
                  representing an entity.  These go in the .cc file
 ** Side Effects:  prints c++ code to a file
 ** Status:  ok 17-Feb-1992
 ******************************************************************/
void LIBmemberFunctionPrint( Entity entity, Linked_List neededAttr, FILE * file ) {

    Linked_List attr_list;
    char entnm [BUFSIZ];

    strncpy( entnm, ENTITYget_classname( entity ), BUFSIZ ); /*  assign entnm */

    /*  1. put in member functions which belong to all entities */
    /*  the common function are still in the class definition 17-Feb-1992 */

    /*  2. print access functions for attributes    */
    attr_list = ENTITYget_attributes( entity );
    LISTdo( attr_list, a, Variable )
    /*  do for EXPLICIT, REDEFINED, and INVERSE attributes - but not DERIVED */
    if( ! VARis_derived( a ) )  {

        /*  retrieval  and  assignment   */
        ATTRprint_access_methods( entnm, a, file );
    }
    LISTod;
    /* //////////////// */
    if( multiple_inheritance ) {
        LISTdo( neededAttr, attr, Variable ) {
            if( ! VARis_derived( attr ) && ! VARis_overrider( entity, attr ) ) {
                ATTRprint_access_methods( entnm, attr, file );
            }
        }
        LISTod;
    }
    /* //////////////// */

}

/**************************************************************//**
 ** Procedure:  ENTITYinc_print
 ** Parameters:  Entity *entity --  entity being processed
 **     FILE* file  --  file being written to
 ** Returns:
 ** Description:  drives the generation of the c++ class definition code
 ** Side Effects:  prints segment of the c++ .h file
 ** Status:  ok 1/15/91
 ******************************************************************/
void ENTITYinc_print( Entity entity, Linked_List neededAttr, FILE * file, Schema schema ) {
    ENTITYhead_print( entity, file, schema );
    DataMemberPrint( entity, neededAttr, file, schema );
    MemberFunctionSign( entity, neededAttr, file );
}

/**************************************************************//**
 ** Procedure:  LIBcopy_constructor
 ** Parameters:
 ** Returns:
 ** Description:
 ** Side Effects:
 ** Status:  not used 17-Feb-1992
 ******************************************************************/
void LIBcopy_constructor( Entity ent, FILE * file ) {
    Linked_List attr_list;
    Class_Of_Type class;
    Type t;
    char buffer [BUFSIZ],
         attrnm[BUFSIZ],
         *b = buffer;
    int count = attr_count;

    const char * entnm = ENTITYget_classname( ent );
    const char * StrToLower( const char * word );

    /*mjm7/10/91 copy constructor definition  */
    fprintf( file, "        %s::%s(%s& e )\n", entnm, entnm, entnm );
    fprintf( file, "  {" );

    /*  attributes  */
    attr_list = ENTITYget_attributes( ent );
    LISTdo( attr_list, a, Variable )
    if( VARget_initializer( a ) == EXPRESSION_NULL ) {
        /*  include attribute if it is not derived  */
        generate_attribute_name( a, attrnm );
        t = VARget_type( a );
        class = TYPEget_type( t );

        /*  1. initialize everything to NULL (even if not optional)  */

        /*    default:  to intialize attribute to NULL  */
        sprintf( b, "        _%s = e.%s();\n", attrnm, attrnm );

        /*mjm7/11/91  case TYPE_STRING */
        if( ( class == string_ ) || ( class == binary_ ) ) {
            sprintf( b, "        _%s = strdup(e.%s());\n", attrnm, attrnm );
        }


        /*      case TYPE_ENTITY:   */
        if( class == entity_ ) {
            sprintf( b, "        _%s = e.%s();\n", attrnm, attrnm );
        }
        /* previous line modified to conform with SDAI C++ Binding for PDES, Inc. Prototyping 5/22/91 CD */

        /*    case TYPE_ENUM:   */
        if( class == enumeration_ ) {
            sprintf( b, "        _%s.put(e.%s().asInt());\n", attrnm, attrnm );
        }
        /*    case TYPE_SELECT: */
        if( class == select_ ) {
            sprintf( b, "DDDDDDD        _%s.put(e.%s().asInt());\n", attrnm, attrnm );
        }
        /*   case TYPE_BOOLEAN    */
        if( class == boolean_ ) {
            sprintf( b, "        _%s.put(e.%s().asInt());\n", attrnm, attrnm );
        }
        /* previous line modified to conform with SDAI C++ Binding for PDES, Inc. Prototyping 5/22/91 CD */

        /*   case TYPE_LOGICAL    */
        if( class == logical_ ) {
            sprintf( b, "        _%s.put(e.%s().asInt());\n", attrnm, attrnm );
        }
        /* previous line modified to conform with SDAI C++ Binding for PDES, Inc. Prototyping 5/22/91 CD */

        /*  case TYPE_ARRAY:
        case TYPE_LIST:
          case TYPE_SET:
          case TYPE_BAG:  */
        if( isAggregateType( t ) ) {
            *b = '\0';
        }

        fprintf( file, "%s", b )       ;

        fprintf( file, "         attributes.push " );

        /*  2.  put attribute on attributes list    */

        /*  default:    */

        fprintf( file, "\n        (new STEPattribute(*%s%d%s, %s &_%s));\n",
                 ATTR_PREFIX, count,
                 attrnm,
                 ( TYPEis_entity( t ) ? "(SDAI_Application_instance_ptr *)" : "" ),
                 attrnm );
        ++count;

    }
    LISTod;
    fprintf( file, " }\n" );
}

int get_attribute_number( Entity entity ) {
    int i = 0;
    int found = 0;
    Linked_List local, complete;
    complete = ENTITYget_all_attributes( entity );
    local = ENTITYget_attributes( entity );

    LISTdo( local, a, Variable )
    /*  go to the child's first explicit attribute */
    if( ( ! VARget_inverse( a ) ) && ( ! VARis_derived( a ) ) )  {
        LISTdo( complete, p, Variable )
        /*  cycle through all the explicit attributes until the
        child's attribute is found  */
        if( !found && ( ! VARget_inverse( p ) ) && ( ! VARis_derived( p ) ) ) {
            if( p != a ) {
                ++i;
            } else {
                found = 1;
            }
        }
        LISTod;
        if( found ) {
            return i;
        } else printf( "Internal error:  %s:%d\n"
                           "Attribute %s not found.\n"
                           , __FILE__, __LINE__, EXPget_name( VARget_name( a ) ) );
    }

    LISTod;
    return -1;
}

/// initialize attributes in the constructor; used for two different constructors
void initializeAttrs( Entity e, FILE* file ) {
    const orderedAttr * oa;
    orderedAttrsInit( e );
    while( 0 != ( oa = nextAttr() ) ) {
        if( oa->deriver ) {
            fprintf( file, "    MakeDerived( \"%s\", \"%s\" );\n", oa->attr->name->symbol.name, oa->creator->symbol.name );
        }
    }
    orderedAttrsCleanup();
}

/**************************************************************//**
 ** Procedure:  LIBstructor_print
 ** Parameters:  Entity *entity --  entity being processed
 **     FILE* file  --  file being written to
 ** Returns:
 ** Description:  prints the c++ code for entity class's
 **     constructor and destructor.  goes to .cc file
 ** Side Effects:  generates codes segment in c++ .cc file
 ** Status:  ok 1/15/91
 ** Changes: Modified generator to initialize attributes to NULL based
 **          on the NULL symbols defined in "SDAI C++ Binding for PDES,
 **          Inc. Prototyping" by Stephen Clark.
 ** Change Date: 5/22/91 CD
 ** Changes: Modified STEPattribute constructors to take fewer arguments
 **     21-Dec-1992 -kcm
 ******************************************************************/
void LIBstructor_print( Entity entity, FILE * file, Schema schema ) {
    Linked_List attr_list;
    Type t;
    char attrnm [BUFSIZ];

    Linked_List list;
    Entity principalSuper = 0;

    const char * entnm = ENTITYget_classname( entity );
    int count = attr_count;
    bool first = true;

    /*  constructor definition  */

    //parent class initializer (if any) and '{' printed below
    fprintf( file, "%s::%s()", entnm, entnm );

    /* ////MULTIPLE INHERITANCE//////// */

    if( multiple_inheritance ) {
        int super_cnt = 0;
        list = ENTITYget_supertypes( entity );
        if( ! LISTempty( list ) ) {
            LISTdo( list, e, Entity ) {
                /*  if there's no super class yet,
                    or the super class doesn't have any attributes
                */

                super_cnt++;
                if( super_cnt == 1 ) {
                    /* ignore the 1st parent */
                    const char * parent = ENTITYget_classname( e );

                    //parent class initializer
                    fprintf( file, ": %s() {\n", parent );
                    fprintf( file, "        /*  parent: %s  */\n%s\n%s\n", parent,
                            "        /* Ignore the first parent since it is */",
                            "        /* part of the main inheritance hierarchy */"  );
                    principalSuper = e; /* principal SUPERTYPE */
                } else {
                    fprintf( file, "        /*  parent: %s  */\n", ENTITYget_classname( e ) );
                    fprintf( file, "    HeadEntity(this);\n" );
                    fprintf( file, "    AppendMultInstance(new %s(this));\n",
                            ENTITYget_classname( e ) );

                    if( super_cnt == 2 ) {
                        printf( "\nMULTIPLE INHERITANCE for entity: %s\n",
                                ENTITYget_name( entity ) );
                        printf( "        SUPERTYPE 1: %s (principal supertype)\n",
                                ENTITYget_name( principalSuper ) );
                    }
                    printf( "        SUPERTYPE %d: %s\n", super_cnt, ENTITYget_name( e ) );
                }
            } LISTod;

        } else {    /*  if entity has no supertypes, it's at top of hierarchy  */
            // no parent class constructor has been printed, so still need an opening brace
            fprintf( file, " {\n" );
            fprintf( file, "        /*  no SuperTypes */\n" );
        }
    }

    /* what if entity comes from other schema?
     * It appears that entity.superscope.symbol.name is the schema name (but only if entity.superscope.type == 's'?)  --MAP 27Nov11
     */
    fprintf( file, "\n    eDesc = %s::%s%s;\n",
             SCHEMAget_name( schema ), ENT_PREFIX, ENTITYget_name( entity ) );

    attr_list = ENTITYget_attributes( entity );

    LISTdo( attr_list, a, Variable )
    if( VARget_initializer( a ) == EXPRESSION_NULL ) {
        /*  include attribute if it is not derived  */
        generate_attribute_name( a, attrnm );
        t = VARget_type( a );

        if( ( ! VARget_inverse( a ) ) && ( ! VARis_derived( a ) ) )  {
            /*  1. create a new STEPattribute */

            // if type is aggregate, the variable is a pointer and needs initialized
            if( TYPEis_aggregate( t ) ) {
                fprintf( file, "    _%s = new %s;\n", attrnm, TYPEget_ctype( t ) );
            }
            fprintf( file, "    %sa = new STEPattribute( * %s::%s%d%s%s, %s %s_%s );\n",
                     ( first ? "STEPattribute * " : "" ), //  first time through, declare 'a'
                     SCHEMAget_name( schema ),
                     ATTR_PREFIX, count,
                     ( VARis_type_shifter( a ) ? "R" : "" ),
                     attrnm,
                     ( TYPEis_entity( t ) ? "( SDAI_Application_instance_ptr * )" : "" ),
                     ( TYPEis_aggregate( t ) ? "" : "& " ),
                     attrnm );
            if( first ) {
                first = false;
            }
            /*  2. initialize everything to NULL (even if not optional)  */

            fprintf( file, "    a->set_null();\n" );

            /*  3.  put attribute on attributes list  */
            fprintf( file, "    attributes.push( a );\n" );

            /* if it is redefining another attribute make connection of
               redefined attribute to redefining attribute */
            if( VARis_type_shifter( a ) ) {
                fprintf( file, "    MakeRedefined( a, \"%s\" );\n",
                         VARget_simple_name( a ) );
            }
        }
        count++;
    }

    LISTod;

    initializeAttrs( entity, file );

    fprintf( file, "}\n" );

    /*  copy constructor  */
    /*  LIBcopy_constructor (entity, file); */
    entnm = ENTITYget_classname( entity );
    fprintf( file, "%s::%s ( %s & e ) : ", entnm, entnm, entnm );

    /* include explicit initialization of base class */
    if( principalSuper ) {
        fprintf( file, "%s()", ENTITYget_classname( principalSuper ) );
    } else {
        fprintf( file, "SDAI_Application_instance()" );
    }

    fprintf( file, " {\n    CopyAs( ( SDAI_Application_instance_ptr ) & e );\n}\n" );

    /*  print destructor  */
    /*  currently empty, but should check to see if any attributes need
    to be deleted -- attributes will need reference count  */

    entnm = ENTITYget_classname( entity );
    fprintf( file, "%s::~%s() {\n", entnm, entnm );

    attr_list = ENTITYget_attributes( entity );

    LISTdo( attr_list, a, Variable )
    if( VARget_initializer( a ) == EXPRESSION_NULL ) {
        generate_attribute_name( a, attrnm );
        t = VARget_type( a );

        if( ( ! VARget_inverse( a ) ) && ( ! VARis_derived( a ) ) )  {
            if( TYPEis_aggregate( t ) ) {
                fprintf( file, "    delete _%s;\n", attrnm );
            }
        }
    }
    LISTod;

    fprintf( file, "}\n" );
}

/********************/
/** print the constructor that accepts a SDAI_Application_instance as an argument used
   when building multiply inherited entities.
   \sa LIBstructor_print()
*/
void LIBstructor_print_w_args( Entity entity, FILE * file, Schema schema ) {
    Linked_List attr_list;
    Type t;
    char attrnm [BUFSIZ];

    Linked_List list;
    int super_cnt = 0;

    /* added for calling parents constructor if there is one */
    char parentnm [BUFSIZ];
    char * parent = 0;

    const char * entnm;
    int count = attr_count;
    bool first = true;

    if( multiple_inheritance ) {
        Entity parentEntity = 0;
        list = ENTITYget_supertypes( entity );
        if( ! LISTempty( list ) ) {
            parentEntity = ( Entity )LISTpeek_first( list );
            if( parentEntity ) {
                strcpy( parentnm, ENTITYget_classname( parentEntity ) );
                parent = parentnm;
            } else {
                parent = 0;    /* no parent */
            }
        } else {
            parent = 0;    /* no parent */
        }

        /* ENTITYget_classname returns a static buffer so don't call it twice
           before it gets used - (I didn't write it) - I had to move it below
            the above use. DAS */
        entnm = ENTITYget_classname( entity );
        /*  constructor definition  */
        if( parent )
            fprintf( file, "%s::%s( SDAI_Application_instance * se, bool addAttrs ) : %s( se, addAttrs ) {\n", entnm, entnm, parentnm );
        else {
            fprintf( file, "%s::%s( SDAI_Application_instance * se, bool addAttrs ) {\n", entnm, entnm );
        }

        fprintf( file, "    /* Set this to point to the head entity. */\n" );
        fprintf( file, "    HeadEntity(se);\n" );
        if( !parent ) {
            fprintf( file, "    ( void ) addAttrs; /* quell potentially unused var */\n\n" );
        }

        list = ENTITYget_supertypes( entity );
        if( ! LISTempty( list ) ) {
            LISTdo( list, e, Entity )
            /*  if there's no super class yet,
                or the super class doesn't have any attributes
                */
            fprintf( file, "        /* parent: %s */\n", ENTITYget_classname( e ) );

            super_cnt++;
            if( super_cnt == 1 ) {
                /* ignore the 1st parent */
                fprintf( file,
                         "        /* Ignore the first parent since it is part *\n%s\n",
                         "        ** of the main inheritance hierarchy        */" );
            }  else {
                fprintf( file, "    se->AppendMultInstance( new %s( se, addAttrs ) );\n",
                         ENTITYget_classname( e ) );
            }
            LISTod;

        }  else {   /*  if entity has no supertypes, it's at top of hierarchy  */
            fprintf( file, "        /*  no SuperTypes */\n" );
        }

        /* what if entity comes from other schema? */
        fprintf( file, "\n    eDesc = %s::%s%s;\n",
                 SCHEMAget_name( schema ), ENT_PREFIX, ENTITYget_name( entity ) );

        attr_list = ENTITYget_attributes( entity );

        LISTdo( attr_list, a, Variable )
        if( VARget_initializer( a ) == EXPRESSION_NULL ) {
            /*  include attribute if it is not derived  */
            generate_attribute_name( a, attrnm );
            t = VARget_type( a );

            if( ( ! VARget_inverse( a ) ) && ( ! VARis_derived( a ) ) )  {
                /*  1. create a new STEPattribute */

                // if type is aggregate, the variable is a pointer and needs initialized
                if( TYPEis_aggregate( t ) ) {
                    fprintf( file, "    _%s = new %s;\n", attrnm, TYPEget_ctype( t ) );
                }
                fprintf( file, "    %sa = new STEPattribute( * %s::%s%d%s%s, %s %s_%s );\n",
                         ( first ? "STEPattribute * " : "" ), //  first time through, declare a
                         SCHEMAget_name( schema ),
                         ATTR_PREFIX, count,
                         ( VARis_type_shifter( a ) ? "R" : "" ),
                         attrnm,
                         ( TYPEis_entity( t ) ? "( SDAI_Application_instance_ptr * )" : "" ),
                         ( TYPEis_aggregate( t ) ? "" : "&" ),
                         attrnm );

                if( first ) {
                    first = false;
                }

                fprintf( file, "        /* initialize to NULL (even if not optional)  */\n" );
                fprintf( file, "    a -> set_null();\n" );

                fprintf( file, "        /* Put attribute on this class' attributes list so the access functions still work. */\n" );
                fprintf( file, "    attributes.push( a );\n" );

                fprintf( file, "        /* Put attribute on the attributes list for the main inheritance heirarchy.  **\n" );
                fprintf( file, "        ** The push method rejects duplicates found by comparing attrDescriptor's.   */\n" );
                fprintf( file, "    if( addAttrs ) {\n" );
                fprintf( file, "        se->attributes.push( a );\n    }\n" );

                /* if it is redefining another attribute make connection of redefined attribute to redefining attribute */
                if( VARis_type_shifter( a ) ) {
                    fprintf( file, "    MakeRedefined( a, \"%s\" );\n",
                             VARget_simple_name( a ) );
                }
            }
            count++;
        }

        LISTod;

        initializeAttrs( entity, file );

        fprintf( file, "}\n" );
    } /* end if(multiple_inheritance) */

}

/**************************************************************//**
 ** Procedure:  ENTITYlib_print
 ** Parameters:  Entity *entity --  entity being processed
 **     FILE* file  --  file being written to
 ** Returns:
 ** Description:  drives the printing of the code for the class library
 **     additional member functions can be generated by writing a routine
 **     to generate the code and calling that routine from this procedure
 ** Side Effects:  generates code segment for c++ library file
 ** Status:  ok 1/15/91
 ******************************************************************/
void ENTITYlib_print( Entity entity, Linked_List neededAttr, FILE * file, Schema schema ) {
    LIBdescribe_entity( entity, file, schema );
    LIBstructor_print( entity, file, schema );
    if( multiple_inheritance ) {
        LIBstructor_print_w_args( entity, file, schema );
    }
    LIBmemberFunctionPrint( entity, neededAttr, file );
}

/** return 1 if types are predefined by us */
bool TYPEis_builtin( const Type t ) {
    switch( TYPEget_body( t )->type ) { /* dunno if correct*/
        case integer_:
        case real_:
        case string_:
        case binary_:
        case boolean_:
        case number_:
        case logical_:
            return true;
        default:
            break;
    }
    return false;
}

/**
 * For aggregates, initialize Bound1, Bound2, OptionalElements, UniqueElements.
 * Handles bounds that depend on attributes (via SetBound1FromMemberAccessor() ), or
 * on function calls (via SetBound1FromExpressFuncall() ). In C++, runtime bounds
 * look like `t_0->Bound2FromMemberAccessor(SdaiStructured_mesh::index_count_);`
 * \param f the file to write to
 * \param t the Type
 * \param var_name the name of the C++ variable, such as t_1 or schema::t_name
 */
void AGGRprint_init( FILES * files, const Type t, const char * var_name, const char * aggr_name ) {
    if( !TYPEget_head( t ) ) {
        //the code for lower and upper is almost identical
        if( TYPEget_body( t )->lower ) {
            if( TYPEget_body( t )->lower->symbol.resolved ) {
                if( TYPEget_body( t )->lower->type == Type_Funcall ) {
                    fprintf( files->init, "        %s->SetBound1FromExpressFuncall(\"%s\");\n", var_name,
                             EXPRto_string( TYPEget_body( t )->lower ) );
                } else {
                    fprintf( files->init, "        %s->SetBound1(%d);\n", var_name, TYPEget_body( t )->lower->u.integer );
                }
            } else { //resolved == 0 seems to mean that this is Type_Runtime
                assert( ( t->superscope ) && ( t->superscope->symbol.name ) && ( TYPEget_body( t )->lower->e.op2 ) &&
                        ( TYPEget_body( t )->lower->e.op2->symbol.name ) );
                fprintf( files->init, "        %s->SetBound1FromMemberAccessor( &getBound1_%s__%s );\n", var_name,
                         ClassName( t->superscope->symbol.name ), aggr_name );
                fprintf( files->helpers, "inline SDAI_Integer getBound1_%s__%s( SDAI_Application_instance* this_ptr ) {\n",
                         ClassName( t->superscope->symbol.name ), aggr_name );
                fprintf( files->helpers, "    return ( (%s *) this_ptr)->%s_();\n}\n",
                         ClassName( t->superscope->symbol.name ), TYPEget_body( t )->lower->e.op2->symbol.name );
            }
        }
        if( TYPEget_body( t )->upper ) {
            if( TYPEget_body( t )->upper->symbol.resolved ) {
                if( TYPEget_body( t )->upper->type == Type_Funcall ) {
                    fprintf( files->init, "        %s->SetBound2FromExpressFuncall(\"%s\");\n", var_name,
                             EXPRto_string( TYPEget_body( t )->upper ) );
                } else {
                    fprintf( files->init, "        %s->SetBound2(%d);\n", var_name, TYPEget_body( t )->upper->u.integer );
                }
            } else { //resolved == 0 seems to mean that this is Type_Runtime
                assert( ( t->superscope ) && ( t->superscope->symbol.name ) && ( TYPEget_body( t )->upper->e.op2 ) &&
                        ( TYPEget_body( t )->upper->e.op2->symbol.name ) );
                fprintf( files->init, "        %s->SetBound2FromMemberAccessor( &getBound2_%s__%s );\n", var_name,
                         ClassName( t->superscope->symbol.name ), aggr_name );
                fprintf( files->helpers, "inline SDAI_Integer getBound2_%s__%s( SDAI_Application_instance* this_ptr ) {\n",
                         ClassName( t->superscope->symbol.name ), aggr_name );
                fprintf( files->helpers, "    return ( (%s *) this_ptr)->%s_();\n}\n",
                         ClassName( t->superscope->symbol.name ), TYPEget_body( t )->upper->e.op2->symbol.name );
            }
        }
        if( TYPEget_body( t )->flags.unique ) {
            fprintf( files->init, "        %s->UniqueElements(LTrue);\n", var_name );
        }
        if( TYPEget_body( t )->flags.optional ) {
            fprintf( files->init, "        %s->OptionalElements(LTrue);\n", var_name );
        }
    }
}

/** go down through a type's base type chain,
   make and print new TypeDescriptors for each type with no name.

   This function should only be called for types that don't have an
   associated Express name.  Currently this only includes aggregates.
   If this changes this function needs to be changed to support the type
   that changed.  This function prints TypeDescriptors for types
   without names and it will go down through the type chain until it hits
   a type that has a name.  i.e. when it hits a type with a name it stops.
   There are only two places where a type can not have a name - both
   cases are aggregate types.
   1. an aggregate created in an attr declaration
      e.g. names : ARRAY [1:3] of STRING;
   2. an aggregate that is an element of another aggregate.
      e.g. TYPE Label = STRING; END_TYPE;
           TYPE listSetOfLabel = LIST of SET of Label; END_TYPE;
      LIST of SET of Label has a name i.e. listSetOfReal
      SET of Label does not have a name and this function should be called
         to generate one.
      This function will not generate the code to handle Label.

      Type t contains the Type with no Express name that needs to have
        TypeDecriptor[s] generated for it.
      buf needs to have space declared enough to hold the name of the var
        that can be referenced to refer to the type that was created for
    Type t.
*/
void print_typechain( FILES * files, const Type t, char * buf, Schema schema, const char * type_name ) {
    /* if we've been called, current type has no name */
    /* nor is it a built-in type */
    /* the type_count variable is there for debugging purposes  */

    const char * ctype = TYPEget_ctype( t );
    int count = type_count++;
    char name_buf[MAX_LEN];
    int s;

    switch( TYPEget_body( t )->type ) {
        case aggregate_:
        case array_:
        case bag_:
        case set_:
        case list_:
            /* create a new TypeDescriptor variable, e.g. t1, and new space for it */
            fprintf( files->init, "        %s * %s%d = new %s;\n",
                     GetTypeDescriptorName( t ), TD_PREFIX, count,
                     GetTypeDescriptorName( t ) );

            fprintf( files->init,
                     "        %s%d->AssignAggrCreator((AggregateCreator) create_%s);%s",
                     TD_PREFIX, count, ctype, "        // Creator function\n" );

            s = sprintf( name_buf, "%s%d", TD_PREFIX, count );
            assert( ( s > 0 ) && ( s < MAX_LEN ) );
            AGGRprint_init( files, t, name_buf, type_name );

            break;

        default: /* this should not happen since only aggregates are allowed to
          not have a name. This funct should only be called for aggrs
          without names. */
            fprintf( files->init, "        TypeDescriptor * %s%d = new TypeDescriptor;\n",
                     TD_PREFIX, count );
    }

    /* there is no name so name doesn't need to be initialized */

    fprintf( files->init, "        %s%d->FundamentalType(%s);\n", TD_PREFIX, count,
             FundamentalType( t, 1 ) );
    fprintf( files->init, "        %s%d->Description(\"%s\");\n", TD_PREFIX, count,
             TypeDescription( t ) );

    /* DAS ORIG SCHEMA FIX */
    fprintf( files->init, "        %s%d->OriginatingSchema(%s::schema);\n", TD_PREFIX, count, SCHEMAget_name( schema ) );

    if( TYPEget_RefTypeVarNm( t, name_buf, schema ) ) {
        fprintf( files->init, "        %s%d->ReferentType(%s);\n", TD_PREFIX, count, name_buf );
    } else {
        Type base = 0;
        /* no name, recurse */
        char callee_buffer[MAX_LEN];
        if( TYPEget_body( t ) ) {
            base = TYPEget_body( t )->base;
        }
        print_typechain( files, base, callee_buffer, schema, type_name );
        fprintf( files->init, "        %s%d->ReferentType(%s);\n", TD_PREFIX, count, callee_buffer );
    }
    sprintf( buf, "%s%d", TD_PREFIX, count );

    /* Types */
    fprintf( files->init, "        %s::schema->AddUnnamedType(%s%d);\n", SCHEMAget_name( schema ), TD_PREFIX, count );
}

/**************************************************************//**
 ** Procedure:  ENTITYincode_print
 ** Parameters:  Entity *entity --  entity being processed
 **     FILE* file  --  file being written to
 ** Returns:
 ** Description:  generates code to enter entity in STEP registry
 **      This goes to the .init.cc file
 ** Side Effects:
 ** Status:  ok 1/15/91
 ******************************************************************/
void ENTITYincode_print( Entity entity, FILES * files, Schema schema ) {
#define entity_name ENTITYget_name(entity)
#define schema_name SCHEMAget_name(schema)
    char attrnm [BUFSIZ];
    char dict_attrnm [BUFSIZ];
    const char * super_schema;
    char * tmp, *tmp2;

#ifdef NEWDICT
    /* DAS New SDAI Dictionary 5/95 */
    /* insert the entity into the schema descriptor */
    fprintf( files->init,
             "        ((SDAIAGGRH(Set,EntityH))%s::schema->Entities())->Add(%s::%s%s);\n",
             schema_name, schema_name, ENT_PREFIX, entity_name );
#endif

    if( ENTITYget_abstract( entity ) ) {
        if( entity->u.entity->subtype_expression ) {

            fprintf( files->init, "        str.clear();\n        str.append( \"ABSTRACT SUPERTYPE OF ( \" );\n" );

            format_for_std_stringout( files->init, SUBTYPEto_string( entity->u.entity->subtype_expression ) );
            fprintf( files->init, "\n      str.append( \")\" );\n" );
            fprintf( files->init, "        %s::%s%s->AddSupertype_Stmt( str );", schema_name, ENT_PREFIX, entity_name );
        } else {
            fprintf( files->init, "        %s::%s%s->AddSupertype_Stmt( \"ABSTRACT SUPERTYPE\" );\n",
                     schema_name, ENT_PREFIX, entity_name );
        }
    } else {
        if( entity->u.entity->subtype_expression ) {
            fprintf( files->init, "        str.clear();\n        str.append( \"SUPERTYPE OF ( \" );\n" );
            format_for_std_stringout( files->init, SUBTYPEto_string( entity->u.entity->subtype_expression ) );
            fprintf( files->init, "\n      str.append( \")\" );\n" );
            fprintf( files->init, "        %s::%s%s->AddSupertype_Stmt( str );", schema_name, ENT_PREFIX, entity_name );
        }
    }
    LISTdo( ENTITYget_supertypes( entity ), sup, Entity )
    /*  set the owning schema of the supertype  */
    super_schema = SCHEMAget_name( ENTITYget_schema( sup ) );
    /* print the supertype list for this entity */
    fprintf( files->init, "        %s::%s%s->AddSupertype(%s::%s%s);\n",
             schema_name, ENT_PREFIX, entity_name,
             super_schema,
             ENT_PREFIX, ENTITYget_name( sup ) );

    /* add this entity to the subtype list of it's supertype    */
    fprintf( files->init, "        %s::%s%s->AddSubtype(%s::%s%s);\n",
             super_schema,
             ENT_PREFIX, ENTITYget_name( sup ),
             schema_name, ENT_PREFIX, entity_name );
    LISTod

    LISTdo( ENTITYget_attributes( entity ), v, Variable )
    generate_attribute_name( v, attrnm );
    /*  do EXPLICIT and DERIVED attributes first  */
    /*    if  ( ! VARget_inverse (v))  {*/
    /* first make sure that type descriptor exists */
    if( TYPEget_name( v->type ) ) {
        if( ( !TYPEget_head( v->type ) ) &&
                ( TYPEget_body( v->type )->type == entity_ ) ) {
            fprintf( files->init, "        %s::%s%d%s%s =\n          new %s"
                     "(\"%s\",%s::%s%s,\n          %s,%s%s,\n          *%s::%s%s);\n",
                     SCHEMAget_name( schema ), ATTR_PREFIX, attr_count,
                     ( VARis_derived( v ) ? "D" :
                       ( VARis_type_shifter( v ) ? "R" :
                         ( VARget_inverse( v ) ? "I" : "" ) ) ),
                     attrnm,

                     ( VARget_inverse( v ) ? "Inverse_attribute" : ( VARis_derived( v ) ? "Derived_attribute" : "AttrDescriptor" ) ),

                     /* attribute name param */
                     generate_dict_attr_name( v, dict_attrnm ),

                     /* following assumes we are not in a nested */
                     /* entity otherwise we should search upward */
                     /* for schema */
                     /* attribute's type  */
                     TYPEget_name(
                         TYPEget_body( v->type )->entity->superscope ),
                     ENT_PREFIX, TYPEget_name( v->type ),

                     ( VARget_optional( v ) ? "LTrue" : "LFalse" ),

                     ( VARget_unique( v ) ? "LTrue" : "LFalse" ),

                     /* Support REDEFINED */
                     ( VARget_inverse( v ) ? "" :
                       ( VARis_derived( v ) ? ", AttrType_Deriving" :
                         ( VARis_type_shifter( v ) ? ", AttrType_Redefining" : ", AttrType_Explicit" ) ) ),

                     schema_name, ENT_PREFIX, TYPEget_name( entity )
                   );
        } else {
            /* type reference */
            fprintf( files->init, "        %s::%s%d%s%s =\n          new %s"
                     "(\"%s\",%s::%s%s,\n          %s,%s%s,\n          *%s::%s%s);\n",
                     SCHEMAget_name( schema ), ATTR_PREFIX, attr_count,
                     ( VARis_derived( v ) ? "D" :
                       ( VARis_type_shifter( v ) ? "R" :
                         ( VARget_inverse( v ) ? "I" : "" ) ) ),
                     attrnm,

                     ( VARget_inverse( v ) ? "Inverse_attribute" : ( VARis_derived( v ) ? "Derived_attribute" : "AttrDescriptor" ) ),

                     /* attribute name param */
                     generate_dict_attr_name( v, dict_attrnm ),

                     SCHEMAget_name( v->type->superscope ),
                     TD_PREFIX, TYPEget_name( v->type ),

                     ( VARget_optional( v ) ? "LTrue" : "LFalse" ),

                     ( VARget_unique( v ) ? "LTrue" : "LFalse" ),

                     ( VARget_inverse( v ) ? "" :
                       ( VARis_derived( v ) ? ", AttrType_Deriving" :
                         ( VARis_type_shifter( v ) ? ", AttrType_Redefining" : ", AttrType_Explicit" ) ) ),

                     schema_name, ENT_PREFIX, TYPEget_name( entity )
                   );
        }
    } else if( TYPEis_builtin( v->type ) ) {
        /*  the type wasn't named -- it must be built in or aggregate  */

        fprintf( files->init, "        %s::%s%d%s%s =\n          new %s"
                 "(\"%s\",%s%s,\n          %s,%s%s,\n          *%s::%s%s);\n",
                 SCHEMAget_name( schema ), ATTR_PREFIX, attr_count,
                 ( VARis_derived( v ) ? "D" :
                   ( VARis_type_shifter( v ) ? "R" :
                     ( VARget_inverse( v ) ? "I" : "" ) ) ),
                 attrnm,
                 ( VARget_inverse( v ) ? "Inverse_attribute" : ( VARis_derived( v ) ? "Derived_attribute" : "AttrDescriptor" ) ),
                 /* attribute name param */
                 generate_dict_attr_name( v, dict_attrnm ),
                 /* not sure about 0 here */ TD_PREFIX, FundamentalType( v->type, 0 ),
                 ( VARget_optional( v ) ? "LTrue" :
                   "LFalse" ),
                 ( VARget_unique( v ) ? "LTrue" :
                   "LFalse" ),
                 ( VARget_inverse( v ) ? "" :
                   ( VARis_derived( v ) ? ", AttrType_Deriving" :
                     ( VARis_type_shifter( v ) ?
                       ", AttrType_Redefining" :
                       ", AttrType_Explicit" ) ) ),
                 schema_name, ENT_PREFIX, TYPEget_name( entity )
               );
    } else {
        /* manufacture new one(s) on the spot */
        char typename_buf[MAX_LEN];
        print_typechain( files, v->type, typename_buf, schema, v->name->symbol.name );
        fprintf( files->init, "        %s::%s%d%s%s =\n          new %s"
                 "(\"%s\",%s,%s,%s%s,\n          *%s::%s%s);\n",
                 SCHEMAget_name( schema ), ATTR_PREFIX, attr_count,
                 ( VARis_derived( v ) ? "D" :
                   ( VARis_type_shifter( v ) ? "R" :
                     ( VARget_inverse( v ) ? "I" : "" ) ) ),
                 attrnm,
                 ( VARget_inverse( v ) ? "Inverse_attribute" : ( VARis_derived( v ) ? "Derived_attribute" : "AttrDescriptor" ) ),
                 /* attribute name param */
                 generate_dict_attr_name( v, dict_attrnm ),
                 typename_buf,
                 ( VARget_optional( v ) ? "LTrue" :
                   "LFalse" ),
                 ( VARget_unique( v ) ? "LTrue" :
                   "LFalse" ),
                 ( VARget_inverse( v ) ? "" :
                   ( VARis_derived( v ) ? ", AttrType_Deriving" :
                     ( VARis_type_shifter( v ) ?
                       ", AttrType_Redefining" :
                       ", AttrType_Explicit" ) ) ),
                 schema_name, ENT_PREFIX, TYPEget_name( entity )
               );
    }

    fprintf( files->init, "        %s::%s%s->Add%sAttr (%s::%s%d%s%s);\n",
             schema_name, ENT_PREFIX, TYPEget_name( entity ),
             ( VARget_inverse( v ) ? "Inverse" : "Explicit" ),
             SCHEMAget_name( schema ), ATTR_PREFIX, attr_count,
             ( VARis_derived( v ) ? "D" :
               ( VARis_type_shifter( v ) ? "R" :
                 ( VARget_inverse( v ) ? "I" : "" ) ) ),
             attrnm );

    if( VARis_derived( v ) && v->initializer ) {
        tmp = EXPRto_string( v->initializer );
        tmp2 = ( char * )sc_malloc( sizeof( char ) * ( strlen( tmp ) + BUFSIZ ) );
        fprintf( files->init, "        %s::%s%d%s%s->initializer_(\"%s\");\n",
                 schema_name, ATTR_PREFIX, attr_count,
                 ( VARis_derived( v ) ? "D" :
                   ( VARis_type_shifter( v ) ? "R" :
                     ( VARget_inverse( v ) ? "I" : "" ) ) ),
                 attrnm, format_for_stringout( tmp, tmp2 ) );
        sc_free( tmp );
        sc_free( tmp2 );
    }
    if( VARget_inverse( v ) ) {
        fprintf( files->init, "        %s::%s%d%s%s->inverted_attr_id_(\"%s\");\n",
                 schema_name, ATTR_PREFIX, attr_count,
                 ( VARis_derived( v ) ? "D" :
                   ( VARis_type_shifter( v ) ? "R" :
                     ( VARget_inverse( v ) ? "I" : "" ) ) ),
                 attrnm, v->inverse_attribute->name->symbol.name );
        if( v->type->symbol.name ) {
            fprintf( files->init,
                     "        %s::%s%d%s%s->inverted_entity_id_(\"%s\");\n",
                     schema_name, ATTR_PREFIX, attr_count,
                     ( VARis_derived( v ) ? "D" :
                       ( VARis_type_shifter( v ) ? "R" :
                         ( VARget_inverse( v ) ? "I" : "" ) ) ), attrnm,
                     v->type->symbol.name );
            fprintf( files->init, "// inverse entity 1 %s\n", v->type->symbol.name );
        } else {
            switch( TYPEget_body( v->type )->type ) {
                case entity_:
                    fprintf( files->init,
                             "        %s%d%s%s->inverted_entity_id_(\"%s\");\n",
                             ATTR_PREFIX, attr_count,
                             ( VARis_derived( v ) ? "D" :
                               ( VARis_type_shifter( v ) ? "R" :
                                 ( VARget_inverse( v ) ? "I" : "" ) ) ), attrnm,
                             TYPEget_body( v->type )->entity->symbol.name );
                    fprintf( files->init, "// inverse entity 2 %s\n", TYPEget_body( v->type )->entity->symbol.name );
                    break;
                case aggregate_:
                case array_:
                case bag_:
                case set_:
                case list_:
                    fprintf( files->init,
                             "        %s::%s%d%s%s->inverted_entity_id_(\"%s\");\n",
                             schema_name, ATTR_PREFIX, attr_count,
                             ( VARis_derived( v ) ? "D" :
                               ( VARis_type_shifter( v ) ? "R" :
                                 ( VARget_inverse( v ) ? "I" : "" ) ) ), attrnm,
                             TYPEget_body( v->type )->base->symbol.name );
                    fprintf( files->init, "// inverse entity 3 %s\n", TYPEget_body( v->type )->base->symbol.name );
                    break;
            }
        }
    }
    attr_count++;

    LISTod

    fprintf( files->init, "        reg.AddEntity (*%s::%s%s);\n",
             schema_name, ENT_PREFIX, entity_name );

#undef schema_name
}

static bool listContainsVar( Linked_List l, Variable v ) {
    const char * vName = VARget_simple_name( v );
    LISTdo( l, curr, Variable ) {
        if( streq( vName, VARget_simple_name( curr ) ) ) {
            return true;
        }
    }
    LISTod;
    return false;
}

/**************************************************************//**
 ** Procedure:  ENTITYPrint
 ** Parameters:  Entity *entity --  entity being processed
 **     FILE* file  --  file being written to
 ** Returns:
 ** Description:  drives the functions for printing out code in lib,
 **     include, and initialization files for a specific entity class
 ** Side Effects:  generates code in 3 files
 ** Status:  complete 1/15/91
 ******************************************************************/
void ENTITYPrint( Entity entity, FILES * files, Schema schema ) {

    char * n = ENTITYget_name( entity );
    Linked_List remaining = LISTcreate();

    DEBUG( "Entering ENTITYPrint for %s\n", n );

    if( multiple_inheritance ) {
        Linked_List existing = LISTcreate();
        Linked_List required = LISTcreate();

        // create list of attr inherited from the parents in C++
        collectAttributes( existing, entity, FIRST_ONLY );

        // create list of attr that have to be inherited in EXPRESS
        collectAttributes( required, entity, ALL_BUT_FIRST );

        // build list of unique attr that are required but havn't been
        // inherited
        LISTdo( required, attr, Variable ) {
            if( !listContainsVar( existing, attr ) &&
                    !listContainsVar( remaining, attr ) ) {
                LISTadd_first( remaining, ( Generic ) attr );
            }
        }
        LISTod;
        LIST_destroy( existing );
        LIST_destroy( required );
    }

    fprintf( files->inc,   "\n/////////         ENTITY %s\n", n );
    ENTITYinc_print( entity, remaining, files -> inc, schema );
    fprintf( files->inc,     "/////////         END_ENTITY %s\n", n );

    fprintf( files->names, "\n/////////         ENTITY %s\n", n );
    ENTITYnames_print( entity, files -> names, schema );
    fprintf( files->names,   "/////////         END_ENTITY %s\n", n );

    fprintf( files->lib,   "\n/////////         ENTITY %s\n", n );
    ENTITYlib_print( entity, remaining, files -> lib, schema );
    fprintf( files->lib,     "/////////         END_ENTITY %s\n", n );

    fprintf( files->init,  "\n/////////         ENTITY %s\n", n );
    ENTITYincode_print( entity, files , schema );
    fprintf( files->init,    "/////////         END_ENTITY %s\n", n );

    DEBUG( "DONE ENTITYPrint\n" )    ;
    LIST_destroy( remaining );
}

void MODELPrintConstructorBody( Entity entity, FILES * files, Schema schema ) {
    const char * n;
    DEBUG( "Entering MODELPrintConstructorBody for %s\n", n );

    n = ENTITYget_classname( entity );

    fprintf( files->lib, "    eep = new SDAI_Entity_extent;\n" );

    fprintf( files->lib, "    eep->definition_(%s::%s%s);\n",
             SCHEMAget_name( schema ), ENT_PREFIX, ENTITYget_name( entity ) );
    fprintf( files->lib, "    _folders.Append(eep);\n\n" );
}

void MODELPrint( Entity entity, FILES * files, Schema schema, int index ) {

    const char * n;
    DEBUG( "Entering MODELPrint for %s\n", n );

    n = ENTITYget_classname( entity );
    fprintf( files->lib, "\n%s__set_var SdaiModel_contents_%s::%s_get_extents()\n",
             n, SCHEMAget_name( schema ), n );
    fprintf( files->lib,
             "{\n    return (%s__set_var)((_folders.retrieve(%d))->instances_());\n}\n",
             n, index );
    DEBUG( "DONE MODELPrint\n" )    ;
}

/** print in include file: class forward prototype, class typedefs, and
 *   extern EntityDescriptor.  `externMap' = 1 if entity must be instantiated
 *   with external mapping (see Part 21, sect 11.2.5.1).
 *  Nov 2011 - MAP - print EntityDescriptor in namespace file, modify other
 *   generated code to use namespace
 */
void ENTITYprint_new( Entity entity, FILES * files, Schema schema, int externMap ) {
    const char * n;
    Linked_List wheres;
    char * whereRule, *whereRule_formatted = NULL;
    size_t whereRule_formatted_size = 0;
    char * ptr, *ptr2;
    char * uniqRule, *uniqRule_formatted;
    Linked_List uniqs;

    fprintf( files->create, "    %s::%s%s = new EntityDescriptor(\n        ",
             SCHEMAget_name( schema ), ENT_PREFIX, ENTITYget_name( entity ) );
    fprintf( files->create, "  \"%s\", %s::schema, %s, ",
             PrettyTmpName( ENTITYget_name( entity ) ),
             SCHEMAget_name( schema ), ( ENTITYget_abstract( entity ) ? "LTrue" : "LFalse" ) );
    fprintf( files->create, "%s,\n          ", externMap ? "LTrue" :
             "LFalse" );

    fprintf( files->create, "  (Creator) create_%s );\n",
             ENTITYget_classname( entity ) );
    /* add the entity to the Schema dictionary entry */
    fprintf( files->create, "    %s::schema->AddEntity(%s::%s%s);\n", SCHEMAget_name( schema ), SCHEMAget_name( schema ), ENT_PREFIX, ENTITYget_name( entity ) );

    wheres = TYPEget_where( entity );

    if( wheres ) {
        fprintf( files->create,
                 "    %s::%s%s->_where_rules = new Where_rule__list;\n",
                 SCHEMAget_name( schema ), ENT_PREFIX, ENTITYget_name( entity ) );

        LISTdo( wheres, w, Where )
        whereRule = EXPRto_string( w->expr );
        ptr2 = whereRule;

        if( whereRule_formatted_size == 0 ) {
            whereRule_formatted_size = 3 * BUFSIZ;
            whereRule_formatted = ( char * )sc_malloc( sizeof( char ) * whereRule_formatted_size );
        } else if( ( strlen( whereRule ) + 300 ) > whereRule_formatted_size ) {
            sc_free( whereRule_formatted );
            whereRule_formatted_size = strlen( whereRule ) + BUFSIZ;
            whereRule_formatted = ( char * )sc_malloc( sizeof( char ) * whereRule_formatted_size );
        }
        whereRule_formatted[0] = '\0';
        if( w->label ) {
            strcpy( whereRule_formatted, w->label->name );
            strcat( whereRule_formatted, ": (" );
            ptr = whereRule_formatted + strlen( whereRule_formatted );
            while( *ptr2 ) {
                if( *ptr2 == '\n' ) {
                    ;
                } else if( *ptr2 == '\\' ) {
                    *ptr = '\\';
                    ptr++;
                    *ptr = '\\';
                    ptr++;

                } else if( *ptr2 == '(' ) {
                    *ptr = '\\';
                    ptr++;
                    *ptr = 'n';
                    ptr++;
                    *ptr = '\\';
                    ptr++;
                    *ptr = 't';
                    ptr++;
                    *ptr = *ptr2;
                    ptr++;
                } else {
                    *ptr = *ptr2;
                    ptr++;
                }
                ptr2++;
            }
            *ptr = '\0';

            strcat( ptr, ");\\n" );
        } else {
            /* no label */
            strcpy( whereRule_formatted, "(" );
            ptr = whereRule_formatted + strlen( whereRule_formatted );

            while( *ptr2 ) {
                if( *ptr2 == '\n' ) {
                    ;
                } else if( *ptr2 == '\\' ) {
                    *ptr = '\\';
                    ptr++;
                    *ptr = '\\';
                    ptr++;

                } else if( *ptr2 == '(' ) {
                    *ptr = '\\';
                    ptr++;
                    *ptr = 'n';
                    ptr++;
                    *ptr = '\\';
                    ptr++;
                    *ptr = 't';
                    ptr++;
                    *ptr = *ptr2;
                    ptr++;
                } else {
                    *ptr = *ptr2;
                    ptr++;
                }
                ptr2++;
            }
            *ptr = '\0';
            strcat( ptr, ");\\n" );
        }
        fprintf( files->create, "        wr = new Where_rule(\"%s\");\n", whereRule_formatted );
        fprintf( files->create, "        %s::%s%s->_where_rules->Append(wr);\n",
                 SCHEMAget_name( schema ), ENT_PREFIX, ENTITYget_name( entity ) );

        sc_free( whereRule );
        ptr2 = whereRule = 0;
        LISTod
    }

    uniqs = entity->u.entity->unique;

    if( uniqs ) {
        int i;
        fprintf( files->create,
                 "        %s::%s%s->_uniqueness_rules = new Uniqueness_rule__set;\n",
                 SCHEMAget_name( schema ), ENT_PREFIX, ENTITYget_name( entity ) );

        if( whereRule_formatted_size == 0 ) {
            uniqRule_formatted = ( char * )sc_malloc( sizeof( char ) * 2 * BUFSIZ );
            whereRule_formatted = uniqRule_formatted;
        } else {
            uniqRule_formatted = whereRule_formatted;
        }

        /*
        FIXME DASBUG
         * take care of qualified attribute names like SELF\entity.attrname
         * add parent entity to the uniqueness rule
         * change EntityDescriptor::generate_express() to generate the UNIQUE clause
        */
        LISTdo( uniqs, list, Linked_List )
        i = 0;
        fprintf( files->create, "        ur = new Uniqueness_rule(\"" );
        LISTdo( list, v, Variable )
        i++;
        if( i == 1 ) {
            /* print label if present */
            if( v ) {
                fprintf( files->create, "%s : ", StrToUpper( ( ( Symbol * )v )->name ) );
            }
        } else {
            if( i > 2 ) {
                fprintf( files->create, ", " );
            }
            uniqRule = EXPRto_string( v->name );
            fprintf( files->create, "%s", uniqRule );
            sc_free( uniqRule );
        }
        LISTod
        fprintf( files->create, ";\\n\");\n" );
        fprintf( files->create, "        %s::%s%s->_uniqueness_rules->Append(ur);\n",
                 SCHEMAget_name( schema ), ENT_PREFIX, ENTITYget_name( entity ) );
        LISTod
    }

    if( whereRule_formatted_size > 0 ) {
        sc_free( whereRule_formatted );
    }

    n = ENTITYget_classname( entity );
    fprintf( files->classes, "\nclass %s;\n", n );
    fprintf( files->classes, "typedef %s *          %sH;\n", n, n );
    fprintf( files->classes, "typedef %s *          %s_ptr;\n", n, n );
    fprintf( files->classes, "typedef %s_ptr        %s_var;\n", n, n );
    fprintf( files->classes, "#define %s__set         SDAI_DAObject__set\n", n );
    fprintf( files->classes, "#define %s__set_var     SDAI_DAObject__set_var\n", n );
}

void MODELprint_new( Entity entity, FILES * files, Schema schema ) {
    const char * n;

    n = ENTITYget_classname( entity );
    fprintf( files->inc, "\n    %s__set_var %s_get_extents();\n", n, n );
}

/******************************************************************
 **         TYPE GENERATION             **/


/******************************************************************
 ** Procedure:  TYPEprint_enum
 ** Parameters: const Type type - type to print
 **     FILE*      f    - file on which to print
 ** Returns:
 ** Requires:   TYPEget_class(type) == TYPE_ENUM
 ** Description:  prints code to represent an enumerated type in c++
 ** Side Effects:  prints to header file
 ** Status:  ok 1/15/91
 ** Changes: Modified to check for appropiate key words as described
 **          in "SDAI C++ Binding for PDES, Inc. Prototyping" by
 **          Stephen Clark.
 ** - Changed to match CD2 Part 23, 1/14/97 DAS
 ** Change Date: 5/22/91  CD
 ******************************************************************/
const char * EnumCElementName( Type type, Expression expr )  {

    static char buf [BUFSIZ];
    sprintf( buf, "%s__",
             EnumName( TYPEget_name( type ) ) );
    strcat( buf, StrToLower( EXPget_name( expr ) ) );

    return buf;
}

char * CheckEnumSymbol( char * s ) {

    static char b [BUFSIZ];
    if( strcmp( s, "sdaiTRUE" )
            && strcmp( s, "sdaiFALSE" )
            && strcmp( s, "sdaiUNKNOWN" ) ) {
        /*  if the symbol is not a reserved one */
        return ( s );

    } else {
        strcpy( b, s );
        strcat( b, "_" );
        printf( "** warning:  the enumerated value %s is already being used ", s );
        printf( " and has been changed to %s **\n", b );
        return ( b );
    }
}

/**************************************************************//**
 ** Procedure:   TYPEenum_inc_print
 ** Description: Writes enum type descriptors and classes.
 ** Change Date:
 ********************************************************************/
void TYPEenum_inc_print( const Type type, FILE * inc ) {
    Expression expr;

    char tdnm[BUFSIZ],
         enumAggrNm[BUFSIZ];
    const char * n;  /*  pointer to class name  */
    int cnt = 0;

    fprintf( inc, "\n//////////  ENUMERATION TYPE %s\n",
             TYPEget_name( type ) );

    /* print c++ enumerated values for class   */
    fprintf( inc, "enum %s {\n", EnumName( TYPEget_name( type ) ) );

    LISTdo_links( TYPEget_body( type )->list, link )
    /* print the elements of the c++ enum type  */
    expr = ( Expression )link->data;
    if( cnt != 0 ) {
        fprintf( inc, ",\n" );
    }
    ++cnt;
    fprintf( inc, "        %s", EnumCElementName( type, expr ) );

    LISTod

    fprintf( inc, ",\n        %s_unset\n};\n", EnumName( TYPEget_name( type ) ) );

    /* print class for enumeration */
    n = TYPEget_ctype( type );
    fprintf( inc, "\nclass %s  :  public SDAI_Enum  {\n", n );

    fprintf( inc, "  protected:\n        EnumTypeDescriptor *type;\n\n" );

    /* constructors */
    strncpy( tdnm, TYPEtd_name( type ), BUFSIZ );
    tdnm[BUFSIZ-1] = '\0';
    fprintf( inc, "  public:\n        %s (const char * n =0, Enum"
             "TypeDescriptor *et =%s);\n", n, tdnm );
    fprintf( inc, "        %s (%s e, EnumTypeDescriptor *et =%s)\n"
             "                : type(et) {  set_value (e);  }\n",
             n, EnumName( TYPEget_name( type ) ), tdnm );

    /* destructor */
    fprintf( inc, "        ~%s () { }\n", n );

    /* copy constructor */
    fprintf( inc, "        %s(const %s& e)\n",
             n, TYPEget_ctype( type ) );
    fprintf( inc, "                {  set_value (e); }\n" );

    /* operator = */
    fprintf( inc, "        %s& operator= (const %s& e)\n",
             n, TYPEget_ctype( type ) );
    fprintf( inc, "                {  set_value (e);  return *this;  }\n" );

    /* operator to cast to an enumerated type */
    fprintf( inc, "        operator %s () const;\n",
             EnumName( TYPEget_name( type ) ) );

    /* others */
    fprintf( inc, "\n        inline virtual const char * Name () const\n" );
    fprintf( inc, "                {  return type->Name();  }\n" );
    fprintf( inc, "        inline virtual int no_elements () const"
             "  {  return %d;  }\n", cnt );
    fprintf( inc, "        virtual const char * element_at (int n) const;\n" );

    /* end class definition */
    fprintf( inc, "};\n" );

    fprintf( inc, "\ntypedef %s * %s_ptr;\n", n, n );


    /* Print ObjectStore Access Hook function */
    printEnumCreateHdr( inc, type );

    /* DAS brandnew above */

    /* print things for aggregate class */
    sprintf( enumAggrNm, "%s_agg", n );

    fprintf( inc, "\nclass %s_agg  :  public EnumAggregate  {\n", n );

    fprintf( inc, "  protected:\n    EnumTypeDescriptor *enum_type;\n\n" );
    fprintf( inc, "  public:\n" );
    fprintf( inc, "    %s_agg( EnumTypeDescriptor * =%s);\n", n, tdnm );
    fprintf( inc, "    virtual ~%s_agg();\n", n );
    fprintf( inc, "    virtual SingleLinkNode * NewNode()\n" );
    fprintf( inc, "        { return new EnumNode (new %s( \"\", enum_type )); }"
             "\n", n );

    fprintf( inc, "};\n" );

    fprintf( inc, "\ntypedef %s_agg * %s_agg_ptr;\n", n, n );

    /* DAS brandnew below */

    /* DAS creation function for enum aggregate class */
    printEnumAggrCrHdr( inc, type );

    /* DAS brandnew above */

    fprintf( inc, "\n//////////  END ENUMERATION %s\n\n", TYPEget_name( type ) );
}

void TYPEenum_lib_print( const Type type, FILE * f ) {
    DictionaryEntry de;
    Expression expr;
    const char * n;   /*  pointer to class name  */
    char c_enum_ele [BUFSIZ];

    fprintf( f, "//////////  ENUMERATION TYPE %s\n", TYPEget_ctype( type ) );
    n = TYPEget_ctype( type );

    /*  set up the dictionary info  */

    fprintf( f, "const char *\n%s::element_at (int n) const  {\n", n );
    fprintf( f, "  switch (n)  {\n" );
    DICTdo_type_init( ENUM_TYPEget_items( type ), &de, OBJ_ENUM );
    while( 0 != ( expr = ( Expression )DICTdo( &de ) ) ) {
        strncpy( c_enum_ele, EnumCElementName( type, expr ), BUFSIZ );
	c_enum_ele[BUFSIZ-1] = '\0';
        fprintf( f, "  case %s:  return \"%s\";\n",
                 c_enum_ele,
                 StrToUpper( EXPget_name( expr ) ) );
    }
    fprintf( f, "  case %s_unset        :\n", EnumName( TYPEget_name( type ) ) );
    fprintf( f, "  default                :  return \"UNSET\";\n  }\n}\n" );

    /*    constructors    */
    /*    construct with character string  */
    fprintf( f, "\n%s::%s (const char * n, EnumTypeDescriptor *et)\n"
             "  : type(et)\n{\n", n, n );
    fprintf( f, "  set_value (n);\n}\n" );

    /*      cast operator to an enumerated type  */
    fprintf( f, "\n%s::operator %s () const {\n", n,
             EnumName( TYPEget_name( type ) ) );
    fprintf( f, "  switch (v) {\n" );
    DICTdo_type_init( ENUM_TYPEget_items( type ), &de, OBJ_ENUM );
    while( 0 != ( expr = ( Expression )DICTdo( &de ) ) ) {
        strncpy( c_enum_ele, EnumCElementName( type, expr ), BUFSIZ );
        fprintf( f, "        case %s        :  ", c_enum_ele );
        fprintf( f, "return %s;\n", c_enum_ele );
    }
    /*  print the last case with the default so sun c++ doesn't complain */
    fprintf( f, "        case %s_unset        :\n", EnumName( TYPEget_name( type ) ) );
    fprintf( f, "        default                :  return %s_unset;\n  }\n}\n", EnumName( TYPEget_name( type ) ) );

    printEnumCreateBody( f, type );

    /* print the enum aggregate functions */

    fprintf( f, "\n%s_agg::%s_agg( EnumTypeDescriptor *et )\n", n, n );
    fprintf( f, "    : enum_type(et)\n{\n}\n\n" );
    fprintf( f, "%s_agg::~%s_agg()\n{\n}\n", n, n );

    printEnumAggrCrBody( f, type );

    fprintf( f, "\n//////////  END ENUMERATION %s\n\n", TYPEget_name( type ) );
}


void Type_Description( const Type, char * );

/**
 * return printable version of entire type definition
 * return it in static buffer
 */
char * TypeDescription( const Type t ) {
    static char buf[6000];

    buf[0] = '\0';

    if( TYPEget_head( t ) ) {
        Type_Description( TYPEget_head( t ), buf );
    } else {
        TypeBody_Description( TYPEget_body( t ), buf );
    }

    /* should also print out where clause here */

    return buf + 1;
}

void strcat_expr( Expression e, char * buf ) {
    if( e == LITERAL_INFINITY ) {
        strcat( buf, "?" );
    } else if( e == LITERAL_PI ) {
        strcat( buf, "PI" );
    } else if( e == LITERAL_E ) {
        strcat( buf, "E" );
    } else if( e == LITERAL_ZERO ) {
        strcat( buf, "0" );
    } else if( e == LITERAL_ONE ) {
        strcat( buf, "1" );
    } else if( TYPEget_name( e ) ) {
        strcat( buf, TYPEget_name( e ) );
    } else if( TYPEget_body( e->type )->type == integer_ ) {
        char tmpbuf[30];
        sprintf( tmpbuf, "%d", e->u.integer );
        strcat( buf, tmpbuf );
    } else {
        strcat( buf, "??" );
    }
}

/// print t's bounds to end of buf
void strcat_bounds( TypeBody b, char * buf ) {
    if( !b->upper ) {
        return;
    }

    strcat( buf, " [" );
    strcat_expr( b->lower, buf );
    strcat( buf, ":" );
    strcat_expr( b->upper, buf );
    strcat( buf, "]" );
}

void TypeBody_Description( TypeBody body, char * buf ) {
    char * s;

    switch( body->type ) {
        case integer_:
            strcat( buf, " INTEGER" );
            break;
        case real_:
            strcat( buf, " REAL" );
            break;
        case string_:
            strcat( buf, " STRING" );
            break;
        case binary_:
            strcat( buf, " BINARY" );
            break;
        case boolean_:
            strcat( buf, " BOOLEAN" );
            break;
        case logical_:
            strcat( buf, " LOGICAL" );
            break;
        case number_:
            strcat( buf, " NUMBER" );
            break;
        case entity_:
            strcat( buf, " " );
            strcat( buf, PrettyTmpName( TYPEget_name( body->entity ) ) );
            break;
        case aggregate_:
        case array_:
        case bag_:
        case set_:
        case list_:
            switch( body->type ) {
                    /* ignore the aggregate bounds for now */
                case aggregate_:
                    strcat( buf, " AGGREGATE OF" );
                    break;
                case array_:
                    strcat( buf, " ARRAY" );
                    strcat_bounds( body, buf );
                    strcat( buf, " OF" );
                    if( body->flags.optional ) {
                        strcat( buf, " OPTIONAL" );
                    }
                    if( body->flags.unique ) {
                        strcat( buf, " UNIQUE" );
                    }
                    break;
                case bag_:
                    strcat( buf, " BAG" );
                    strcat_bounds( body, buf );
                    strcat( buf, " OF" );
                    break;
                case set_:
                    strcat( buf, " SET" );
                    strcat_bounds( body, buf );
                    strcat( buf, " OF" );
                    break;
                case list_:
                    strcat( buf, " LIST" );
                    strcat_bounds( body, buf );
                    strcat( buf, " OF" );
                    if( body->flags.unique ) {
                        strcat( buf, " UNIQUE" );
                    }
                    break;
            }

            Type_Description( body->base, buf );
            break;
        case enumeration_:
            strcat( buf, " ENUMERATION of (" );
            LISTdo( body->list, e, Expression )
            strcat( buf, ENUMget_name( e ) );
            strcat( buf, ", " );
            LISTod
            /* find last comma and replace with ')' */
            s = strrchr( buf, ',' );
            if( s ) {
                strcpy( s, ")" );
            }
            break;

        case select_:
            strcat( buf, " SELECT (" );
            LISTdo( body->list, t, Type )
            strcat( buf, PrettyTmpName( TYPEget_name( t ) ) );
            strcat( buf, ", " );
            LISTod
            /* find last comma and replace with ')' */
            s = strrchr( buf, ',' );
            if( s ) {
                strcpy( s, ")" );
            }
            break;
        default:
            strcat( buf, " UNKNOWN" );
    }

    if( body->precision ) {
        strcat( buf, " (" );
        strcat_expr( body->precision, buf );
        strcat( buf, ")" );
    }
    if( body->flags.fixed ) {
        strcat( buf, " FIXED" );
    }
}

void Type_Description( const Type t, char * buf ) {
    if( TYPEget_name( t ) ) {
        strcat( buf, " " );
        strcat( buf, TYPEget_name( t ) );
    } else {
        TypeBody_Description( TYPEget_body( t ), buf );
    }
}

/** ************************************************************************
 ** Procedure:  TYPEprint_typedefs
 ** Parameters:  const Type type
 ** Returns:
 ** Description:
 **    Prints in Sdaiclasses.h typedefs, forward declarations, and externs
 **    for user-defined types.  Only a fraction of the typedefs and decla-
 **    rations are needed in Sdaiclasses.h.  Enum's and selects must actu-
 **    ally be defined before objects (such as entities) which use it can
 **    be defined.  So forward declarations will not serve any purpose.
 **    Other redefined types and aggregate types may be declared here.
 ** Side Effects:
 ** Status:  16-Mar-1993 kcm; updated 04-Feb-1997 dar
 ** Dec 2011 - MAP - remove goto
 **************************************************************************/
void TYPEprint_typedefs( Type t, FILE * classes ) {
    char nm [BUFSIZ];
    Type i;
    bool aggrNot1d = true;  //added so I can get rid of a goto

    /* Print the typedef statement (poss also a forward class def: */
    if( TYPEis_enumeration( t ) ) {
        /* For enums and sels (else clause below), we need forward decl's so
        // that if we later come across a type which is an aggregate of one of
        // them, we'll be able to process it.  For selects, we also need a decl
        // of the class itself, while for enum's we don't.  Objects which con-
        // tain an enum can't be generated until the enum is generated.  (The
        // same is basically true for the select, but a sel containing an ent
        // containing a sel needs the forward decl (trust me ;-) ). */
        if( !TYPEget_head( t ) ) {
            /* Only print this enum if it is an actual type and not a redefi-
            // nition of another enum.  (Those are printed at the end of the
            // classes file - after all the actual enum's.  They must be
            // printed last since they depend on the others.) */
            strncpy( nm, TYPEget_ctype( t ), BUFSIZ );
	    nm[BUFSIZ-1] = '\0';
            fprintf( classes, "class %s_agg;\n", nm );
        }
    } else if( TYPEis_select( t ) ) {
        if( !TYPEget_head( t ) ) {
            /* Same comment as above. */
            strncpy( nm, SelectName( TYPEget_name( t ) ), BUFSIZ );
	    nm[BUFSIZ-1] = '\0';
            fprintf( classes, "class %s;\n", nm );
            fprintf( classes, "typedef %s * %s_ptr;\n", nm, nm );
            fprintf( classes, "class %s_agg;\n", nm );
            fprintf( classes, "typedef %s_agg * %s_agg_ptr;\n", nm, nm );
        }
    } else {
        if( TYPEis_aggregate( t ) ) {
            i = TYPEget_base_type( t );
            if( TYPEis_enumeration( i ) || TYPEis_select( i ) ) {
                /* One exceptional case - a 1d aggregate of an enum or select.
                // We must wait till the enum/sel itself has been processed.
                // To ensure this, we process all such 1d aggrs in a special
                // loop at the end (in multpass.c).  2d aggrs (or higher), how-
                // ever, can be processed now - they only require GenericAggr
                // for their definition here. */
                aggrNot1d = false;
            }
        }
        if( aggrNot1d ) {
            /* At this point, we'll print typedefs for types which are redefined
            // fundamental types and their aggregates, and for 2D aggregates(aggre-
            // gates of aggregates) of enum's and selects. */
            strncpy( nm, ClassName( TYPEget_name( t ) ), BUFSIZ );
	    nm[BUFSIZ-1] = '\0';
            fprintf( classes, "typedef %s         %s;\n", TYPEget_ctype( t ), nm );
            if( TYPEis_aggregate( t ) ) {
                fprintf( classes, "typedef %s *         %sH;\n", nm, nm );
                fprintf( classes, "typedef %s *         %s_ptr;\n", nm, nm );
                fprintf( classes, "typedef %s_ptr         %s_var;\n", nm, nm );
            }
        }
    }

    /* Print the extern statement: */
#if !defined(__BORLAND__)
    strncpy( nm, TYPEtd_name( t ), BUFSIZ );
    fprintf( classes, "extern %s         *%s;\n", GetTypeDescriptorName( t ), nm );
#endif
}

/** return 1 if it is a multidimensional aggregate at the level passed in
   otherwise return 0;  If it refers to a type that is a multidimensional
   aggregate 0 is still returned. */
int isMultiDimAggregateType( const Type t ) {
    if( TYPEget_body( t )->base )
        if( isAggregateType( TYPEget_body( t )->base ) ) {
            return 1;
        }
    return 0;
}

/** Get the TypeDescriptor variable name that t's TypeDescriptor references (if
   possible).
   pass space in through buf, buff will be filled in with the name of the
   TypeDescriptor (TD) that needs to be referenced by the TD that is
   generated for Type t.  Return 1 if buf has been filled in with the name
   of a TD.  Return 0 if it hasn't for these reasons: Enumeration TDs don't
   reference another TD, select TDs reference several TDs and are handled
   separately, Multidimensional aggregates generate unique intermediate TD
   variables that are referenced - when these don't have an Express related
   name this function can't know about them - e.g.
   TYPE listSetAggr = LIST OF SET OF STRING;  This function won't fill in the
   name that listSetAggr's ListTypeDescriptor will reference.
   TYPE arrListSetAggr = ARRAY [1:4] of listSetAggr;  This function will
   return the name of the TD that arrlistSetAggr's ArrayTypeDescriptor should
   reference since it has an Express name associated with it.
   *
   Nov 2011 - MAP - modified to insert scope operator into variable name.
   Reason: use of namespace for global variables
*/
int TYPEget_RefTypeVarNm( const Type t, char * buf, Schema schema ) {

    /* It looks like TYPEget_head(t) is true when processing a type
       that refers to another type. e.g. when processing "name" in:
       TYPE name = label; ENDTYPE; TYPE label = STRING; ENDTYPE; DAS */
    if( TYPEget_head( t ) ) {
        /* this means that it is defined in an Express TYPE stmt and
           it refers to another Express TYPE stmt */
        /*  it would be a reference_ type */
        /*  a TypeDescriptor of the form <schema_name>t_<type_name_referred_to> */
        sprintf( buf, "%s::%s%s",
                 SCHEMAget_name( TYPEget_head( t )->superscope ),
                 TYPEprefix( t ), TYPEget_name( TYPEget_head( t ) ) );
        return 1;
    } else {
        switch( TYPEget_body( t )->type ) {
            case integer_:
            case real_:
            case boolean_:
            case logical_:
            case string_:
            case binary_:
            case number_:
                /* one of the SCL builtin TypeDescriptors of the form
                   t_STRING_TYPE, or t_REAL_TYPE */
                sprintf( buf, "%s%s", TD_PREFIX, FundamentalType( t, 0 ) );
                return 1;
                break;

            case enumeration_: /* enums don't have a referent type */
            case select_:  /* selects are handled differently elsewhere, they
                refer to several TypeDescriptors */
                return 0;
                break;

            case entity_:
                sprintf( buf, "%s", TYPEtd_name( t ) );
                /* following assumes we are not in a nested entity */
                /* otherwise we should search upward for schema */
                return 1;
                break;

            case aggregate_:
            case array_:
            case bag_:
            case set_:
            case list_:
                /* referent TypeDescriptor will be the one for the element unless it
                   is a multidimensional aggregate then return 0 */

                if( isMultiDimAggregateType( t ) ) {
                    if( TYPEget_name( TYPEget_body( t )->base ) ) {
                        sprintf( buf, "%s::%s%s",
                                 SCHEMAget_name( TYPEget_body( t )->base->superscope ),
                                 TYPEprefix( t ), TYPEget_name( TYPEget_body( t )->base ) );
                        return 1;
                    }

                    /* if the multi aggr doesn't have a name then we are out of scope
                       of what this function can do */
                    return 0;
                } else {
                    /* for a single dimensional aggregate return TypeDescriptor
                       for element */
                    /* being an aggregate implies that base below is not 0 */

                    if( TYPEget_body( TYPEget_body( t )->base )->type == enumeration_ ||
                            TYPEget_body( TYPEget_body( t )->base )->type == select_ ) {

                        sprintf( buf, "%s", TYPEtd_name( TYPEget_body( t )->base ) );
                        return 1;
                    } else if( TYPEget_name( TYPEget_body( t )->base ) ) {
                        if( TYPEget_body( TYPEget_body( t )->base )->type == entity_ ) {
                            sprintf( buf, "%s", TYPEtd_name( TYPEget_body( t )->base ) );
                            return 1;
                        }
                        sprintf( buf, "%s::%s%s",
                                 SCHEMAget_name( TYPEget_body( t )->base->superscope ),
                                 TYPEprefix( t ), TYPEget_name( TYPEget_body( t )->base ) );
                        return 1;
                    }
                    return TYPEget_RefTypeVarNm( TYPEget_body( t )->base, buf, schema );
                }
                break;
            default:
                return 0;
        }
    }
    /* NOTREACHED */
    return 0;
}


/** **
   print stuff for types that are declared in Express TYPE statements... i.e.
   extern descriptor declaration in .h file - MOVED BY DAR to TYPEprint_type-
       defs - in order to print all the Sdaiclasses.h stuff in exp2cxx's
       first pass through each schema.
   descriptor definition in the .cc file
   initialize it in the .init.cc file (DAR - all initialization done in fn
       TYPEprint_init() (below) which is done in exp2cxx's 1st pass only.)
*****/
void TYPEprint_descriptions( const Type type, FILES * files, Schema schema ) {
    char tdnm [BUFSIZ],
         typename_buf [MAX_LEN],
         base [BUFSIZ],
         nm [BUFSIZ];
    Type i;

    strncpy( tdnm, TYPEtd_name( type ), BUFSIZ );
    tdnm[BUFSIZ-1] = '\0';

    /* define type descriptor pointer */
    /*  in source - declare the real definition of the pointer */
    /*  i.e. in the .cc file                                   */
    fprintf( files -> lib, "%s         *%s;\n", GetTypeDescriptorName( type ), tdnm );

    if( isAggregateType( type ) ) {
        const char * ctype = TYPEget_ctype( type );

        fprintf( files->inc, "STEPaggregate * create_%s ();\n\n",
                 ClassName( TYPEget_name( type ) ) );

        fprintf( files->lib,
                 "STEPaggregate *\ncreate_%s () {  return create_%s();  }\n",
                 ClassName( TYPEget_name( type ) ), ctype );

        /* this function is assigned to the aggrCreator var in TYPEprint_new */
    }

    if( TYPEis_enumeration( type ) && ( i = TYPEget_ancestor( type ) ) != NULL ) {
        /* If we're a renamed enum type, just print a few typedef's to the
         * original and some specialized create functions:
         */
        strncpy( base, EnumName( TYPEget_name( i ) ), BUFSIZ );
        strncpy( nm, EnumName( TYPEget_name( type ) ), BUFSIZ );
        fprintf( files->inc, "typedef %s %s;\n", base, nm );
        strncpy( base, TYPEget_ctype( i ), BUFSIZ );
        strncpy( nm, TYPEget_ctype( type ), BUFSIZ );
        fprintf( files->inc, "typedef %s %s;\n", base, nm );
        printEnumCreateHdr( files->inc, type );
        printEnumCreateBody( files->lib, type );
        fprintf( files->inc, "typedef       %s_agg *       %s_agg_ptr;\n", nm, nm );
        printEnumAggrCrHdr( files->inc, type );
        printEnumAggrCrBody( files->lib, type );
        return;
    }

    if( !TYPEget_RefTypeVarNm( type, typename_buf, schema ) ) {
        switch( TYPEget_body( type )->type ) {
            case enumeration_:
                TYPEenum_inc_print( type, files -> inc );
                TYPEenum_lib_print( type, files -> lib );
                break;

            case select_:
                /*  the select definitions are done seperately, since they depend
                    on the others  */
                /*******
                  TYPEselect_inc_print (type, files -> inc);
                  TYPEselect_lib_print (type, files -> lib);
                  *******/
                break;
            default:
                break;
        }
    }
}

/**
 * Prints a bunch of lines for enumeration creation functions (i.e., "cre-
 * ate_SdaiEnum1()").  Since this is done both for an enum and for "copies"
 * of it (when "TYPE enum2 = enum1"), I placed this code in a separate fn.
 */
static void printEnumCreateHdr( FILE * inc, const Type type ) {
    const char * nm = TYPEget_ctype( type );

    fprintf( inc, "  SDAI_Enum * create_%s ();\n", nm );
}

/// See header comment above by printEnumCreateHdr.
static void printEnumCreateBody( FILE * lib, const Type type ) {
    const char * nm = TYPEget_ctype( type );
    char tdnm[BUFSIZ];
    tdnm[BUFSIZ-1] = '\0';

    strncpy( tdnm, TYPEtd_name( type ), BUFSIZ );
    tdnm[BUFSIZ-1] = '\0';

    fprintf( lib, "\nSDAI_Enum *\ncreate_%s ()\n{\n", nm );
    fprintf( lib, "    return new %s( \"\", %s );\n}\n\n", nm, tdnm );
}

/// Similar to printEnumCreateHdr above for the enum aggregate.
static void printEnumAggrCrHdr( FILE * inc, const Type type ) {
    const char * n = TYPEget_ctype( type );
    /*    const char *n = ClassName( TYPEget_name(type) ));*/
    fprintf( inc, "  STEPaggregate * create_%s_agg ();\n", n );
}

static void printEnumAggrCrBody( FILE * lib, const Type type ) {
    const char * n = TYPEget_ctype( type );
    char tdnm[BUFSIZ];

    strncpy( tdnm, TYPEtd_name( type ), BUFSIZ );
    tdnm[BUFSIZ-1] = '\0';

    fprintf( lib, "\nSTEPaggregate *\ncreate_%s_agg ()\n{\n", n );
    fprintf( lib, "    return new %s_agg( %s );\n}\n", n, tdnm );
}

void TYPEprint_init( const Type type, FILES * files, Schema schema ) {
    char tdnm [BUFSIZ];
    char typename_buf[MAX_LEN];

    strncpy( tdnm, TYPEtd_name( type ), BUFSIZ );

    if( isAggregateType( type ) ) {
        AGGRprint_init( files, type, tdnm, type->symbol.name );
    }

    /* fill in the TD's values in the SchemaInit function (it is already
    declared with basic values) */

    if( TYPEget_RefTypeVarNm( type, typename_buf, schema ) ) {
        fprintf( files->init, "        %s->ReferentType(%s);\n", tdnm, typename_buf );
    } else {
        switch( TYPEget_body( type )->type ) {
            case aggregate_: /* aggregate_ should not happen? DAS */
            case array_:
            case bag_:
            case set_:
            case list_: {
                if( isMultiDimAggregateType( type ) ) {
                    print_typechain( files, TYPEget_body( type )->base,
                                     typename_buf, schema, type->symbol.name );
                    fprintf( files->init, "        %s->ReferentType(%s);\n", tdnm,
                             typename_buf );
                }
                break;
            }
            default:
                break;
        }
    }

    /* DAR - moved fn call below from TYPEselect_print to here to put all init
    ** info together. */
    if( TYPEis_select( type ) ) {
        TYPEselect_init_print( type, files->init, schema );
    }
#ifdef NEWDICT
    /* DAS New SDAI Dictionary 5/95 */
    /* insert the type into the schema descriptor */
    fprintf( files->init,
             "        ((SDAIAGGRH(Set,DefinedTypeH))%s::schema->Types())->Add((DefinedTypeH)%s);\n",
             SCHEMAget_name( schema ), tdnm );
#endif
    /* insert into type dictionary */
    fprintf( files->init, "        reg.AddType (*%s);\n", tdnm );
}

/** print name, fundamental type, and description initialization function
   calls */
void TYPEprint_nm_ft_desc( Schema schema, const Type type, FILE * f, char * endChars ) {

    fprintf( f, "                  \"%s\",        // Name\n",
             PrettyTmpName( TYPEget_name( type ) ) );
    fprintf( f, "                  %s,        // FundamentalType\n",
             FundamentalType( type, 1 ) );
    fprintf( f, "                  %s::schema,        // Originating Schema\n",
             SCHEMAget_name( schema ) );
    fprintf( f, "                  \"%s\"%s        // Description\n",
             TypeDescription( type ), endChars );
}

/** new space for a variable of type TypeDescriptor (or subtype).  This
 *  function is called for Types that have an Express name.
 */
void TYPEprint_new( const Type type, FILE * create, Schema schema ) {
    Linked_List wheres;
    char * whereRule, *whereRule_formatted = NULL;
    size_t whereRule_formatted_size = 0;
    char * ptr, *ptr2;

    Type tmpType = TYPEget_head( type );
    Type bodyType = tmpType;

    /* define type definition */
    /*  in source - the real definition of the TypeDescriptor   */

    if( TYPEis_select( type ) ) {
        char * temp;
        temp = non_unique_types_string( type );
        fprintf( create,
                 "        %s = new SelectTypeDescriptor (\n                  ~%s,        //unique elements,\n",
                 TYPEtd_name( type ),
                 temp );
        sc_free( temp );
        TYPEprint_nm_ft_desc( schema, type, create, "," );

        fprintf( create,
                 "                  (SelectCreator) create_%s);        // Creator function\n",
                 SelectName( TYPEget_name( type ) ) );
    } else
        switch( TYPEget_body( type )->type ) {
            case boolean_:

                fprintf( create, "        %s = new EnumTypeDescriptor (\n",
                         TYPEtd_name( type ) );

                /* fill in it's values  */
                TYPEprint_nm_ft_desc( schema, type, create, "," );
                fprintf( create,
                         "                  (EnumCreator) create_BOOLEAN);        // Creator function\n" );
                break;

            case logical_:

                fprintf( create, "        %s = new EnumTypeDescriptor (\n",
                         TYPEtd_name( type ) );

                /* fill in it's values  */
                TYPEprint_nm_ft_desc( schema, type, create, "," );
                fprintf( create,
                         "                  (EnumCreator) create_LOGICAL);        // Creator function\n" );
                break;

            case enumeration_:

                fprintf( create, "        %s = new EnumTypeDescriptor (\n",
                         TYPEtd_name( type ) );

                /* fill in it's values  */
                TYPEprint_nm_ft_desc( schema, type, create, "," );

                /* get the type name of the underlying type - it is the type that
                   needs to get created */

                tmpType = TYPEget_head( type );
                if( tmpType ) {

                    bodyType = tmpType;

                    while( tmpType ) {
                        bodyType = tmpType;
                        tmpType = TYPEget_head( tmpType );
                    }

                    fprintf( create,
                             "                  (EnumCreator) create_%s);        // Creator function\n",
                             TYPEget_ctype( bodyType ) );
                } else
                    fprintf( create,
                             "                  (EnumCreator) create_%s);        // Creator function\n",
                             TYPEget_ctype( type ) );
                break;

            case aggregate_:
            case array_:
            case bag_:
            case set_:
            case list_:

                fprintf( create, "\n        %s = new %s (\n",
                         TYPEtd_name( type ), GetTypeDescriptorName( type ) );

                /* fill in it's values  */
                TYPEprint_nm_ft_desc( schema, type, create, "," );

                fprintf( create,
                         "                  (AggregateCreator) create_%s);        // Creator function\n\n",
                         ClassName( TYPEget_name( type ) ) );
                break;

            default:
                fprintf( create, "        %s = new TypeDescriptor (\n",
                         TYPEtd_name( type ) );

                /* fill in it's values  */
                TYPEprint_nm_ft_desc( schema, type, create, ");" );

                break;
        }
    /* add the type to the Schema dictionary entry */
    fprintf( create, "        %s::schema->AddType(%s);\n", SCHEMAget_name( schema ), TYPEtd_name( type ) );


    wheres = type->where;

    if( wheres ) {
        fprintf( create, "        %s->_where_rules = new Where_rule__list;\n",
                 TYPEtd_name( type ) );

        LISTdo( wheres, w, Where )
        whereRule = EXPRto_string( w->expr );
        ptr2 = whereRule;

        if( whereRule_formatted_size == 0 ) {
            whereRule_formatted_size = 3 * BUFSIZ;
            whereRule_formatted = ( char * )sc_malloc( sizeof( char ) * whereRule_formatted_size );
        } else if( ( strlen( whereRule ) + 300 ) > whereRule_formatted_size ) {
            sc_free( whereRule_formatted );
            whereRule_formatted_size = strlen( whereRule ) + BUFSIZ;
            whereRule_formatted = ( char * )sc_malloc( sizeof( char ) * whereRule_formatted_size );
        }
        whereRule_formatted[0] = '\0';
        if( w->label ) {
            strcpy( whereRule_formatted, w->label->name );
            strcat( whereRule_formatted, ": (" );
            ptr = whereRule_formatted + strlen( whereRule_formatted );
            while( *ptr2 ) {
                if( *ptr2 == '\n' )
                    ;
                else if( *ptr2 == '\\' ) {
                    *ptr = '\\';
                    ptr++;
                    *ptr = '\\';
                    ptr++;

                } else if( *ptr2 == '(' ) {
                    *ptr = *ptr2;
                    ptr++;
                } else {
                    *ptr = *ptr2;
                    ptr++;
                }
                ptr2++;
            }
            *ptr = '\0';

            strcat( ptr, ");\\n" );
        } else {
            /* no label */
            strcpy( whereRule_formatted, "(" );
            ptr = whereRule_formatted + strlen( whereRule_formatted );

            while( *ptr2 ) {
                if( *ptr2 == '\n' )
                    ;
                else if( *ptr2 == '\\' ) {
                    *ptr = '\\';
                    ptr++;
                    *ptr = '\\';
                    ptr++;

                } else if( *ptr2 == '(' ) {
                    *ptr = *ptr2;
                    ptr++;
                } else {
                    *ptr = *ptr2;
                    ptr++;
                }
                ptr2++;
            }
            *ptr = '\0';
            strcat( ptr, ");\\n" );
        }
        fprintf( create, "        wr = new Where_rule(\"%s\");\n", whereRule_formatted );
        fprintf( create, "        %s->_where_rules->Append(wr);\n",
                 TYPEtd_name( type ) );

        sc_free( whereRule );
        ptr2 = whereRule = 0;
        LISTod
        sc_free( whereRule_formatted );
    }
}

