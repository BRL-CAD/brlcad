#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "complexSupport.h"

extern int corba_binding;

void use_ref( Schema, Express, FILES * );

void
create_builtin_type_decl( FILES * files, char * name ) {
    //fprintf( files->incall, "extern TypeDescriptor *%s%s_TYPE;\n",
    //         TD_PREFIX, name );
}

void
create_builtin_type_defn( FILES * files, char * name ) {
    //fprintf( files->initall, "\t%s%s_TYPE = TypeDescriptor (",
    //         TD_PREFIX, name );
    //fprintf( files->initall, "\"%s\", %s_TYPE, \"%s\");\n",
    //         PrettyTmpName( name ), StrToUpper( name ), StrToLower( name ) );
}

/******************************************************************
 ** Procedure:  print_file_header
 ** Parameters: const Schema schema - top-level schema being printed
 **     FILE*        file   - file on which to print header
 ** Returns:
 ** Description:  handles file related tasks that need to be done once
 ** at the beginning of processing.
 ** In this case the file schema.h is initiated
 ** Status:  ok 1/15/91
 ******************************************************************/

void
print_file_header( Express express, FILES * files ) {
    //files -> incall = FILEcreate( "schema.h" );

    /* prevent RCS from expanding this! */
    //fprintf( files->incall, "import sdai\n" );
    //fprintf( files->incall, "import Registry\n" );

    //fprintf( files->incall, "\n#include <STEPaggregate.h>\n" );
    //fprintf( files->incall, "\n#include <STEPundefined.h>\n" );
    //fprintf( files->incall, "\n#include <ExpDict.h>\n" );
    //fprintf( files->incall, "\n#include <STEPattribute.h>\n" );

    //fprintf( files->incall, "\n#include <Sdaiclasses.h>\n" );
    //fprintf( files->incall, "import Sdaiclasses\n" );
    //fprintf( files->incall, "extern void SchemaInit (Registry &);\n" );
    //fprintf( files->incall, "extern void InitSchemasAndEnts (Registry &);\n" );

    //files -> initall = FILEcreate( "schema.py" );
    //fprintf( files->initall, "/* %cId$  */ \n", '\044' );
    //fprintf( files->initall, "#include <schema.h>\n" );
    //fprintf( files->initall, "import schema\n" );
    //fprintf( files-> initall, "class Registry:\n" );

    //fprintf( files->initall, "\tdef SchemaInit (Registry reg)\n{\n" );
    //fprintf( files->initall, "\t extern void InitSchemasAndEnts " );
    //fprintf( files->initall, "(Registry & r);\n" );
    //fprintf( files->initall, "\t InitSchemasAndEnts (reg);\n" );

    // This file will contain instantiation statements for all the schemas and
    // entities in the express file.  (They must all be in separate function
    // called first by SchemaInit() so that all entities will exist
    //files -> create = FILEcreate( "SdaiAll.py" );
    //fprintf( files->create, "/* %cId$  */ \n", '\044' );
    //fprintf( files->create, "#include <schema.h>\n" );
    //fprintf( files->create, "import schema" );
    //fprintf( files->create, "\nvoid\nInitSchemasAndEnts (Registry & reg)\n{\n" );

    // This file declares all entity classes as incomplete types.  This will
    // allow all the .h files to reference all .h's.  We can then have e.g.,
    // entX from schemaA have attribute attr1 = entY from schemaB.
    //files -> classes = FILEcreate( "Sdaiclasses.h" );
    //fprintf( files->classes, "/* %cId$  */ \n", '$' );
    //fprintf( files->classes, "#include <schema.h>\n" );

    /* create built-in types */
    /*  no need to generate
        create_builtin_type_decl(files,"INTEGER");
        create_builtin_type_decl(files,"REAL");
        create_builtin_type_decl(files,"STRING");
        create_builtin_type_decl(files,"BINARY");
        create_builtin_type_decl(files,"BOOLEAN");
        create_builtin_type_decl(files,"LOGICAL");
        create_builtin_type_decl(files,"NUMBER");
        create_builtin_type_decl(files,"GENERIC");
    */
    /* create built-in types */
    /*  no need to generate
        create_builtin_type_defn(files,"INTEGER");
        create_builtin_type_defn(files,"REAL");
        create_builtin_type_defn(files,"STRING");
        create_builtin_type_defn(files,"BINARY");
        create_builtin_type_defn(files,"BOOLEAN");
        create_builtin_type_defn(files,"LOGICAL");
        create_builtin_type_defn(files,"NUMBER");
        create_builtin_type_defn(files,"GENERIC");
    */

}

