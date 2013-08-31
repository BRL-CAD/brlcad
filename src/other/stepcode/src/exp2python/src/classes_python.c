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

/******************************************************************
***  The functions in this file generate the C++ code for ENTITY **
***  classes, TYPEs, and TypeDescriptors.                       ***
 **                             **/


/* this is used to add new dictionary calls */
/* #define NEWDICT */

#include <stdlib.h>
#include "classes.h"

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#define EXPRESSION_out(e,p,f) EXPRESSION__out(e,p,OP_UNKNOWN,f)
#define EXPRESSIONop2_out(oe,string,paren,pad,f) \
        EXPRESSIONop2__out(oe,string,paren,pad,OP_UNKNOWN,f)
#define EXPRESSIONop_out(oe,paren,f) EXPRESSIONop__out(oe,paren,OP_UNKNOWN,f)

#define ATTRIBUTE_INITIALIZER_out(e,p,f) ATTRIBUTE_INITIALIZER__out(e,p,OP_UNKNOWN,f)
#define ATTRIBUTE_INITIALIZERop2_out(oe,string,paren,pad,f) \
        ATTRIBUTE_INITIALIZERop2__out(oe,string,paren,pad,OP_UNKNOWN,f)
#define ATTRIBUTE_INITIALIZERop_out(oe,paren,f) ATTRIBUTE_INITIALIZERop__out(oe,paren,OP_UNKNOWN,f)

#define PAD 1
#define NOPAD   0

int isAggregateType( const Type t );
int isAggregate( Variable a );
Variable VARis_type_shifter( Variable a );
const char * ENTITYget_CORBAname( Entity ent );
const char * GetTypeDescriptorName( Type t );

int multiple_inheritance = 1;
int print_logging = 0;
int corba_binding = 0;
int old_accessors = 0;

/* several classes use attr_count for naming attr dictionary entry
   variables.  All but the last function generating code for a particular
   entity increment a copy of it for naming each attr in the entity.
   Here are the functions:
   ENTITYhead_print (Entity entity, FILE* file,Schema schema)
   LIBdescribe_entity (Entity entity, FILE* file, Schema schema)
   LIBcopy_constructor (Entity ent, FILE* file)
   LIBstructor_print (Entity entity, FILE* file, Schema schema)
   LIBstructor_print_w_args (Entity entity, FILE* file, Schema schema)
   ENTITYincode_print (Entity entity, FILE* file,Schema schema)
   DAS
 */
static int attr_count;  /* number each attr to avoid inter-entity clashes */
static int type_count;  /* number each temporary type for same reason above */

extern int any_duplicates_in_select( const Linked_List list );
extern int unique_types( const Linked_List list );
extern char * non_unique_types_string( const Type type );
static void printEnumCreateHdr( FILE *, const Type );
static void printEnumCreateBody( FILE *, const Type );
static void printEnumAggrCrHdr( FILE *, const Type );
static void printEnumAggrCrBody( FILE *, const Type );
void printAccessHookFriend( FILE *, const char * );
void printAccessHookHdr( FILE *, const char * );
int TYPEget_RefTypeVarNm( const Type t, char * buf, Schema schema );
void TypeBody_Description( TypeBody body, char * buf );

void STATEMENTSPrint( Linked_List stmts , int indent_level, FILE * file );
void STATEMENTPrint( Statement s, int indent_level, FILE * file );
void STATEMENTlist_out( Linked_List stmts, int indent_level, FILE * file );
void EXPRESSION__out( Expression e, int paren, int previous_op , FILE * file );
void EXPRESSIONop__out( struct Op_Subexpression * oe, int paren, int previous_op , FILE * file );
void EXPRESSIONop1_out( struct Op_Subexpression * eo, char * opcode, int paren, FILE * file );
void EXPRESSIONop2__out( struct Op_Subexpression * eo, char * opcode, int paren, int pad, int previous_op, FILE * file );
void ATTRIBUTE_INITIALIZER__out( Expression e, int paren, int previous_op , FILE * file );
void ATTRIBUTE_INITIALIZERop__out( struct Op_Subexpression * oe, int paren, int previous_op , FILE * file );
void ATTRIBUTE_INITIALIZERop1_out( struct Op_Subexpression * eo, char * opcode, int paren, FILE * file );
void ATTRIBUTE_INITIALIZERop2__out( struct Op_Subexpression * eo, char * opcode, int paren, int pad, int previous_op, FILE * file );
void CASEout( struct Case_Statement_ *c, int level, FILE * file );
void LOOPpyout( struct Loop_ *loop, int level, FILE * file );
void WHEREPrint( Linked_List wheres, int level , FILE * file );

/*
Turn the string into a new string that will be printed the same as the
original string. That is, turn backslash into a quoted backslash and
turn \n into "\n" (i.e. 2 chars).
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

void
USEREFout( Schema schema, Dictionary refdict, Linked_List reflist, char * type, FILE * file ) {
    Dictionary dict;
    DictionaryEntry de;
    struct Rename * r;
    Linked_List list;
    char td_name[BUFSIZ];
    char sch_name[BUFSIZ];

    strncpy( sch_name, PrettyTmpName( SCHEMAget_name( schema ) ), BUFSIZ );

    LISTdo( reflist, s, Schema ) {
        fprintf( file, "\t// %s FROM %s; (all objects)\n", type, s->symbol.name );
        fprintf( file, "\tis = new Interface_spec(\"%s\",\"%s\");\n", sch_name, PrettyTmpName( s->symbol.name ) );
        fprintf( file, "\tis->all_objects_(1);\n" );
        if( !strcmp( type, "USE" ) ) {
            fprintf( file, "\t%s%s->use_interface_list_()->Append(is);\n", SCHEMA_PREFIX, SCHEMAget_name( schema ) );
        } else {
            fprintf( file, "\t%s%s->ref_interface_list_()->Append(is);\n", SCHEMA_PREFIX, SCHEMAget_name( schema ) );
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
            DICTdefine( dict, r->schema->symbol.name, list,
                        ( Symbol * )0, OBJ_UNKNOWN );
        }
        LISTadd( list, r );
    }

    /* step 2: for each list, print out the renames */
    DICTdo_init( dict, &de );
    while( 0 != ( list = ( Linked_List )DICTdo( &de ) ) ) {
        bool first_time = true;
        LISTdo( list, r, struct Rename * )

        /*
           Interface_spec_ptr is;
           Used_item_ptr ui;
           is = new Interface_spec(const char * cur_sch_id);
           schemadescriptor->use_interface_list_()->Append(is);
           ui = new Used_item(TypeDescriptor *ld, const char *oi, const char *ni) ;
           is->_explicit_items->Append(ui);
        */

        /* note: SCHEMAget_name(r->schema) equals r->schema->symbol.name) */
        if( first_time ) {
            fprintf( file, "\t// %s FROM %s (selected objects)\n", type, r->schema->symbol.name );
            fprintf( file, "\tis = new Interface_spec(\"%s\",\"%s\");\n", sch_name, PrettyTmpName( r->schema->symbol.name ) );
            if( !strcmp( type, "USE" ) ) {
                fprintf( file, "\t%s%s->use_interface_list_()->Append(is);\n", SCHEMA_PREFIX, SCHEMAget_name( schema ) );
            } else {
                fprintf( file, "\t%s%s->ref_interface_list_()->Append(is);\n", SCHEMA_PREFIX, SCHEMAget_name( schema ) );
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
            fprintf( file, "\t// object %s AS %s\n", r->old->name,
                     r->nnew->name );
            if( !strcmp( type, "USE" ) ) {
                fprintf( file, "\tui = new Used_item(\"%s\", %s, \"%s\", \"%s\");\n", r->schema->symbol.name, td_name, r->old->name, r->nnew->name );
                fprintf( file, "\tis->explicit_items_()->Append(ui);\n" );
                fprintf( file, "\t%s%s->interface_().explicit_items_()->Append(ui);\n", SCHEMA_PREFIX, SCHEMAget_name( schema ) );
            } else {
                fprintf( file, "\tri = new Referenced_item(\"%s\", %s, \"%s\", \"%s\");\n", r->schema->symbol.name, td_name, r->old->name, r->nnew->name );
                fprintf( file, "\tis->explicit_items_()->Append(ri);\n" );
                fprintf( file, "\t%s%s->interface_().explicit_items_()->Append(ri);\n", SCHEMA_PREFIX, SCHEMAget_name( schema ) );
            }
        } else {
            fprintf( file, "\t// object %s\n", r->old->name );
            if( !strcmp( type, "USE" ) ) {
                fprintf( file, "\tui = new Used_item(\"%s\", %s, \"\", \"%s\");\n", r->schema->symbol.name, td_name, r->nnew->name );
                fprintf( file, "\tis->explicit_items_()->Append(ui);\n" );
                fprintf( file, "\t%s%s->interface_().explicit_items_()->Append(ui);\n", SCHEMA_PREFIX, SCHEMAget_name( schema ) );
            } else {
                fprintf( file, "\tri = new Referenced_item(\"%s\", %s, \"\", \"%s\");\n", r->schema->symbol.name, td_name, r->nnew->name );
                fprintf( file, "\tis->explicit_items_()->Append(ri);\n" );
                fprintf( file, "\t%s%s->interface_().explicit_items_()->Append(ri);\n", SCHEMA_PREFIX, SCHEMAget_name( schema ) );
            }
        }
        LISTod
    }
    HASHdestroy( dict );
}

