#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "complexSupport.h"

void use_ref(Schema, Express, FILES *);

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

void SCOPEPrint(Scope scope, FILES *files, Schema schema)
{
    Linked_List list = SCOPEget_entities_superclass_order(scope);
    Linked_List function_list = SCOPEget_functions(scope);
    Linked_List rule_list = SCOPEget_rules(scope);
    DictionaryEntry de;
    Type i;
    int redefs = 0;// index = 0;

    /* Defined Types based on SIMPLE types */
    SCOPEdo_types(scope, t, de)
    if((t->search_id == CANPROCESS)
            && !(TYPEis_enumeration(t) || TYPEis_select(t) || TYPEis_aggregate(t))
            && (TYPEget_ancestor(t) == NULL)) {
        TYPEprint_descriptions(t, files, schema);
        t->search_id = PROCESSED;
    }
    SCOPEod

    /* Defined Types with defined ancestor head
     * TODO: recursive approach
     */
    SCOPEdo_types(scope, t, de)
    if((t->search_id == CANPROCESS)
            && !(TYPEis_enumeration(t) || TYPEis_select(t) || TYPEis_aggregate(t))
            && ((i = TYPEget_head(t)) != NULL)) {
        if(i->search_id == PROCESSED) {
            TYPEprint_descriptions(t, files, schema);
            t->search_id = PROCESSED;
        }
    }
    SCOPEod

    /* fill in the values for the type descriptors */
    /* and print the enumerations */
    //fprintf( files -> inc, "\n/*\t**************  TYPES  \t*/\n" );
    //fprintf( files -> lib, "\n/*\t**************  TYPES  \t*/\n" );
    SCOPEdo_types(scope, t, de)
    // First check for one exception:  Say enumeration type B is defined
    // to be a rename of enum A.  If A is in this schema but has not been
    // processed yet, we must wait till it's processed first.  The reason
    // is because B will basically be defined with a couple of typedefs to
    // the classes which represent A.  (To simplify, we wait even if A is
    // in another schema, so long as it's been processed.)
    if((t->search_id == CANPROCESS)
            && (TYPEis_enumeration(t))
            && ((i = TYPEget_ancestor(t)) != NULL)
            && (i->search_id >= CANPROCESS)) {
        redefs = 1;
    }
    SCOPEod

    SCOPEdo_types(scope, t, de)
    // Do the non-redefined enumerations:
    if((t->search_id == CANPROCESS)
            && !(TYPEis_enumeration(t) && TYPEget_head(t))) {
        TYPEprint_descriptions(t, files, schema);
        if(!TYPEis_select(t)) {
            // Selects have a lot more processing and are done below.
            t->search_id = PROCESSED;
        }
    }
    SCOPEod;

    // process redifined enumerations
    if(redefs) {
        SCOPEdo_types(scope, t, de)
        if(t->search_id == CANPROCESS && TYPEis_enumeration(t)) {
            TYPEprint_descriptions(t, files, schema);
            t->search_id = PROCESSED;
        }
        SCOPEod;
    }

    /*  do the select definitions next, since they depend on the others  */
    //fprintf( files->inc, "\n//\t***** Build the SELECT Types  \t\n" );
    // Note - say we have sel B, rename of sel A (as above by enum's).  Here
    // we don't have to worry about printing B before A.  This is checked in
    // TYPEselect_print().
    SCOPEdo_types(scope, t, de)
    if(t->search_id == CANPROCESS) {
        // Only selects haven't been processed yet and may still be set to
        // CANPROCESS.
        //FIXME this function is not implemented!
//         TYPEselect_print( t, files, schema );
        t->search_id = PROCESSED;
    }
    SCOPEod;

    // process each entity. This must be done *before* typedefs are defined
    LISTdo(list, e, Entity);
    if(e->search_id == CANPROCESS) {
        ENTITYPrint(e, files);
        e->search_id = PROCESSED;
    }
    LISTod;
    LISTfree(list);

    // process each function. This must be done *before* typedefs are defined
    LISTdo(function_list, f, Function);
    FUNCPrint(f, files);
    LISTod;
    LISTfree(function_list);

    // process each rule. This must be done *before* typedefs are defined
    LISTdo(rule_list, r, Rule);
    RULEPrint(r, files);
    LISTod;
    LISTfree(rule_list);

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

void SCHEMAprint(Schema schema, FILES *files, int suffix)
{
    int ocnt = 0;
    char schnm[MAX_LEN], sufnm[MAX_LEN], fnm[MAX_LEN], *np;
    /* sufnm = schema name + suffix */
    FILE *libfile;
    /**********  create files based on name of schema   ***********/
    /*  return if failure           */
    /*  1.  header file             */
    sprintf(schnm, "%s", SCHEMAget_name(schema));
    if(suffix == 0) {
        sprintf(sufnm, "%s", schnm);
    } else {
        ocnt = snprintf(sufnm, MAX_LEN, "%s_%d", schnm, suffix);
        if(ocnt > MAX_LEN) {
            std::cerr << "Warning - classes_wrapper_python.cc - sufnm not large enough to hold string\n";
        }
    }
    ocnt = snprintf(fnm, MAX_LEN, "%s.h", sufnm);
    if(ocnt > MAX_LEN) {
        std::cerr << "Warning - classes_wrapper_python.cc - fnm not large enough to hold string\n";
    }

    np = fnm + strlen(fnm) - 1;   /*  point to end of constant part of string  */

    /*  2.  class source file            */
    sprintf(np, "py");
    if(!(libfile = (files -> lib) = FILEcreate(fnm))) {
        return;
    }
    fprintf(libfile, "import sys\n");
    fprintf(libfile, "\n");
    fprintf(libfile, "from SCL.SCLBase import *\n");
    fprintf(libfile, "from SCL.SimpleDataTypes import *\n");
    fprintf(libfile, "from SCL.ConstructedDataTypes import *\n");
    fprintf(libfile, "from SCL.AggregationDataTypes import *\n");
    fprintf(libfile, "from SCL.TypeChecker import check_type\n");
    fprintf(libfile, "from SCL.Builtin import *\n");
    fprintf(libfile, "from SCL.Rules import *\n");

    /********* export schema name *******/
    fprintf(libfile, "\nschema_name = '%s'\n\n", SCHEMAget_name(schema));

    /******** export schema scope *******/
    fprintf(libfile, "schema_scope = sys.modules[__name__]\n\n");


    /**********  do the schemas ***********/

    /* really, create calls for entity constructors */
    SCOPEPrint(schema, files, schema);

    /**********  close the files    ***********/
    FILEclose(libfile);
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
getMCPrint(Express express, FILE *schema_h, FILE *schema_cc)
{
    DictionaryEntry de;
    Schema schema;

    fprintf(schema_h,
            "\nSCLP23(Model_contents_ptr) GetModelContents(char *schemaName);\n");
    fprintf(schema_cc, "%s%s%s%s",
            "// Generate a function to be called by Model to help it\n",
            "// create the necessary Model_contents without the\n",
            "// dictionary (Registry) handle since it doesn't have a\n",
            "// predetermined way to access to the handle.\n");
    fprintf(schema_cc,
            "\nSCLP23(Model_contents_ptr) GetModelContents(char *schemaName)\n{\n");
    DICTdo_type_init(express->symbol_table, &de, OBJ_SCHEMA);
    schema = (Scope)DICTdo(&de);
    fprintf(schema_cc,
            "    if(!strcmp(schemaName, \"%s\"))\n",
            SCHEMAget_name(schema));
    fprintf(schema_cc,
            "        return (SCLP23(Model_contents_ptr)) new SdaiModel_contents_%s; \n",
            SCHEMAget_name(schema));
    while((schema = (Scope)DICTdo(&de)) != 0) {
        fprintf(schema_cc,
                "    else if(!strcmp(schemaName, \"%s\"))\n",
                SCHEMAget_name(schema));
        fprintf(schema_cc,
                "        return (SCLP23(Model_contents_ptr)) new SdaiModel_contents_%s; \n",
                SCHEMAget_name(schema));
    }
    fprintf(schema_cc, "}\n");
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
EXPRESSPrint(Express express, FILES *files)
{
    char fnm [MAX_LEN];
    const char   *schnm;  /* schnm is really "express name" */
    FILE *libfile;
    /* new */
    Schema schema;
    DictionaryEntry de;


    /**********  create files based on name of schema   ***********/
    /*  return if failure           */
    /*  1.  header file             */
    sprintf(fnm, "%s.h", schnm = ClassName(EXPRESSget_basename(express)));

    /*  2.  class source file            */
    //sprintf( np, "cc" );
    if(!(libfile = (files -> lib) = FILEcreate(fnm))) {
        return;
    }

    /**********  do all schemas ***********/
    DICTdo_init(express->symbol_table, &de);
    while(0 != (schema = (Scope)DICTdo(&de))) {
        SCOPEPrint(schema, files, schema);
    }


    /**********  close the files    ***********/
    FILEclose(libfile);
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
print_schemas_combined(Express express, FILES *files)
{

    EXPRESSPrint(express, files);
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
print_file(Express express)
{
    extern void RESOLUTIONsucceed(void);
    int separate_schemas = 1;

    File_holder files;

    resolution_success();

    if(separate_schemas) {
        print_schemas_separate(express, &files);
    } else {
        print_schemas_combined(express, &files);
    }
}