/******************************************************************
 ** Procedure:  print_file_trailer
 ** Parameters: const Schema schema - top-level schema printed
 **     FILE*        file   - file on which to print trailer
 ** Returns:
 ** Description:  handles cleaning up things at end of processing
 ** Status:  ok 1/15/91
 ******************************************************************/

/*ARGSUSED*/
void
print_file_trailer( Express express, FILES * files ) {
    //FILEclose( files->incall );
    //FILEclose( files->initall );
    //fprintf( files->create, "}\n\n" );
    //FILEclose( files->create );
    //fprintf( files->classes, "\n" );
    //FILEclose( files->classes );
}

/******************************************************************
 **  SCHEMA SECTION                      **/

/******************************************************************
 ** Procedure:  SCOPEPrint
 ** Parameters: const Scope scope   - scope to print
 **     FILE*       file    - file on which to print
 ** Returns:
 ** Description:  cycles through the scopes of the input Express schema
 ** Side Effects:  calls functions for processing entities and types
 ** Status:  working 1/15/91
 ** --  it's still not clear how include files should be organized
 ** and what the relationship is between this organization and the
 ** organization of the schemas in the input Express
 ******************************************************************/

void
SCOPEPrint( Scope scope, FILES * files, Schema schema, Express model,
            ComplexCollect * col, int cnt ) {
    Linked_List list = SCOPEget_entities_superclass_order( scope );
    Linked_List function_list = SCOPEget_functions( scope );
    Linked_List rule_list = SCOPEget_rules( scope );
    DictionaryEntry de;
    Type i;
    int redefs = 0;// index = 0;

    /* fill in the values for the type descriptors */
    /* and print the enumerations */
    //fprintf( files -> inc, "\n/*\t**************  TYPES  \t*/\n" );
    //fprintf( files -> lib, "\n/*\t**************  TYPES  \t*/\n" );
    SCOPEdo_types( scope, t, de )
    // First check for one exception:  Say enumeration type B is defined
    // to be a rename of enum A.  If A is in this schema but has not been
    // processed yet, we must wait till it's processed first.  The reason
    // is because B will basically be defined with a couple of typedefs to
    // the classes which represent A.  (To simplify, we wait even if A is
    // in another schema, so long as it's been processed.)
    if( ( t->search_id == CANPROCESS )
            && ( TYPEis_enumeration( t ) )
            && ( ( i = TYPEget_ancestor( t ) ) != NULL )
            && ( i->search_id >= CANPROCESS ) ) {
        redefs = 1;
    }
    SCOPEod

    SCOPEdo_types( scope, t, de )
    // Do the non-redefined enumerations:
    if( ( t->search_id == CANPROCESS )
            && !( TYPEis_enumeration( t ) && TYPEget_head( t ) ) ) {
        TYPEprint_descriptions( t, files, schema );
        if( !TYPEis_select( t ) ) {
            // Selects have a lot more processing and are done below.
            t->search_id = PROCESSED;
        }
    }
    SCOPEod;

    // process redifined enumerations
    if( redefs ) {
        SCOPEdo_types( scope, t, de )
        if( t->search_id == CANPROCESS && TYPEis_enumeration( t ) ) {
            TYPEprint_descriptions( t, files, schema );
            t->search_id = PROCESSED;
        }
        SCOPEod;
    }

    /*  do the select definitions next, since they depend on the others  */
    //fprintf( files->inc, "\n//\t***** Build the SELECT Types  \t\n" );
    // Note - say we have sel B, rename of sel A (as above by enum's).  Here
    // we don't have to worry about printing B before A.  This is checked in
    // TYPEselect_print().
    SCOPEdo_types( scope, t, de )
    if( t->search_id == CANPROCESS ) {
        // Only selects haven't been processed yet and may still be set to
        // CANPROCESS.
        TYPEselect_print( t, files, schema );
        t->search_id = PROCESSED;
    }
    SCOPEod;

    // process each entity. This must be done *before* typedefs are defined
    LISTdo( list, e, Entity );
    if( e->search_id == CANPROCESS ) {
        ENTITYPrint( e, files, schema );
        e->search_id = PROCESSED;
    }
    LISTod;
    LISTfree( list );

    // process each function. This must be done *before* typedefs are defined
    LISTdo( function_list, f, Function );
    FUNCPrint( f, files, schema );
    LISTod;
    LISTfree( function_list );

    // process each rule. This must be done *before* typedefs are defined
    LISTdo( rule_list, r, Rule );
    RULEPrint( r, files, schema );
    LISTod;
    LISTfree( rule_list );

}