const char *
IdlEntityTypeName( Type t ) {
}


int Handle_FedPlus_Args( int i, char * arg ) {
    if( ( ( char )i == 's' ) || ( ( char )i == 'S' ) ) {
        multiple_inheritance = 0;
    }
    if( ( ( char )i == 'a' ) || ( ( char )i == 'A' ) ) {
        old_accessors = 1;
    }
    if( ( char )i == 'L' ) {
        print_logging = 1;
    }
    if( ( ( char )i == 'c' ) || ( ( char )i == 'C' ) ) {
        corba_binding = 1;
    }
    return 0;
}


bool is_python_keyword( char * word ) {
    bool python_keyword = false;
    if( strcmp( word, "class" ) == 0 ) {
        python_keyword = true;
    }
    return python_keyword;
}

/******************************************************************
 ** Procedure:  generate_attribute_name
 ** Parameters:  Variable a, an Express attribute; char *out, the C++ name
 ** Description:  converts an Express name into the corresponding C++ name
 **       see relation to generate_dict_attr_name() DAS
 ** Side Effects:
 ** Status:  complete 8/5/93
 ******************************************************************/
char *
generate_attribute_name( Variable a, char * out ) {
    char * temp, *p, *q;
    int j;
    temp = EXPRto_string( VARget_name( a ) );
    p = temp;
    if( ! strncmp( StrToLower( p ), "self\\", 5 ) ) {
        p = p + 5;
    }
    /*  copy p to out  */
    /* DAR - fixed so that '\n's removed */
    for( j = 0, q = out; j < BUFSIZ; p++ ) {
        /* copy p to out, 1 char at time.  Skip \n's and spaces, convert */
        /*  '.' to '_', and convert to lowercase. */
        if( ( *p != '\n' ) && ( *p != ' ' ) ) {
            if( *p == '.' ) {
                *q = '_';
            } else {
                *q = tolower( *p );
            }
            j++;
            q++;
        }
    }
    free( temp );
    // python generator : we should prevend an attr name to be a python reserved keyword
    if( is_python_keyword( out ) ) {
        strcat( out, "_" );
    }
    return out;
}

char *
generate_attribute_func_name( Variable a, char * out ) {
    generate_attribute_name( a, out );
    strncpy( out, CheckWord( StrToLower( out ) ), BUFSIZ );
    if( old_accessors ) {
        out[0] = toupper( out[0] );
    } else {
        out[strlen( out )] = '_';
    }
    return out;
}

/******************************************************************
 ** Procedure:  generate_dict_attr_name
 ** Parameters:  Variable a, an Express attribute; char *out, the C++ name
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
char *
generate_dict_attr_name( Variable a, char * out ) {
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
    for( j = 0, q = out; j < BUFSIZ; p++ ) {
        /* copy p to out, 1 char at time.  Skip \n's, and convert to lc. */
        if( *p != '\n' ) {
            *q = tolower( *p );
            j++;
            q++;
        }
    }

    free( temp );
    return out;
}

/******************************************************************
**      Entity Generation                */

/******************************************************************
 ** Procedure:  ENTITYhead_print
 ** Parameters:  const Entity entity
 **   FILE* file  --  file being written to
 ** Returns:
 ** Description:  prints the beginning of the entity class definition for the
 **               c++ code and the declaration of extern attr descriptors for
 **       the registry.  In the .h file
 ** Side Effects:  generates c++ code
 ** Status:  good 1/15/91
 **          added registry things 12-Apr-1993
 ******************************************************************/

void
ENTITYhead_print( Entity entity, FILE * file, Schema schema ) {
    char entnm [BUFSIZ];
    char attrnm [BUFSIZ];
    Linked_List list;
    int attr_count_tmp = attr_count;
    Entity super = 0;

    strncpy( entnm, ENTITYget_classname( entity ), BUFSIZ );
    entnm[BUFSIZ-1] = '\0';

    /* DAS print all the attr descriptors and inverse attr descriptors for an
       entity as extern defs in the .h file. */
    LISTdo( ENTITYget_attributes( entity ), v, Variable )
    generate_attribute_name( v, attrnm );
    fprintf( file, "extern %s *%s%d%s%s;\n",
             ( VARget_inverse( v ) ? "Inverse_attribute" : ( VARis_derived( v ) ? "Derived_attribute" : "AttrDescriptor" ) ),
             ATTR_PREFIX, attr_count_tmp++,
             ( VARis_derived( v ) ? "D" : ( VARis_type_shifter( v ) ? "R" : ( VARget_inverse( v ) ? "I" : "" ) ) ),
             attrnm );

    /* **** testing the functions **** */
    /*
        if( !(VARis_derived(v) &&
          VARget_initializer(v) &&
          VARis_type_shifter(v) &&
          VARis_overrider(entity, v)) )
          fprintf(file,"// %s Attr is not derived, a type shifter, overrider, no initializer.\n",attrnm);

        if(VARis_derived (v))
          fprintf(file,"// %s Attr is derived\n",attrnm);
        if (VARget_initializer (v))
          fprintf(file,"// %s Attr has an initializer\n",attrnm);
        if(VARis_type_shifter (v))
          fprintf(file,"// %s Attr is a type shifter\n",attrnm);
        if(VARis_overrider (entity, v))
          fprintf(file,"// %s Attr is an overrider\n",attrnm);
    */
    /* ****** */

    LISTod

    fprintf( file, "\nclass %s  :  ", entnm );

    /* inherit from either supertype entity class or root class of
       all - i.e. SCLP23(Application_instance) */

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
        fprintf( file, "  public SCLP23(Application_instance) {\n" );
    }


}

/******************************************************************
 ** Procedure:  DataMemberPrint
 ** Parameters:  const Entity entity  --  entity being processed
 **   FILE* file  --  file being written to
 ** Returns:
 ** Description:  prints out the data members for an entity's c++ class
 **               definition
 ** Side Effects:  generates c++ code
 ** Status:  ok 1/15/91
 ******************************************************************/

void
DataMemberPrint( Entity entity, FILE * file, Schema schema ) {
}

/******************************************************************
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

void
MemberFunctionSign( Entity entity, FILE * file ) {

}

/******************************************************************
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
char *
GetAttrTypeName( Type t ) {
    char * attr_type;
    if( TYPEis_string( t ) ) {
        attr_type = "STRING";
    } else if( TYPEis_logical( t ) ) {
        attr_type = "LOGICAL";
    } else if( TYPEis_boolean( t ) ) {
        attr_type = "BOOLEAN";
    } else if( TYPEis_real( t ) ) {
        attr_type = "REAL";
    } else if( TYPEis_integer( t ) ) {
        attr_type = "INTEGER";
    } else {
        attr_type = TYPEget_name( t );
    }
    return attr_type;
}

/*
*
* A function that prints BAG, ARRAY, SET or LIST to the file
*
*/

void
print_aggregate_type( FILE * file, Type t ) {
    switch( TYPEget_body( t )->type ) {
        case array_:
            fprintf( file, "ARRAY" );
            break;
        case bag_:
            fprintf( file, "BAG" );
            break;
        case set_:
            fprintf( file, "SET" );
            break;
        case list_:
            fprintf( file, "LIST" );
            break;
        default:
            break;
    }
}