void
PrintModelContentsSchema( Scope scope, FILES * files, Schema schema,
                          Express model ) {
    Linked_List list;
    char nm[BUFSIZ];
    DictionaryEntry de;

    //fprintf( files -> inc, "\n/*\t**************  TYPES  \t*/\n" );
    // Types should be exported to schema_name.DefinedDataTypes
    //fprintf( files -> lib, "\n/*\t**************  TYPES  \t*/\n" );
    //fprintf( files -> init, "\n/*\t**************  TYPES  \t*/\n" );

    /* do \'new\'s for types descriptors  */
    SCOPEdo_types( scope, t, de )
    //TYPEprint_new( t, files->create, schema );
    SCOPEod;

    /* do \'new\'s for entity descriptors  */
    list = SCOPEget_entities_superclass_order( scope );
    //fprintf( files->init, "\n\t//\t*****  Describe the Entities  \t*/\n" );
    fprintf( files->inc, "\n//\t***** Describe the Entities  \t\n" );
    LISTdo( list, e, Entity );
    ENTITYput_superclass( e ); /*  find supertype to use for single  */
    ENTITYprint_new( e, files, schema, 0 );         /*  inheritance  */
    LISTod;

    // Print Entity Classes
    LISTdo( list, e, Entity );
    ENTITYPrint( e, files, schema );
    LISTod;
    /*  fill in the values for the type descriptors */
    /*  and print the enumerations  */
    //fprintf(files->lib,"register_defined_types():\n");
    fprintf( files->inc, "\n//\t***** Describe the Other Types  \t\n" );
    SCOPEdo_types( scope, t, de )
    TYPEprint_descriptions( t, files, schema );
    if( TYPEis_select( t ) ) {
        /*   do the select aggregates here  */
        strncpy( nm, SelectName( TYPEget_name( t ) ), BUFSIZ );
        nm[BUFSIZ-1] = '\0';
        fprintf( files->inc, "class %s;\ntypedef %s * %sH;\n", nm, nm, nm );
        fprintf( files->inc,
                 "typedef %s * %s_ptr;\ntypedef %s_ptr %s_var;\n\n",
                 nm, nm, nm, nm );
        fprintf( files->inc, "class %ss;\ntypedef %ss * %ssH;\n", nm, nm, nm );
        fprintf( files->inc,
                 "typedef %ss * %ss_ptr;\ntypedef %ss_ptr %ss_var;\n\n",
                 nm, nm, nm, nm );
    }
    SCOPEod;

    /*  build the typedefs  */
    SCOPEdo_types( scope, t, de )
    if( !( TYPEis_select( t ) ) ) {
        TYPEprint_typedefs( t, files ->inc );
    }
    SCOPEod;

    /*  do the select definitions next, since they depend on the others  */
    fprintf( files->inc, "\n//\t***** Build the SELECT Types  \t\n" );
    //fprintf( files->init, "\n//\t***** Add the TypeDescriptor's to the"
    //         " SELECT Types  \t\n" );
    SCOPEdo_types( scope, t, de )
    if( TYPEis_select( t ) ) {
        TYPEselect_print( t, files, schema );
    }
    SCOPEod;

    LISTfree( list );
}



/******************************************************************
 ** Procedure:  SCHEMAprint
 ** Parameters: const Schema schema - schema to print
 **     FILES *file     - file on which to print
 **             Express model       - fedex rep of entire EXPRESS file
 **             ComplexCollect col  - all the complex entity info pertaining
 **                                   to this EXPRESS file
 **             int suffix          - suffix to use for generated cc files
 ** Returns:
 ** Description:  handles initialization of files specific to schemas
 ** Side Effects:
 ** Status:
 ******************************************************************/