/*
*
* A recursive function to export aggregate to python
*
*/
void
process_aggregate( FILE * file, Type t ) {
    Expression lower = AGGR_TYPEget_lower_limit( t );
    char * lower_str = EXPRto_string( lower );
    Expression upper = AGGR_TYPEget_upper_limit( t );
    char * upper_str = NULL;
    Type base_type;
    if( upper == LITERAL_INFINITY ) {
        upper_str = "None";
    } else {
        upper_str = EXPRto_string( upper );
    }
    switch( TYPEget_body( t )->type ) {
        case array_:
            fprintf( file, "ARRAY" );
            break;
        case bag_:
            fprintf( file, "BAG" );
            break;
        case set_:
            fprintf( file, "SET" );
            break;
        case list_:
            fprintf( file, "LIST" );
            break;
        default:
            break;
    }
    fprintf( file, "(%s,%s,", lower_str, upper_str );
    //write base type
    base_type = TYPEget_base_type( t );
    if( TYPEis_aggregate( base_type ) ) {
        process_aggregate( file, base_type );
        fprintf( file, ")" ); //close parenthesis
    } else {
        char * array_base_type = GetAttrTypeName( TYPEget_base_type( t ) );
        //fprintf(file,"%s)",array_base_type);
        fprintf( file, "'%s', scope = schema_scope)", array_base_type );
    }
}

void
LIBdescribe_entity( Entity entity, FILE * file, Schema schema ) {
    int attr_count_tmp = attr_count;
    char attrnm [BUFSIZ], parent_attrnm[BUFSIZ];
    char * attr_type;
    bool generate_constructor = true; //by default, generates a python constructor
    bool single_inheritance = false;
    bool multiple_inheritance = false;
    Type t;
    Linked_List list;
    int num_parent = 0;
    int num_derived_inverse_attr = 0;

    /* class name
     need to use new-style classes for properties to work correctly
    so class must inherit from object */
    if( is_python_keyword( ENTITYget_name( entity ) ) ) {
        fprintf( file, "class %s_(", ENTITYget_name( entity ) );
    } else {
        fprintf( file, "class %s(", ENTITYget_name( entity ) );
    }

    /*
    * Look for inheritance and super classes
    */
    list = ENTITYget_supertypes( entity );
    num_parent = 0;
    if( ! LISTempty( list ) ) {
        LISTdo( list, e, Entity )
        /*  if there\'s no super class yet,
            or the super class doesn\'t have any attributes
        */
        if( num_parent > 0 ) {
            fprintf( file, "," );    //separator for parent classes names
        }
        if( is_python_keyword( ENTITYget_name( e ) ) ) {
            fprintf( file, "%s_", ENTITYget_name( e ) );
        } else {
            fprintf( file, "%s", ENTITYget_name( e ) );
        }
        num_parent++;
        LISTod;
        if( num_parent == 1 ) {
            single_inheritance = true;
            multiple_inheritance = false;
        } else {
            single_inheritance = false;
            multiple_inheritance = true;
        }
    } else {
        //inherit from BaseEntityClass by default, in order to enable decorators
        // as well as advanced __repr__ feature
        fprintf( file, "BaseEntityClass" );
    }
    fprintf( file, "):\n" );

    /*
    * Write docstrings in a Sphinx compliant manner
    */
    fprintf( file, "\t'''Entity %s definition.\n", ENTITYget_name( entity ) );
    LISTdo( ENTITYget_attributes( entity ), v, Variable )
    generate_attribute_name( v, attrnm );
    t = VARget_type( v );
    fprintf( file, "\n\t:param %s\n", attrnm );
    fprintf( file, "\t:type %s:", attrnm );
    if( TYPEis_aggregate( t ) ) {
        process_aggregate( file, t );
        fprintf( file, "\n" );
    } else {
        if( TYPEget_name( t ) == NULL ) {
            attr_type = GetAttrTypeName( t );
        } else {
            attr_type = TYPEget_name( t );
        }
        fprintf( file, "%s\n", attr_type );
    }
    attr_count_tmp++;
    LISTod
    fprintf( file, "\t'''\n" );
    /*
    * Before writing constructor, check if this entity has any attribute
    * other wise just a 'pass' statement is enough
    */
    attr_count_tmp = 0;
    num_derived_inverse_attr = 0;
    LISTdo( ENTITYget_attributes( entity ), v, Variable )
    if( VARis_derived( v ) || VARget_inverse( v ) ) {
        num_derived_inverse_attr++;
    } else {
        attr_count_tmp++;
    }
    LISTod
    if( ( attr_count_tmp == 0 ) && !single_inheritance && !multiple_inheritance ) {
        fprintf( file, "\t# This class does not define any attribute.\n" );
        fprintf( file, "\tpass\n" );
        generate_constructor = false;
    }
    if( false ) {}
    else {
        /*
        * write class constructor
        */
        if( generate_constructor ) {
            fprintf( file, "\tdef __init__( self , " );
        }
        // if inheritance, first write the inherited parameters
        list = ENTITYget_supertypes( entity );
        num_parent = 0;
        int index_attribute = 0;
        if( ! LISTempty( list ) ) {
            LISTdo( list, e, Entity )
            /*  search attribute names for superclass */
            LISTdo( ENTITYget_all_attributes( e ), v2, Variable )
            generate_attribute_name( v2, parent_attrnm );
            //fprintf(file,"%s__%s , ",ENTITYget_name(e),parent_attrnm);
            if( !VARis_derived( v2 ) && !VARget_inverse( v2 ) ) {
                fprintf( file, "inherited%i__%s , ", index_attribute, parent_attrnm );
                index_attribute++;
            }
            LISTod
            num_parent++;
            LISTod;
        }
        LISTdo( ENTITYget_attributes( entity ), v, Variable )
        generate_attribute_name( v, attrnm );
        if( !VARis_derived( v ) && !VARget_inverse( v ) ) {
            fprintf( file, "%s,", attrnm );
        }
        LISTod
        // close constructor method
        if( generate_constructor ) {
            fprintf( file, " ):\n" );
        }
        /** if inheritance, first init base class **/
        list = ENTITYget_supertypes( entity );
        index_attribute = 0;
        if( ! LISTempty( list ) ) {
            LISTdo( list, e, Entity )
            fprintf( file, "\t\t%s.__init__(self , ", ENTITYget_name( e ) );
            /*  search and write attribute names for superclass */
            LISTdo( ENTITYget_all_attributes( e ), v2, Variable )
            generate_attribute_name( v2, parent_attrnm );
            //fprintf(file,"%s__%s , ",ENTITYget_name(e),parent_attrnm);
            if( !VARis_derived( v2 ) && !VARget_inverse( v2 ) ) {
                fprintf( file, "inherited%i__%s , ", index_attribute, parent_attrnm );
                index_attribute++;
            }
            LISTod
            num_parent++;
            fprintf( file, ")\n" ); //separator for parent classes names
            LISTod;
        }
        // init variables in constructor
        LISTdo( ENTITYget_attributes( entity ), v, Variable )
        generate_attribute_name( v, attrnm );
        if( !VARis_derived( v ) && !VARget_inverse( v ) ) {
            fprintf( file, "\t\tself.%s = %s\n", attrnm, attrnm );
        }
        //attr_count_tmp++;
        LISTod
        /*
        * write attributes as python properties
        */
        LISTdo( ENTITYget_attributes( entity ), v, Variable )
        generate_attribute_name( v, attrnm );
        fprintf( file, "\n\t@apply\n" );
        fprintf( file, "\tdef %s():\n", attrnm );
        // fget
        fprintf( file, "\t\tdef fget( self ):\n" );
        if( !VARis_derived( v ) ) {
            fprintf( file, "\t\t\treturn self._%s\n", attrnm );
        } else {
            // evaluation of attribute
            fprintf( file, "\t\t\tattribute_eval = " );
            // outputs expression initializer
            ATTRIBUTE_INITIALIZER_out( v->initializer, 1, file );
            // then returns the value
            fprintf( file, "\n\t\t\treturn attribute_eval\n" );
        }
        // fset
        fprintf( file, "\t\tdef fset( self, value ):\n" );
        t = VARget_type( v );

        // find attr type name
        if( TYPEget_name( t ) == NULL ) {
            attr_type = GetAttrTypeName( t );
        } else {
            attr_type = TYPEget_name( t );
        }

        if( !VARis_derived( v ) && !VARget_inverse( v ) ) {
            // if the argument is not optional
            if( !VARget_optional( v ) ) {
                fprintf( file, "\t\t# Mandatory argument\n" );
                fprintf( file, "\t\t\tif value==None:\n" );
                fprintf( file, "\t\t\t\traise AssertionError('Argument %s is mantatory and can not be set to None')\n", attrnm );
                fprintf( file, "\t\t\tif not check_type(value," );
                if( TYPEis_aggregate( t ) ) {
                    process_aggregate( file, t );
                    fprintf( file, "):\n" );
                } else {
                    fprintf( file, "%s):\n", attr_type );
                }
            } else {
                fprintf( file, "\t\t\tif value != None: # OPTIONAL attribute\n\t" );
                fprintf( file, "\t\t\tif not check_type(value," );
                if( TYPEis_aggregate( t ) ) {
                    process_aggregate( file, t );
                    fprintf( file, "):\n\t" );
                } else {
                    fprintf( file, "%s):\n\t", attr_type );
                }
            }
            // check wether attr_type is aggr or explicit
            if( TYPEis_aggregate( t ) ) {
                fprintf( file, "\t\t\t\tself._%s = ", attrnm );
                print_aggregate_type( file, t );
                fprintf( file, "(value)\n" );
            } else {
                fprintf( file, "\t\t\t\tself._%s = %s(value)\n", attrnm, attr_type );
            }
            if( VARget_optional( v ) ) {
                fprintf( file, "\t\t\t\telse:\n" );
                fprintf( file, "\t\t\t\t\tself._%s = value\n", attrnm );
            }
            fprintf( file, "\t\t\telse:\n\t" );
            fprintf( file, "\t\t\tself._%s = value\n", attrnm );
        }
        // if the attribute is derived, prevent fset to attribute to be set
        else if( VARis_derived( v ) ) {
            fprintf( file, "\t\t# DERIVED argument\n" );
            fprintf( file, "\t\t\traise AssertionError('Argument %s is DERIVED. It is computed and can not be set to any value')\n", attrnm );
        } else if( VARget_inverse( v ) ) {
            fprintf( file, "\t\t# INVERSE argument\n" );
            fprintf( file, "\t\t\traise AssertionError('Argument %s is INVERSE. It is computed and can not be set to any value')\n", attrnm );
        }
        fprintf( file, "\t\treturn property(**locals())\n" );
        LISTod
    }
    // before exiting, process where rules
    WHEREPrint( entity->where, 0, file );
}

int
get_local_attribute_number( Entity entity ) {
    int i = 0;
    Linked_List local = ENTITYget_attributes( entity );
    LISTdo( local, a, Variable )
    /*  go to the child's first explicit attribute */
    if( ( ! VARget_inverse( a ) ) && ( ! VARis_derived( a ) ) ) {
        ++i;
    }
    LISTod;
    return i;
}

int
get_attribute_number( Entity entity ) {
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
                           "Attribute %s not found. \n"
                           /* In this case, a is a Variable - so macro VARget_name (a) expands  *
                            * to an Expression. The first element of an Expression is a Symbol. *
                            * The first element of a Symbol is char * name.                     */
                           , __FILE__, __LINE__, VARget_name( a )->symbol.name );
    }

    LISTod;
    return -1;
}

void
LIBstructor_print( Entity entity, FILE * file, Schema schema ) {
}

/********************/
/* print the constructor that accepts a SCLP23(Application_instance) as an argument used
   when building multiply inherited entities.
*/

void
LIBstructor_print_w_args( Entity entity, FILE * file, Schema schema ) {
}

/******************************************************************
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
void
ENTITYlib_print( Entity entity, FILE * file, Schema schema ) {
    LIBdescribe_entity( entity, file, schema );
}

//FIXME should return bool
/* return 1 if types are predefined by us */
int
TYPEis_builtin( const Type t ) {
    switch( TYPEget_body( t )->type ) { /* dunno if correct*/
        case integer_:
        case real_:
        case string_:
        case binary_:
        case boolean_:
        case number_:
        case logical_:
            return 1;
            break;
        default:
            break;
    }
    return 0;
}


void
print_typechain( FILE * f, const Type t, char * buf, Schema schema ) {
}

void
ENTITYincode_print( Entity entity, FILE * file, Schema schema ) {
}

/******************************************************************
 ** Procedure:  RULEPrint
 ** Parameters:  Rule *rule --  rule being processed
 **     FILE* file  --  file being written to
 ** Returns:
 ** Description:  drives the functions for printing out code in lib
 **  and initialization files for a specific rule
 ** Status:  started 2012/3/1
 ******************************************************************/
void
RULEPrint( Rule rule, FILES * files, Schema schema ) {
    char * n = RULEget_name( rule );
    fprintf( files->lib, "\n####################\n # RULE %s #\n####################\n", n );
    /* write function definition */
    fprintf( files->lib, "%s = Rule()\n", n );
    /* write parameter list */
    //LISTdo(RULEget_parameters( rule ), v, Variable)
    //    param_name = EXPRto_string( VARget_name( v ) );
    //    fprintf(files->lib, "%s,",param_name);
    //LISTod

    //close function prototype
    //fprintf(files->lib,"):\n");
    /* so far, just raise "not implemented" */
    //fprintf(files->lib, "\traise NotImplemented('Function %s not implemented)')\n",n);
}


/******************************************************************
 ** Procedure:  FUNCPrint
 ** Parameters:  Function *function --  function being processed
 **     FILE* file  --  file being written to
 ** Returns:
 ** Description:  drives the functions for printing out code in lib
 **  and initialization files for a specific function
 ** Status:  started 2012/3/1
 ******************************************************************/
void
FUNCPrint( Function function, FILES * files, Schema schema ) {
    char * function_name = FUNCget_name( function );
    char * param_name;
    Type t, return_type = FUNCget_return_type( function );
    fprintf( files->lib, "\n####################\n # FUNCTION %s #\n####################\n", function_name );

    /* write function definition */
    fprintf( files->lib, "def %s(", function_name );

    /* write parameter list */
    LISTdo( FUNCget_parameters( function ), v, Variable )
    param_name = EXPRto_string( VARget_name( v ) );
    t = VARget_type( v );
    fprintf( files->lib, "%s,", param_name );
    LISTod
    fprintf( files->lib, "):\n" );

    // print function docstring
    fprintf( files->lib, "\t'''\n" );
    LISTdo( FUNCget_parameters( function ), v, Variable )
    param_name = EXPRto_string( VARget_name( v ) );
    t = VARget_type( v );
    fprintf( files->lib, "\t:param %s\n", param_name );
    fprintf( files->lib, "\t:type %s:%s\n", param_name, GetAttrTypeName( t ) );
    LISTod
    fprintf( files->lib, "\t'''\n" );

    // process statements. The indent_level is set to 1 (the number of tabs \t)
    STATEMENTSPrint( function->u.proc->body, 1, files->lib );

}

void
STATEMENTSPrint( Linked_List stmts , int indent_level, FILE * file ) {
    LISTdo( stmts, stmt, Statement )
    STATEMENTPrint( stmt, indent_level, file );
    LISTod
}

void python_indent( FILE * file, int indent_level ) {
    int i;
    for( i = 0; i < indent_level; i++ ) {
        fprintf( file, "\t" );
    }
}

void
STATEMENTPrint( Statement s, int indent_level, FILE * file ) {
    bool first_time = true;
    python_indent( file, indent_level );
    if( !s ) {  /* null statement */
        fprintf( file, "pass" );
        return;
    }
    switch( s->type ) {
        case STMT_ASSIGN:
            EXPRESSION_out( s->u.assign->lhs, 0, file );
            fprintf( file, " = " );
            EXPRESSION_out( s->u.assign->rhs, 0, file );
            fprintf( file, "\n" );
            break;
        case STMT_CASE:
            CASEout( s->u.Case, indent_level, file );
            break;
        case STMT_RETURN:
            fprintf( file, "return " );
            if( s->u.ret->value ) {
                EXPRESSION_out( s->u.ret->value, 0, file );
            }
            fprintf( file, "\n" );
            break;
        case STMT_LOOP:
            LOOPpyout( s->u.loop, indent_level , file );
            break;
        case STMT_ALIAS:
            fprintf( file, "%s = %s\n", s->symbol.name,
                     s->u.alias->variable->name->symbol.name );
            STATEMENTlist_out( s->u.alias->statements, indent_level , file );
            break;
        case STMT_SKIP:
            fprintf( file, "break\n" ); // @TODO: is that correct?
            break;
        case STMT_ESCAPE:
            fprintf( file, "break\n" );
            break;
        case STMT_COMPOUND:
            // following line is necessary other wise indentation
            // errors in python
            fprintf( file, "# begin/end block\n" );
            STATEMENTlist_out( s->u.compound->statements, indent_level, file );
            break;
        case STMT_COND:
            fprintf( file, "if (" );
            EXPRESSION_out( s->u.cond->test, 0 , file );
            fprintf( file, "):\n" );
            STATEMENTlist_out( s->u.cond->code, indent_level + 1, file );
            if( s->u.cond->otherwise ) {
                python_indent( file, indent_level );
                fprintf( file, "else:\n" );
                STATEMENTlist_out( s->u.cond->otherwise, indent_level + 1, file );
            }
            break;
        case STMT_PCALL:
            fprintf( file, "%s(", s->symbol.name );
            LISTdo( s->u.proc->parameters, p, Expression )
            if( first_time ) {
                first_time = false;
            } else {
                fprintf( file, "," );
            }
            EXPRESSION_out( p, 0, file );
            LISTod
            fprintf( file, ")\n" );
    }
}