void
SCHEMAprint( Schema schema, FILES * files, Express model, void * complexCol,
             int suffix ) {
    char schnm[MAX_LEN], sufnm[MAX_LEN], fnm[MAX_LEN], *np;
    /* sufnm = schema name + suffix */
    FILE * libfile;
    Rule r;
    Function f;
    Procedure p;
    DictionaryEntry de;
    /**********  create files based on name of schema   ***********/
    /*  return if failure           */
    /*  1.  header file             */
    //sprintf( schnm, "%s%s", SCHEMA_FILE_PREFIX,
    //         StrToUpper( SCHEMAget_name( schema ) ) );
    sprintf( schnm, "%s", SCHEMAget_name( schema ) );
    if( suffix == 0 ) {
        sprintf( sufnm, "%s", schnm );
    } else {
        sprintf( sufnm, "%s_%d", schnm, suffix );
    }
    sprintf( fnm, "%s.h", sufnm );

    np = fnm + strlen( fnm ) - 1; /*  point to end of constant part of string  */

    /*  2.  class source file            */
    sprintf( np, "py" );
    if( !( libfile = ( files -> lib ) = FILEcreate( fnm ) ) ) {
        return;
    }
    //fprintf( libfile, "/* %cId$  */ \n", '$' );
    fprintf( libfile, "import sys\n" );
    fprintf( libfile, "\n" );
    fprintf( libfile, "from SCL.SCLBase import *\n" );
    fprintf( libfile, "from SCL.SimpleDataTypes import *\n" );
    fprintf( libfile, "from SCL.ConstructedDataTypes import *\n" );
    fprintf( libfile, "from SCL.AggregationDataTypes import *\n" );
    fprintf( libfile, "from SCL.TypeChecker import check_type\n" );
    fprintf( libfile, "from SCL.Builtin import *\n" );
    fprintf( libfile, "from SCL.Rules import *\n" );

    /********* export schema name *******/
    fprintf( libfile, "\nschema_name = '%s'\n\n", SCHEMAget_name( schema ) );

    /******** export schema scope *******/
    fprintf( libfile, "schema_scope = sys.modules[__name__]\n\n" );


    /**********  do the schemas ***********/

    /* really, create calls for entity constructors */
    SCOPEPrint( schema, files, schema, model, ( ComplexCollect * )complexCol,
                suffix );

    /**********  close the files    ***********/
    FILEclose( libfile );
    //FILEclose( incfile );
    //if( schema->search_id == PROCESSED ) {
    //    fprintf( initfile, "\n}\n" );
    //    FILEclose( initfile );
    //} else {
    //    fclose( initfile );
    //}
}

/******************************************************************
 ** Procedure:  getMCPrint
 ** Parameters:
       Express express   - in memory representation of an express model
       FILE*   schema_h  - generated schema.h file
       FILE*   schema_cc - schema.cc file
 ** Returns:
 ** Description:  drives functions to generate code for all the schemas
 ** in an Express model into one set of files  -- works with
 **     print_schemas_combined
 ** Side Effects:  generates code
 ** Status:  24-Feb-1992 new -kcm
 ******************************************************************/
void
getMCPrint( Express express, FILE * schema_h, FILE * schema_cc ) {
    DictionaryEntry de;
    Schema schema;

    fprintf( schema_h,
             "\nSCLP23(Model_contents_ptr) GetModelContents(char *schemaName);\n" );
    fprintf( schema_cc, "%s%s%s%s",
             "// Generate a function to be called by Model to help it\n",
             "// create the necessary Model_contents without the\n",
             "// dictionary (Registry) handle since it doesn't have a\n",
             "// predetermined way to access to the handle.\n" );
    fprintf( schema_cc,
             "\nSCLP23(Model_contents_ptr) GetModelContents(char *schemaName)\n{\n" );
    DICTdo_type_init( express->symbol_table, &de, OBJ_SCHEMA );
    schema = ( Scope )DICTdo( &de );
    fprintf( schema_cc,
             "    if(!strcmp(schemaName, \"%s\"))\n",
             SCHEMAget_name( schema ) );
    fprintf( schema_cc,
             "        return (SCLP23(Model_contents_ptr)) new SdaiModel_contents_%s; \n",
             SCHEMAget_name( schema ) );
    while( ( schema = ( Scope )DICTdo( &de ) ) != 0 ) {
        fprintf( schema_cc,
                 "    else if(!strcmp(schemaName, \"%s\"))\n",
                 SCHEMAget_name( schema ) );
        fprintf( schema_cc,
                 "        return (SCLP23(Model_contents_ptr)) new SdaiModel_contents_%s; \n",
                 SCHEMAget_name( schema ) );
    }
    fprintf( schema_cc, "}\n" );
}