void
CASEout( struct Case_Statement_ *c, int level, FILE * file ) {
    int if_number = 0;
    //fprintf(file, "for case in switch(");
    //EXPRESSION_out( c->selector, 0 , file);
    //fprintf(file,"):\n");
    fprintf( file, "case_selector = " );
    EXPRESSION_out( c->selector, 0, file );
    fprintf( file, "\n" );
    /* pass 2: print them */
    LISTdo( c->cases, ci, Case_Item )
    if( ci->labels ) {
        LISTdo( ci->labels, label, Expression )
        /* print label(s) */
        //indent2 = level + exppp_continuation_indent;
        python_indent( file, level );
        //fprintf(file, "if case(");
        if( if_number == 0 ) {
            fprintf( file, "if " );
        } else {
            fprintf( file, "elif" );
        }
        fprintf( file, " case_selector == " );
        EXPRESSION_out( label, 0, file );
        fprintf( file, ":\n" );

        /* print action */
        STATEMENTPrint( ci->action, level + 1, file );
        if_number++;
        LISTod
    } else {
        /* print OTHERWISE */
        //indent2 = level + exppp_continuation_indent;
        //fprintf(files->lib,  "%*s", level, "" );
        //python_indent(files->lib,level+1);
        python_indent( file, level );
        //fprintf(file,  "if case():\n" );
        fprintf( file,  "else:\n" );
        //fprintf(files->lib,  "%*s : ", level , "" );

        /* print action */
        STATEMENTPrint( ci->action, level + 1, file );
    }
    LISTod
}

void
LOOPpyout( struct Loop_ *loop, int level, FILE * file ) {
    Variable v;
    fprintf( file, "for " );

    /* increment */
    /*  if (loop->scope->u.incr) {*/
    if( loop->scope ) {
        DictionaryEntry de;

        DICTdo_init( loop->scope->symbol_table, &de );
        v = ( Variable )DICTdo( &de );
        fprintf( file, " %s in range(", v->name->symbol.name );
        EXPRESSION_out( loop->scope->u.incr->init, 0 , file );
        fprintf( file, "," );
        EXPRESSION_out( loop->scope->u.incr->end, 0 , file );
        fprintf( file, "," ); /* parser always forces a "by" expr */
        EXPRESSION_out( loop->scope->u.incr->increment, 0 , file );
        fprintf( file, "):\n" );
    }

    /* while */
    if( loop->while_expr ) {
        fprintf( file, " while " );
        EXPRESSION_out( loop->while_expr, 0 , file );
    }

    /* until */
    if( loop->until_expr ) {
        fprintf( file, " UNTIL " );
        EXPRESSION_out( loop->until_expr, 0 , file );
    }

    STATEMENTlist_out( loop->statements, level + 1 , file );
}

void
STATEMENTlist_out( Linked_List stmts, int indent_level, FILE * file ) {
    LISTdo( stmts, stmt, Statement )
    STATEMENTPrint( stmt, indent_level, file );
    LISTod
}
/*****************************************************************
** Procedure:  ATTRIBUTE_INITIALIZER__out
** Description:  this method is almost the same as EXPRESSION__out
** but it adds self. before each attribute identifier
**
******************************************************************/
void
ATTRIBUTE_INITIALIZER__out( Expression e, int paren, int previous_op , FILE * file ) {
    int i;  /* trusty temporary */
    switch( TYPEis( e->type ) ) {
        case integer_:
            if( e == LITERAL_INFINITY ) {
                fprintf( file, " None " );
            } else {
                fprintf( file, "%d", e->u.integer );
            }
            break;
        case real_:
            if( e == LITERAL_PI ) {
                fprintf( file, " PI " );
            } else if( e == LITERAL_E ) {
                fprintf( file, " E " );;
            } else {
                fprintf( file, "%g", e->u.real );
            }
            break;
        case binary_:
            fprintf( file, "%%%s", e->u.binary ); /* put "%" back */
            break;
        case logical_:
        case boolean_:
            switch( e->u.logical ) {
                case Ltrue:
                    fprintf( file, "TRUE" );
                    break;
                case Lfalse:
                    fprintf( file, "FALSE" );
                    break;
                default:
                    fprintf( file, "UNKNOWN" );
                    break;
            }
            break;
        case string_:
            if( TYPEis_encoded( e->type ) ) {
                fprintf( file, "\"%s\"", e->symbol.name );
            } else {
                fprintf( file, "'%s'", e->symbol.name );
            }
            break;
        case entity_:
        case identifier_:
            fprintf( file, "self.%s", e->symbol.name );
            break;
        case attribute_:
            fprintf( file, "%s", e->symbol.name );
            break;
        case enumeration_:
            fprintf( file, "%s", e->symbol.name );
            break;
        case query_:
            //fprintf(file, "QUERY ( %s <* ", e->u.query->local->name->symbol.name );
            //EXPRESSION_out( e->u.query->aggregate, 1, files );
            //fprintf(file, " | " );
            //EXPRESSION_out( e->u.query->expression, 1 ,files);
            //fprintf(file, " )" );
            //break;

            // so far we don't handle queries
            fprintf( file, "None" );
            break;
        case self_:
            fprintf( file, "self" );
            break;
        case funcall_:
            fprintf( file, "%s(", e->symbol.name );
            i = 0;
            LISTdo( e->u.funcall.list, arg, Expression )
            i++;
            if( i != 1 ) {
                fprintf( file, "," );
            }
            ATTRIBUTE_INITIALIZER_out( arg, 0 , file );
            LISTod
            fprintf( file, ")" );
            break;
        case op_:
            ATTRIBUTE_INITIALIZERop__out( &e->e, paren, previous_op, file );
            break;
        case aggregate_:
            fprintf( file, "[" );
            i = 0;
            LISTdo( e->u.list, arg, Expression )
            i++;
            if( i != 1 ) {
                fprintf( file, "," );
            }
            ATTRIBUTE_INITIALIZER_out( arg, 0 , file );
            LISTod
            fprintf( file, "]" );
            break;
        case oneof_:
            fprintf( file, "ONEOF (" );

            i = 0;
            LISTdo( e->u.list, arg, Expression )
            i++;
            if( i != 1 ) {
                fprintf( file, "," );
            }
            ATTRIBUTE_INITIALIZER_out( arg, 0, file );
            LISTod

            fprintf( file, ")" );
            break;
        default:
            fprintf( file, "unknown expression, type %d", TYPEis( e->type ) );
    }
}

/*****************************************************************
** Procedure:  EXPRESSION__out
** Description:  converts an EXPRESS expression to python
**     include, and initialization files for a specific entity class
******************************************************************/
void
EXPRESSION__out( Expression e, int paren, int previous_op , FILE * file ) {
    int i;  /* trusty temporary */
    switch( TYPEis( e->type ) ) {
        case integer_:
            if( e == LITERAL_INFINITY ) {
                fprintf( file, " None " );
            } else {
                fprintf( file, "%d", e->u.integer );
            }
            break;
        case real_:
            if( e == LITERAL_PI ) {
                fprintf( file, " PI " );
            } else if( e == LITERAL_E ) {
                fprintf( file, " E " );;
            } else {
                fprintf( file, "%g", e->u.real );
            }
            break;
        case binary_:
            fprintf( file, "%%%s", e->u.binary ); /* put "%" back */
            break;
        case logical_:
        case boolean_:
            switch( e->u.logical ) {
                case Ltrue:
                    fprintf( file, "TRUE" );
                    break;
                case Lfalse:
                    fprintf( file, "FALSE" );
                    break;
                default:
                    fprintf( file, "UNKNOWN" );
                    break;
            }
            break;
        case string_:
            if( TYPEis_encoded( e->type ) ) {
                fprintf( file, "\"%s\"", e->symbol.name );
            } else {
                fprintf( file, "'%s'", e->symbol.name );
            }
            break;
        case entity_:
        case identifier_:
            fprintf( file, "%s", e->symbol.name );
            break;
        case attribute_:
            fprintf( file, "%s", e->symbol.name );
            break;
        case enumeration_:
            fprintf( file, "%s", e->symbol.name );
            break;
        case query_:
            //fprintf(file, "QUERY ( %s <* ", e->u.query->local->name->symbol.name );
            //EXPRESSION_out( e->u.query->aggregate, 1, files );
            //fprintf(file, " | " );
            //EXPRESSION_out( e->u.query->expression, 1 ,files);
            //fprintf(file, " )" );
            //break;

            // so far we don't handle queries
            fprintf( file, "None" );
            break;
        case self_:
            fprintf( file, "self" );
            break;
        case funcall_:
            fprintf( file, "%s(", e->symbol.name );
            i = 0;
            LISTdo( e->u.funcall.list, arg, Expression )
            i++;
            if( i != 1 ) {
                fprintf( file, "," );
            }
            EXPRESSION_out( arg, 0 , file );
            LISTod
            fprintf( file, ")" );
            break;
        case op_:
            EXPRESSIONop__out( &e->e, paren, previous_op, file );
            break;
        case aggregate_:
            fprintf( file, "[" );
            i = 0;
            LISTdo( e->u.list, arg, Expression )
            i++;
            if( i != 1 ) {
                fprintf( file, "," );
            }
            EXPRESSION_out( arg, 0 , file );
            LISTod
            fprintf( file, "]" );
            break;
        case oneof_:
            fprintf( file, "ONEOF (" );

            i = 0;
            LISTdo( e->u.list, arg, Expression )
            i++;
            if( i != 1 ) {
                fprintf( file, "," );
            }
            EXPRESSION_out( arg, 0, file );
            LISTod

            fprintf( file, ")" );
            break;
        default:
            fprintf( file, "unknown expression, type %d", TYPEis( e->type ) );
    }
}

void
ATTRIBUTE_INITIALIZERop__out( struct Op_Subexpression * oe, int paren, int previous_op , FILE * file ) {
    switch( oe->op_code ) {
        case OP_AND:
            ATTRIBUTE_INITIALIZERop2_out( oe, " and ", paren, PAD, file );
            break;
        case OP_ANDOR:
        case OP_OR:
            ATTRIBUTE_INITIALIZERop2_out( oe, " or ", paren, PAD, file );
            break;
        case OP_CONCAT:
        case OP_EQUAL:
            ATTRIBUTE_INITIALIZERop2_out( oe, " == ", paren, PAD, file );
            break;
        case OP_PLUS:
            ATTRIBUTE_INITIALIZERop2_out( oe, " + ", paren, PAD, file );
            break;
        case OP_TIMES:
            ATTRIBUTE_INITIALIZERop2_out( oe, " * ", paren, PAD, file );
            break;
        case OP_XOR:
            ATTRIBUTE_INITIALIZERop2__out( oe, ( char * )0, paren, PAD, previous_op, file );
            break;
        case OP_EXP:
            ATTRIBUTE_INITIALIZERop2_out( oe, " ** ", paren, PAD, file );
            break;
        case OP_GREATER_EQUAL:
            ATTRIBUTE_INITIALIZERop2_out( oe, " >= ", paren, PAD, file );
            break;
        case OP_GREATER_THAN:
            ATTRIBUTE_INITIALIZERop2_out( oe, " > ", paren, PAD, file );
            break;
        case OP_IN:
            //    EXPRESSIONop2_out( oe, " in ", paren, PAD, file );
            //    break;
        case OP_INST_EQUAL:
            ATTRIBUTE_INITIALIZERop2_out( oe, " == ", paren, PAD, file );
            break;
        case OP_INST_NOT_EQUAL:
            ATTRIBUTE_INITIALIZERop2_out( oe, " != ", paren, PAD, file );
            break;
        case OP_LESS_EQUAL:
            ATTRIBUTE_INITIALIZERop2_out( oe, " <= ", paren, PAD, file );
            break;
        case OP_LESS_THAN:
            ATTRIBUTE_INITIALIZERop2_out( oe, " < ", paren, PAD, file );
            break;
        case OP_LIKE:
        case OP_MOD:
            ATTRIBUTE_INITIALIZERop2_out( oe, " % ", paren, PAD, file );
            break;
        case OP_NOT_EQUAL:
            //EXPRESSIONop2_out( oe, ( char * )0, paren, PAD ,file);
            ATTRIBUTE_INITIALIZERop2_out( oe, " != ", paren, PAD , file );
            break;
        case OP_NOT:
            ATTRIBUTE_INITIALIZERop1_out( oe, " not ", paren, file );
            break;
        case OP_REAL_DIV:
        case OP_DIV:
            ATTRIBUTE_INITIALIZERop2_out( oe, "/", paren, PAD, file );
            break;
        case OP_MINUS:
            ATTRIBUTE_INITIALIZERop2_out( oe, "-", paren, PAD, file );
            break;
        case OP_DOT:
            ATTRIBUTE_INITIALIZERop2_out( oe, ".", paren, NOPAD, file );
            break;
        case OP_GROUP:
            ATTRIBUTE_INITIALIZERop2_out( oe, ".", paren, NOPAD, file );
            break;
        case OP_NEGATE:
            ATTRIBUTE_INITIALIZERop1_out( oe, "-", paren, file );
            break;
        case OP_ARRAY_ELEMENT:
            ATTRIBUTE_INITIALIZER_out( oe->op1, 1, file );
            fprintf( file, "[" );
            ATTRIBUTE_INITIALIZER_out( oe->op2, 0, file );
            fprintf( file, "]" );
            break;
        case OP_SUBCOMPONENT:
            ATTRIBUTE_INITIALIZER_out( oe->op1, 1 , file );
            fprintf( file, "[" );
            ATTRIBUTE_INITIALIZER_out( oe->op2, 0, file );
            fprintf( file, ":" );
            ATTRIBUTE_INITIALIZER_out( oe->op3, 0, file );
            fprintf( file, "]" );
            break;
        default:
            fprintf( file, "(* unknown op-expression *)" );
    }
}

/* print expression that has op and operands */
void
EXPRESSIONop__out( struct Op_Subexpression * oe, int paren, int previous_op , FILE * file ) {
    switch( oe->op_code ) {
        case OP_AND:
            EXPRESSIONop2_out( oe, " and ", paren, PAD, file );
            break;
        case OP_ANDOR:
        case OP_OR:
            EXPRESSIONop2_out( oe, " or ", paren, PAD, file );
            break;
        case OP_CONCAT:
        case OP_EQUAL:
            EXPRESSIONop2_out( oe, " == ", paren, PAD, file );
            break;
        case OP_PLUS:
            EXPRESSIONop2_out( oe, " + ", paren, PAD, file );
            break;
        case OP_TIMES:
            EXPRESSIONop2_out( oe, " * ", paren, PAD, file );
            break;
        case OP_XOR:
            EXPRESSIONop2__out( oe, ( char * )0, paren, PAD, previous_op, file );
            break;
        case OP_EXP:
            EXPRESSIONop2_out( oe, " ** ", paren, PAD, file );
            break;
        case OP_GREATER_EQUAL:
            EXPRESSIONop2_out( oe, " >= ", paren, PAD, file );
            break;
        case OP_GREATER_THAN:
            EXPRESSIONop2_out( oe, " > ", paren, PAD, file );
            break;
        case OP_IN:
            //    EXPRESSIONop2_out( oe, " in ", paren, PAD, file );
            //    break;
        case OP_INST_EQUAL:
            EXPRESSIONop2_out( oe, " == ", paren, PAD, file );
            break;
        case OP_INST_NOT_EQUAL:
            EXPRESSIONop2_out( oe, " != ", paren, PAD, file );
            break;
        case OP_LESS_EQUAL:
            EXPRESSIONop2_out( oe, " <= ", paren, PAD, file );
            break;
        case OP_LESS_THAN:
            EXPRESSIONop2_out( oe, " < ", paren, PAD, file );
            break;
        case OP_LIKE:
        case OP_MOD:
            EXPRESSIONop2_out( oe, " % ", paren, PAD, file );
            break;
        case OP_NOT_EQUAL:
            //EXPRESSIONop2_out( oe, ( char * )0, paren, PAD ,file);
            EXPRESSIONop2_out( oe, " != ", paren, PAD , file );
            break;
        case OP_NOT:
            EXPRESSIONop1_out( oe, " not ", paren, file );
            break;
        case OP_REAL_DIV:
        case OP_DIV:
            EXPRESSIONop2_out( oe, "/", paren, PAD, file );
            break;
        case OP_MINUS:
            EXPRESSIONop2_out( oe, "-", paren, PAD, file );
            break;
        case OP_DOT:
            EXPRESSIONop2_out( oe, ".", paren, NOPAD, file );
            break;
        case OP_GROUP:
            EXPRESSIONop2_out( oe, ".", paren, NOPAD, file );
            break;
        case OP_NEGATE:
            EXPRESSIONop1_out( oe, "-", paren, file );
            break;
        case OP_ARRAY_ELEMENT:
            EXPRESSION_out( oe->op1, 1, file );
            fprintf( file, "[" );
            EXPRESSION_out( oe->op2, 0, file );
            fprintf( file, "]" );
            break;
        case OP_SUBCOMPONENT:
            EXPRESSION_out( oe->op1, 1 , file );
            fprintf( file, "[" );
            EXPRESSION_out( oe->op2, 0, file );
            fprintf( file, ":" );
            EXPRESSION_out( oe->op3, 0, file );
            fprintf( file, "]" );
            break;
        default:
            fprintf( file, "(* unknown op-expression *)" );
    }
}