/******************************************************************
 ** Procedure:  EXPRESSPrint
 ** Parameters:
       Express express -- in memory representation of an express model
       FILES* files  -- set of output files to print to
 ** Returns:
 ** Description:  drives functions to generate code for all the schemas
 ** in an Express model into one set of files  -- works with
 **     print_schemas_combined
 ** Side Effects:  generates code
 ** Status:  24-Feb-1992 new -kcm
 ******************************************************************/
void
EXPRESSPrint( Express express, ComplexCollect & col, FILES * files ) {
    char fnm [MAX_LEN];
    const char  * schnm;  /* schnm is really "express name" */
    FILE * libfile;
    /* new */
    Schema schema;
    DictionaryEntry de;


    /**********  create files based on name of schema   ***********/
    /*  return if failure           */
    /*  1.  header file             */
    sprintf( fnm, "%s.h", schnm = ClassName( EXPRESSget_basename( express ) ) );
    //if( !( incfile = ( files -> inc ) = FILEcreate( fnm ) ) ) {
    //    return;
    //}
    //fprintf( incfile, "/* %cId$ */\n", '$' );

    //fprintf( incfile, "#ifdef __O3DB__\n" );
    //fprintf( incfile, "#include <OpenOODB.h>\n" );
    //fprintf( incfile, "#endif\n\n" );
    //fprintf( incfile, "#include <sdai.h> \n" );
    /*    fprintf (incfile, "#include <schema.h> \n");*/
    /*    fprintf (incfile, "extern void %sInit (Registry & r);\n", schnm);*/

    //np = fnm + strlen( fnm ) - 1; /*  point to end of constant part of string  */

    /*  2.  class source file            */
    //sprintf( np, "cc" );
    if( !( libfile = ( files -> lib ) = FILEcreate( fnm ) ) ) {
        return;
    }
    //fprintf( libfile, "/* %cId$ */\n", '$' );
    //fprintf( libfile, "#include <%s.h> n", schnm );

    /*  3.  source code to initialize entity registry   */
    /*  prints header of file for input function    */

    //sprintf( np, "init.cc" );
    //if( !( initfile = ( files -> init ) = FILEcreate( fnm ) ) ) {
    //    return;
    //}
    //fprintf( initfile, "/* $Id%d */\n", '$' );
    //fprintf( initfile, "#include <%s.h>\n\n", schnm );
    //fprintf( initfile, "void \n%sInit (Registry& reg)\n{\n", schnm );

    /**********  record in files relating to entire input   ***********/

    /*  add to schema's include and initialization file */
    //fprintf( schemafile, "#include <%s.h>\n\n", schnm );
    //fprintf( schemafile, "extern void %sInit (Registry & r);\n", schnm );
    //fprintf( schemainit, "\t extern void %sInit (Registry & r);\n", schnm );
    //fprintf( schemainit, "\t %sInit (reg);\n", schnm );

    /**********  do all schemas ***********/
    DICTdo_init( express->symbol_table, &de );
    while( 0 != ( schema = ( Scope )DICTdo( &de ) ) ) {
        SCOPEPrint( schema, files, schema, express, &col, 0 );
    }


    /**********  close the files    ***********/
    FILEclose( libfile );
    //FILEclose( incfile );
    //fprintf( initfile, "\n}\n" );
    //FILEclose( initfile );

}

/******************************************************************
 ** Procedure:  print_schemas_combined
 ** Parameters:
       Express express -- in memory representation of an express model
       FILES* files  -- set of output files to print to
 ** Returns:
 ** Description:  drives functions to generate code for all the schemas
 ** in an Express model into one set of files  -- works with EXPRESSPrint
 ** Side Effects:  generates code
 ** Status:  24-Feb-1992 new -kcm
 ******************************************************************/

void
print_schemas_combined( Express express, ComplexCollect & col, FILES * files ) {

    EXPRESSPrint( express, col, files );
}

/*
** Procedure:   print_file
** Parameters:  const Schema schema - top-level schema to print
**      FILE*        file   - file on which to print
** Returns: void
** Description:  this function calls one of two different functions
**  depending on whether the output should be combined into a single
**  set of files or a separate set for each schema
**
*/

void
print_file( Express express ) {
    extern void RESOLUTIONsucceed( void );
    int separate_schemas = 1;
    ComplexCollect col( express );

    File_holder files;

    resolution_success();

    print_file_header( express, &files );
    if( separate_schemas ) {
        print_schemas_separate( express, ( void * )&col, &files );
    } else {
        print_schemas_combined( express, col, &files );
    }
    print_file_trailer( express, &files );
    //print_complex( col, ( const char * )"compstructs.cc" );
}