void
EXPRESSIONop2__out( struct Op_Subexpression * eo, char * opcode, int paren, int pad, int previous_op, FILE * file ) {
    if( pad && paren && ( eo->op_code != previous_op ) ) {
        fprintf( file, "(" );
    }
    EXPRESSION__out( eo->op1, 1, eo->op_code , file );
    if( pad ) {
        fprintf( file, " " );
    }
    fprintf( file, "%s", ( opcode ? opcode : EXPop_table[eo->op_code].token ) );
    if( pad ) {
        fprintf( file, " " );
    }
    EXPRESSION__out( eo->op2, 1, eo->op_code, file );
    if( pad && paren && ( eo->op_code != previous_op ) ) {
        fprintf( file, ")" );
    }
}

void
ATTRIBUTE_INITIALIZERop2__out( struct Op_Subexpression * eo, char * opcode, int paren, int pad, int previous_op, FILE * file ) {
    if( pad && paren && ( eo->op_code != previous_op ) ) {
        fprintf( file, "(" );
    }
    ATTRIBUTE_INITIALIZER__out( eo->op1, 1, eo->op_code , file );
    if( pad ) {
        fprintf( file, " " );
    }
    fprintf( file, "%s", ( opcode ? opcode : EXPop_table[eo->op_code].token ) );
    if( pad ) {
        fprintf( file, " " );
    }
    ATTRIBUTE_INITIALIZER__out( eo->op2, 1, eo->op_code, file );
    if( pad && paren && ( eo->op_code != previous_op ) ) {
        fprintf( file, ")" );
    }
}

/* Print out a one-operand operation.  If there were more than two of these */
/* I'd generalize it to do padding, but it's not worth it. */
void
EXPRESSIONop1_out( struct Op_Subexpression * eo, char * opcode, int paren, FILE * file ) {
    if( paren ) {
        fprintf( file, "(" );
    }
    fprintf( file, "%s", opcode );
    EXPRESSION_out( eo->op1, 1, file );
    if( paren ) {
        fprintf( file, ")" );
    }
}

void
ATTRIBUTE_INITIALIZERop1_out( struct Op_Subexpression * eo, char * opcode, int paren, FILE * file ) {
    if( paren ) {
        fprintf( file, "(" );
    }
    fprintf( file, "%s", opcode );
    ATTRIBUTE_INITIALIZER_out( eo->op1, 1, file );
    if( paren ) {
        fprintf( file, ")" );
    }
}

int
EXPRop_length( struct Op_Subexpression * oe ) {
    switch( oe->op_code ) {
        case OP_DOT:
        case OP_GROUP:
            return( 1 + EXPRlength( oe->op1 )
                    + EXPRlength( oe->op2 ) );
        default:
            fprintf( stdout, "EXPRop_length: unknown op-expression" );
    }
    return 0;
}

void
WHEREPrint( Linked_List wheres, int level , FILE * file ) {
    int where_rule_number = 0;

    python_indent( file, level );

    if( !wheres ) {
        return;
    }

    /* pass 2: now print labels and exprs */
    LISTdo( wheres, w, Where )
    if( strcmp( w->label->name, "<unnamed>" ) ) {
        // define a function with the name 'label'
        fprintf( file, "\tdef %s(self):\n", w->label->name );
        fprintf( file, "\t\teval_%s_wr = ", w->label->name );
    } else {
        /* no label */
        fprintf( file, "\tdef unnamed_wr_%i(self):\n", where_rule_number );
        fprintf( file, "\t\teval_unnamed_wr_%i = ", where_rule_number );
    }
    //EXPRESSION_out( w->expr, level+1 , file );
    ATTRIBUTE_INITIALIZER_out( w->expr, level + 1 , file );
    // raise exception if rule violated
    if( strcmp( w->label->name, "<unnamed>" ) ) {
        fprintf( file, "\n\t\tif not eval_%s_wr:\n", w->label->name );
        fprintf( file, "\t\t\traise AssertionError('Rule %s violated')\n", w->label->name );
        fprintf( file, "\t\telse:\n\t\t\treturn eval_%s_wr\n\n", w->label->name );
    } else {
        /* no label */
        fprintf( file, "\n\t\tif not eval_unnamed_wr_%i:\n", where_rule_number );
        fprintf( file, "\t\t\traise AssertionError('Rule unnamed_wr_%i violated')\n", where_rule_number );
        fprintf( file, "\t\telse:\n\t\t\treturn eval_unnamed_wr_%i\n\n", where_rule_number );
        where_rule_number++;
    }
    LISTod
}

int curpos;
int indent2;
char * exppp_bufp = 0;      /* pointer to write position in expppbuf */
char * exppp_buf = 0;
Symbol error_sym;   /* only used when printing errors */
int exppp_buflen = 0;       /* remaining space in expppbuf */

void
expression_output( char * buf, int len ) {
    FILE * exppp_fp = NULL;
    FILE * fp = ( exppp_fp ? exppp_fp : stdout );

    error_sym.line += count_newlines( buf );

    if( exppp_buf ) {
        /* output to string */
        if( len > exppp_buflen ) {
            /* should provide flag to enable complaint */
            /* for now, just ignore */
            return;
        }
        memcpy( exppp_bufp, buf, len + 1 );
        exppp_bufp += len;
        exppp_buflen -= len;
    } else {
        /* output to file */
        fwrite( buf, 1, len, fp );
    }
}

void
#ifdef __STDC__
raw_python( char * fmt, ... ) {
#else
raw_python( va_alist )
va_dcl {
    char * fmt;
#endif
    char * p;
    char buf[10000];
    int len;
    va_list args;
#ifdef __STDC__
    va_start( args, fmt );
#else
    va_start( args );
    fmt = va_arg( args, char * );
#endif

    vsprintf( buf, fmt, args );
    len = strlen( buf );

    expression_output( buf, len );

    if( len ) {
        /* reset cur position based on last newline seen */
        if( 0 == ( p = strrchr( buf, '\n' ) ) ) {
            curpos += len;
        } else {
            curpos = len + buf - p;
        }
    }
}

void
#ifdef __STDC__
wrap_python( char * fmt, ... ) {
#else
wrap_python( va_alist )
va_dcl {
    char * fmt;
#endif
    char * p;
    char buf[10000];
    int len;
    int exppp_linelength;
    va_list args;
#ifdef __STDC__
    va_start( args, fmt );
#else
    va_start( args );
    fmt = va_arg( args, char * );
#endif
    exppp_linelength = 75;      /* leave some slop for closing
    vsprintf( buf, fmt, args );
    len = strlen( buf );

    /* 1st condition checks if string cant fit into current line */
    /* 2nd condition checks if string cant fit into any line */
    /* I.e., if we still can't fit after indenting, don't bother to */
    /* go to newline, just print a long line */
    if( ( ( curpos + len ) > exppp_linelength ) &&
    ( ( indent2 + len ) < exppp_linelength ) ) {
        /* move to new continuation line */
        char line[1000];
        sprintf( line, "\n%*s", indent2, "" );
        expression_output( line, 1 + indent2 );

        curpos = indent2;       /* reset current position */
    }

    expression_output( buf, len );

    if( len ) {
        /* reset cur position based on last newline seen */
        if( 0 == ( p = strrchr( buf, '\n' ) ) ) {
            curpos += len;
        } else {
            curpos = len + buf - p;
        }
    }
}

/******************************************************************
 ** Procedure:  ENTITYPrint
 ** Parameters:  Entity *entity --  entity being processed
 **     FILE* file  --  file being written to
 ** Returns:
 ** Description:  drives the functions for printing out code in lib,
 **     include, and initialization files for a specific entity class
 ** Side Effects:  generates code in 3 files
 ** Status:  complete 1/15/91
 ******************************************************************/

void
ENTITYPrint( Entity entity, FILES * files, Schema schema ) {
    char * n = ENTITYget_name( entity );
    DEBUG( "Entering ENTITYPrint for %s\n", n );
    fprintf( files->lib, "\n####################\n # ENTITY %s #\n####################\n", n );
    ENTITYlib_print( entity, files -> lib, schema );
    DEBUG( "DONE ENTITYPrint\n" )    ;
}

void
MODELPrintConstructorBody( Entity entity, FILES * files, Schema schema
                           /*, int index*/ ) {
}

void
MODELPrint( Entity entity, FILES * files, Schema schema, int index ) {
}

void
ENTITYprint_new( Entity entity, FILES * files, Schema schema, int externMap ) {
}

void
MODELprint_new( Entity entity, FILES * files, Schema schema ) {
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
const char *
EnumCElementName( Type type, Expression expr )  {
}

char *
CheckEnumSymbol( char * s ) {
}

void
TYPEenum_lib_print( const Type type, FILE * f ) {
    DictionaryEntry de;
    Expression expr;
    char c_enum_ele [BUFSIZ];
    // begin the new enum type
    if( is_python_keyword( TYPEget_name( type ) ) ) {
        fprintf( f, "\n# ENUMERATION TYPE %s_\n", TYPEget_name( type ) );
    } else {
        fprintf( f, "\n# ENUMERATION TYPE %s\n", TYPEget_name( type ) );
    }
    // first write all the values of the enum
    DICTdo_type_init( ENUM_TYPEget_items( type ), &de, OBJ_ENUM );
    // then outputs the enum
    if( is_python_keyword( TYPEget_name( type ) ) ) {
        fprintf( f, "%s_ = ENUMERATION(", TYPEget_name( type ) );
    } else {
        fprintf( f, "%s = ENUMERATION(", TYPEget_name( type ) );
    }
    /*  set up the dictionary info  */

    //fprintf( f, "const char * \n%s::element_at (int n) const  {\n", n );
    //fprintf( f, "  switch (n)  {\n" );
    DICTdo_type_init( ENUM_TYPEget_items( type ), &de, OBJ_ENUM );
    while( 0 != ( expr = ( Expression )DICTdo( &de ) ) ) {
        strncpy( c_enum_ele, EnumCElementName( type, expr ), BUFSIZ );
        if( is_python_keyword( EXPget_name( expr ) ) ) {
            fprintf( f, "\n'%s_',", EXPget_name( expr ) );
        } else {
            fprintf( f, "\n\t'%s',", EXPget_name( expr ) );
        }
    }
    fprintf( f, "\n\tscope = schema_scope)\n" );
}


void Type_Description( const Type, char * );

/* return printable version of entire type definition */
/* return it in static buffer */
char *
TypeDescription( const Type t ) {
    static char buf[4000];

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

/* print t's bounds to end of buf */
void
strcat_bounds( TypeBody b, char * buf ) {
    if( !b->upper ) {
        return;
    }

    strcat( buf, " [" );
    strcat_expr( b->lower, buf );
    strcat( buf, ":" );
    strcat_expr( b->upper, buf );
    strcat( buf, "]" );
}

void
TypeBody_Description( TypeBody body, char * buf ) {
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
                default:
                    printf( "Error in %s, line %d: type %d not handled by switch statement.", __FILE__, __LINE__, body->type );
                    abort();
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

void
Type_Description( const Type t, char * buf ) {
    if( TYPEget_name( t ) ) {
        strcat( buf, " " );
        strcat( buf, TYPEget_name( t ) );
        /* strcat(buf,PrettyTmpName (TYPEget_name(t)));*/
    } else {
        TypeBody_Description( TYPEget_body( t ), buf );
    }
}

void
TYPEprint_typedefs( Type t, FILE * classes ) {
}

/* return 1 if it is a multidimensional aggregate at the level passed in
   otherwise return 0;  If it refers to a type that is a multidimensional
   aggregate 0 is still returned. */
int
isMultiDimAggregateType( const Type t ) {
    if( TYPEget_body( t )->base )
        if( isAggregateType( TYPEget_body( t )->base ) ) {
            return 1;
        }
    return 0;
}

/* Get the TypeDescriptor variable name that t's TypeDescriptor references (if
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
*/
int TYPEget_RefTypeVarNm( const Type t, char * buf, Schema schema ) {
}


/*****
   print stuff for types that are declared in Express TYPE statements... i.e.
   extern descriptor declaration in .h file - MOVED BY DAR to TYPEprint_type-
       defs - in order to print all the Sdaiclasses.h stuff in exp2python's
       first pass through each schema.
   descriptor definition in the .cc file
   initialize it in the .init.cc file (DAR - all initialization done in fn
       TYPEprint_init() (below) which is done in exp2python's 1st pass only.)
*****/

void
TYPEprint_descriptions( const Type type, FILES * files, Schema schema ) {
    char tdnm [BUFSIZ],
         typename_buf [MAX_LEN],
         base [BUFSIZ],
         nm [BUFSIZ];
    Type i;

    int where_rule_number = 0;
    strncpy( tdnm, TYPEtd_name( type ), BUFSIZ );
    tdnm[BUFSIZ-1] = '\0';

    if( TYPEis_enumeration( type ) && ( i = TYPEget_ancestor( type ) ) != NULL ) {
        /* If we're a renamed enum type, just print a few typedef's to the
        // original and some specialized create functions: */
        strncpy( base, StrToLower( EnumName( TYPEget_name( i ) ) ), BUFSIZ );
        base[BUFSIZ-1]='\0';
        strncpy( nm, StrToLower( EnumName( TYPEget_name( type ) ) ), BUFSIZ );
        nm[BUFSIZ-1]='\0';
        fprintf( files->lib, "%s = %s\n", nm, base );
        return;
    }

    if( TYPEget_RefTypeVarNm( type, typename_buf, schema ) ) {
        char * output = FundamentalType( type, 0 );
        if( TYPEis_aggregate( type ) ) {
            fprintf( files->lib, "%s = ", TYPEget_name( type ) );
            process_aggregate( files->lib, type );
            fprintf( files->lib, "\n" );
        } else if( TYPEis_select( type ) ) {
            TYPEselect_lib_print( type, files -> lib );
        } else {
            // the defined datatype inherits from the base type
            fprintf( files->lib, "# Defined datatype %s\n", TYPEget_name( type ) );
            fprintf( files->lib, "class %s(", TYPEget_name( type ) );
            if( TYPEget_head( type ) != NULL ) {
                fprintf( files->lib, "%s):\n", TYPEget_name( TYPEget_head( type ) ) );
            } else {
                fprintf( files->lib, "%s):\n", output );
            }
            fprintf( files->lib, "\tdef __init__(self,*kargs):\n" );
            fprintf( files->lib, "\t\tpass\n" );
            // call the where / rules
            LISTdo( type->where, w, Where )
            if( strcmp( w->label->name, "<unnamed>" ) ) {
                // define a function with the name 'label'
                fprintf( files->lib, "\t\tself.%s()\n", w->label->name );
            } else {
                /* no label */
                fprintf( files->lib, "\t\tself.unnamed_wr_%i()\n", where_rule_number );
                where_rule_number ++;
            }
            LISTod
            fprintf( files->lib, "\n" );
            // then we process the where rules
            WHEREPrint( type->where, 0, files->lib );
        }
    } else {
        switch( TYPEget_body( type )->type ) {
            case enumeration_:
                TYPEenum_lib_print( type, files -> lib );
                break;
            case select_:
                break;
            default:
                break;
        }
    }
}


static void
printEnumCreateHdr( FILE * inc, const Type type )
/*
 * Prints a bunch of lines for enumeration creation functions (i.e., "cre-
 * ate_SdaiEnum1()").  Since this is done both for an enum and for "copies"
 * of it (when "TYPE enum2 = enum1"), I placed this code in a separate fn.
 */
{

}

static void
printEnumCreateBody( FILE * lib, const Type type )
/*
 * See header comment above by printEnumCreateHdr.
 */
{
}

static void
printEnumAggrCrHdr( FILE * inc, const Type type )
/*
 * Similar to printEnumCreateHdr above for the enum aggregate.
 */
{
}

static void
printEnumAggrCrBody( FILE * lib, const Type type ) {
}

void
TYPEprint_init( const Type type, FILES * files, Schema schema ) {
}

/* print name, fundamental type, and description initialization function
   calls */

void
TYPEprint_nm_ft_desc( Schema schema, const Type type, FILE * f, char * endChars ) {
}


/* new space for a variable of type TypeDescriptor (or subtype).  This
   function is called for Types that have an Express name. */

void
TYPEprint_new( const Type type, FILE * create, Schema schema ) {
}
