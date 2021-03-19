#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "complexSupport.h"
#include "class_strings.h"
#include <sc_memmgr.h>

#include <sc_trace_fprintf.h>

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

/*****************************************************************
***   The functions in this file drive the processing of an     **
***   EXPRESS file.                                             **
 **                             **/


void use_ref(Schema, Express, FILES *);

void create_builtin_type_decl(FILES *files, char *name)
{
    fprintf(files->incall, "extern SC_%s_EXPORT TypeDescriptor *%s%s_TYPE;\n",
            "SCHEMA", TD_PREFIX, name);
}

void create_builtin_type_defn(FILES *files, char *name)
{
    fprintf(files->initall, "        %s%s_TYPE = new TypeDescriptor (",
            TD_PREFIX, name);
    fprintf(files->initall, "\"%s\", %s_TYPE, \"%s\");\n",
            PrettyTmpName(name), StrToUpper(name), StrToLower(name));
}

/** ****************************************************************
 ** Procedure:  print_file_header
 ** Parameters: const Schema schema - top-level schema being printed
 **     FILE*        file   - file on which to print header
 ** Returns:
 ** Description:  handles file related tasks that need to be done once
 ** at the beginning of processing.
 ** In this case the file schema.h is initiated
 ** Status:  ok 1/15/91
 ******************************************************************/
void print_file_header(FILES *files)
{

    /*  open file which unifies all schema specific header files
    of input Express source */
    files -> incall = FILEcreate("schema.h");
    fprintf(files->incall, "\n// in the exp2cxx source code, this file is generally referred to as files->incall or schemafile\n");

    fprintf(files->incall, "\n#if !defined(SC_STATIC) && defined(_WIN32)\n");
    fprintf(files->incall, "# if defined(SC_SCHEMA_DLL_EXPORTS)\n");
    fprintf(files->incall, "#  define SC_SCHEMA_EXPORT __declspec(dllexport)\n");
    fprintf(files->incall, "# else\n");
    fprintf(files->incall, "#  define SC_SCHEMA_EXPORT __declspec(dllimport)\n");
    fprintf(files->incall, "# endif\n");
    fprintf(files->incall, "#else\n");
    fprintf(files->incall, "# define SC_SCHEMA_EXPORT\n");
    fprintf(files->incall, "#endif\n\n");

    fprintf(files->incall, "#ifdef SC_LOGGING\n");
    fprintf(files->incall, "#include <sys/time.h>\n");
    fprintf(files->incall, "#endif\n");

    fprintf(files->incall, "#include <sdai.h>\n\n");
    fprintf(files->incall, "\n#include <Registry.h>\n");
    fprintf(files->incall, "\n#include <STEPaggregate.h>\n");
    fprintf(files->incall, "\n#include <STEPundefined.h>\n");
    fprintf(files->incall, "\n#include <ExpDict.h>\n");
    fprintf(files->incall, "\n#include <STEPattribute.h>\n");

    fprintf(files->incall, "\n#include <Sdaiclasses.h>\n");

    fprintf(files->incall, "extern SC_%s_EXPORT void SchemaInit (Registry &);\n", "SCHEMA");
    fprintf(files->incall, "extern SC_%s_EXPORT void InitSchemasAndEnts (Registry &);\n", "SCHEMA");

    files -> initall = FILEcreate("schema.cc");
    fprintf(files->initall, "\n// in the exp2cxx source code, this file is generally referred to as files->initall or schemainit\n");
    fprintf(files->initall, "#include \"schema.h\"\n");
    fprintf(files->initall, "#include \"sc_memmgr.h\"\n");
    fprintf(files->initall, "class Registry;\n");

    fprintf(files->initall, "\nvoid SchemaInit (Registry & reg) {\n");
    fprintf(files->initall, "     extern void InitSchemasAndEnts ");
    fprintf(files->initall, "(Registry & r);\n");
    fprintf(files->initall, "     InitSchemasAndEnts (reg);\n");

    // This file will contain instantiation statements for all the schemas and
    // entities in the express file.  (They must all be in separate function
    // called first by SchemaInit() so that all entities will exist
    files -> create = FILEcreate("SdaiAll.cc");
    fprintf(files->create, "\n// in the exp2cxx source code, this file is generally referred to as files->create or createall\n");
    fprintf(files->create, "#include \"schema.h\"\n");
    fprintf(files->create, "#include \"sc_memmgr.h\"\n");
    fprintf(files->create, "\nvoid InitSchemasAndEnts (Registry & reg) {\n");

    // This file declares all entity classes as incomplete types.  This will
    // allow all the .h files to reference all .h's.  We can then have e.g.,
    // entX from schemaA have attribute attr1 = entY from schemaB.
    files -> classes = FILEcreate("Sdaiclasses.h");
    fprintf(files->classes, "\n// in the exp2cxx source code, this file is generally referred to as files->classes\n");
    fprintf(files->classes, "#include \"schema.h\"\n");
}

/** ****************************************************************
 ** Procedure:  print_file_trailer
 ** Parameters: const Schema schema - top-level schema printed
 **     FILE*        file   - file on which to print trailer
 ** Returns:
 ** Description:  handles cleaning up things at end of processing
 ** Status:  ok 1/15/91
 ******************************************************************/
void print_file_trailer(FILES *files)
{
    FILEclose(files->incall);
    FILEclose(files->initall);
    fprintf(files->create, "}\n\n");
    FILEclose(files->create);
    fprintf(files->classes, "\n");
    FILEclose(files->classes);
    fprintf(files->names, "\n}\n");
    FILEclose(files->names);
}

/* set attribute index to simplify attrdescriptor name calculation
 *  and reduce/eliminate use of global attr_count
 */
void numberAttributes(Scope scope)
{
    int count = 0;
    Linked_List list = SCOPEget_entities_superclass_order(scope);
    LISTdo(list, e, Entity) {
        LISTdo_n(ENTITYget_attributes(e), v, Variable, b) {
            v->idx = count++;
        }
        LISTod
    }
    LISTod
}

/******************************************************************
 **  SCHEMA SECTION                      **/

/** ****************************************************************
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
void SCOPEPrint(Scope scope, FILES *files, Schema schema, ComplexCollect *col, int cnt)
{
    Linked_List list = SCOPEget_entities_superclass_order(scope);
    DictionaryEntry de;
    Type i;
    int redefs = 0;

    if(cnt <= 1) {
        /* This will be the case if this is the first time we are generating a
        ** file for this schema.  (cnt = the file suffix.  If it = 1, it's the
        ** first of multiple; if it = 0, it's the only one.)  Basically, this
        ** if generates all the files which are not divided (into _1, _2 ...)
        ** during multiple passes.  This includes Sdaiclasses.h, SdaiAll.cc,
        ** and the Sdaixxx.init.cc files. */

        fprintf(files -> lib, "\nSchema * %s::schema = 0;\n", SCHEMAget_name(schema));

        /* Do \'new\'s for types descriptors (in SdaiAll.cc (files->create)),
           and the externs typedefs, and incomplete descriptors (in Sdai-
           classes.h (files->classes)). */
        fprintf(files->create, "\n  //  *****  Initialize the Types\n");
        fprintf(files->classes, "\n// Types:\n");
        SCOPEdo_types(scope, t, de) {
            //TYPEprint_new moved to TYPEPrint_cc and TYPEprint_descriptions in classes_type.c
            TYPEprint_typedefs(t, files->classes);
            //print in namespace. Some logic copied from TypeDescriptorName()
            fprintf(files->names, "    extern SC_SCHEMA_EXPORT %s * %s%s;\n", GetTypeDescriptorName(t), TYPEprefix(t), TYPEget_name(t));
        }
        SCOPEod

        fprintf(files->classes, "\n// Entity class typedefs:");
        LISTdo(list, e, Entity) {
            ENTITYprint_classes(e, files->classes);
        }
        LISTod
    }

    /* fill in the values for the type descriptors and print the enumerations */
    fprintf(files -> inc, "\n/*    **************  TYPES      */\n");
    fprintf(files -> lib, "\n/*    **************  TYPES      */\n");
    /* The following was `SCOPEdo_types( scope, t, de ) ... SCOPEod;`
     * Modified Jan 2012 by MAP - moving enums to own dictionary */
    if(scope->enum_table) {
        HASHlistinit_by_type(scope->enum_table, &de, OBJ_TYPE);
        Type t;
        while(0 != (t = (Type) DICTdo(&de))) {
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
        }
    }

    SCOPEdo_types(scope, t, de) {
        /* NOTE the following comment seems to contradict the logic below it (... && !( TYPEis_enumeration( t ) && ...)
        // Do the non-redefined enumerations:*/
        if((t->search_id == CANPROCESS)
                && !(TYPEis_enumeration(t) && TYPEget_head(t))) {
            TYPEprint_descriptions(t, files, schema);
            if(!TYPEis_select(t)) {
                // Selects have a lot more processing and are done below.
                t->search_id = PROCESSED;
            }
        }
    }
    SCOPEod

    if(redefs) {
        // Here we process redefined enumerations.  See note, 2 loops ago.
        fprintf(files->inc, "//    ***** Redefined Enumerations:\n");
        /* The following was `SCOPEdo_types( scope, t, de ) ... SCOPEod;`
        * Modified Jan 2012 by MAP - moving enums to own dictionary */
        HASHlistinit_by_type(scope->enum_table, &de, OBJ_TYPE);
        Type t;
        while(0 != (t = (Type) DICTdo(&de))) {
            if(t->search_id == CANPROCESS && TYPEis_enumeration(t)) {
                TYPEprint_descriptions(t, files, schema);
                t->search_id = PROCESSED;
            }
        }
    }

    /*  do the select definitions next, since they depend on the others  */
    fprintf(files->inc, "\n//        ***** Build the SELECT Types          \n");
    // Note - say we have sel B, rename of sel A (as above by enum's).  Here
    // we don't have to worry about printing B before A.  This is checked in
    // TYPEselect_print().
    SCOPEdo_types(scope, t, de) {
        if(t->search_id == CANPROCESS) {
            // Only selects haven't been processed yet and may still be set to
            // CANPROCESS.
            if(TYPEis_select(t)) {
                TYPEselect_print(t, files, schema);
            }
            if(TYPEis_enumeration(t)) {
                TYPEprint_descriptions(t, files, schema);
            }

            t->search_id = PROCESSED;
        }
    }
    SCOPEod

    fprintf(files -> inc, "\n/*        **************  ENTITIES          */\n");
    fprintf(files -> lib, "\n/*        **************  ENTITIES          */\n");

    fprintf(files->inc, "\n//        ***** Print Entity Classes          \n");
    LISTdo(list, e, Entity) {
        if(e->search_id == CANPROCESS) {
            ENTITYPrint(e, files, schema, col->externMapping(ENTITYget_name(e)));
            e->search_id = PROCESSED;
        }
    }
    LISTod

    if(cnt <= 1) {
        int index = 0;

        // Do the model stuff:
        fprintf(files->inc, "\n//        ***** generate Model related pieces\n");
        fprintf(files->inc, "\nclass SdaiModel_contents_%s : public SDAI_Model_contents {\n", SCHEMAget_name(schema));
        fprintf(files -> inc, "\n  public:\n");
        fprintf(files -> inc, "    SdaiModel_contents_%s();\n", SCHEMAget_name(schema));
        LISTdo(list, e, Entity) {
            MODELprint_new(e, files);
        }
        LISTod

        fprintf(files->inc, "\n};\n\n");

        fprintf(files->inc, "typedef       SdaiModel_contents_%s *     SdaiModel_contents_%s_ptr;\n", SCHEMAget_name(schema), SCHEMAget_name(schema));
        fprintf(files->inc, "typedef const SdaiModel_contents_%s *     SdaiModel_contents_%s_ptr_c;\n", SCHEMAget_name(schema), SCHEMAget_name(schema));
        fprintf(files->inc, "typedef       SdaiModel_contents_%s_ptr   SdaiModel_contents_%s_var;\n", SCHEMAget_name(schema), SCHEMAget_name(schema));
        fprintf(files->inc, "SDAI_Model_contents_ptr create_SdaiModel_contents_%s();\n", SCHEMAget_name(schema));

        fprintf(files->lib, "\nSDAI_Model_contents_ptr create_SdaiModel_contents_%s() {\n", SCHEMAget_name(schema));
        fprintf(files->lib, "    return new SdaiModel_contents_%s;\n}\n", SCHEMAget_name(schema));

        fprintf(files->lib, "\nSdaiModel_contents_%s::SdaiModel_contents_%s() {\n", SCHEMAget_name(schema), SCHEMAget_name(schema));
        fprintf(files->lib, "    SDAI_Entity_extent_ptr eep = (SDAI_Entity_extent_ptr)0;\n\n");
        LISTdo(list, e, Entity) {
            MODELPrintConstructorBody(e, files, schema);
        }
        LISTod
        fprintf(files -> lib, "}\n");
        index = 0;
        LISTdo(list, e, Entity) {
            MODELPrint(e, files, schema, index);
            index++;
        }
        LISTod
    }

    LISTfree(list);
}

/** open/init unity files which allow faster compilation with fewer translation units */
void initUnityFiles(const char *schName, FILES *files)
{
    const char *unity = "\n/** this file is for unity builds, which allow faster compilation\n"
                        " * with fewer translation units. not compatible with all compilers!\n */\n\n"
                        "#include \"schema.h\"\n";
    std::string name = schName;
    name.append("_unity_");
    size_t prefixLen = name.length();

    name.append("entities.cc");
    files->unity.entity.impl = FILEcreate(name.c_str());

    name.resize(name.length() - 2);
    name.append("h");
    fprintf(files->unity.entity.impl, "%s#include \"%s\"\n", unity, name.c_str());

    files->unity.entity.hdr = FILEcreate(name.c_str());
    fprintf(files->unity.entity.hdr, "%s\n", unity);

    name.resize(prefixLen);
    name.append("types.cc");
    files->unity.type.impl = FILEcreate(name.c_str());

    name.resize(name.length() - 2);
    name.append("h");
    fprintf(files->unity.type.impl, "%s#include \"%s\"\n", unity, name.c_str());

    files->unity.type.hdr = FILEcreate(name.c_str());
    fprintf(files->unity.type.hdr, "%s\n", unity);
}

/** close unity files
 * \sa initUnityFiles()
 */
void closeUnityFiles(FILES *files)
{
    FILEclose(files->unity.type.hdr);
    FILEclose(files->unity.type.impl);
    FILEclose(files->unity.entity.hdr);
    FILEclose(files->unity.entity.impl);
}

///write tail of initfile, close it
void INITFileFinish(FILE *initfile, Schema schema)
{
    fprintf(initfile, "\n    /* loop through any entities with inverse attrs, calling InitIAttrs */\n");
    fprintf(initfile, "    EntityDescItr edi( *%s::schema->EntsWInverse() );\n", SCHEMAget_name(schema));
    fprintf(initfile, "    EntityDescriptor * ed;\n");
    fprintf(initfile, "    const char * nm = %s::schema->Name();\n", SCHEMAget_name(schema));
    fprintf(initfile, "    while( 0 != ( ed = edi.NextEntityDesc_nc() ) ) {\n");
    fprintf(initfile, "        ed->InitIAttrs( reg, nm );\n");
    fprintf(initfile, "    }\n}\n");
    FILEclose(initfile);
}

/** ****************************************************************
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
void SCHEMAprint(Schema schema, FILES *files, void *complexCol, int suffix)
{
    int ocnt = 0;
    char schnm[MAX_LEN], sufnm[MAX_LEN], fnm[MAX_LEN], *np;
    /* sufnm = schema name + suffix */
    FILE *libfile,
         * incfile,
         * schemafile = files->incall,
           * schemainit = files->initall,
             * initfile,
             * createall = files->create;
    Rule r;
    Function f;
    Procedure p;
    DictionaryEntry de;
    /**********  create files based on name of schema   ***********/
    /*  return if failure           */
    /*  1.  header file             */
    sprintf(schnm, "%s%s", SCHEMA_FILE_PREFIX, StrToUpper(SCHEMAget_name(schema)));       //TODO change file names to CamelCase?
    if(suffix == 0) {
        ocnt = snprintf(sufnm, MAX_LEN, "%s", schnm);
        if(ocnt > MAX_LEN) {
            std::cerr << "Warning - classes_wrapper.cc line 425 - sufnm not large enough to hold schnm\n";
        }
    } else {
        ocnt = snprintf(sufnm, MAX_LEN, "%s_%d", schnm, suffix);
        if(ocnt > MAX_LEN) {
            std::cerr << "Warning - classes_wrapper.cc line 430 - sufnm not large enough to hold string\n";
        }
    }
    ocnt = snprintf(fnm, MAX_LEN, "%s.h", sufnm);
    if(ocnt > MAX_LEN) {
        std::cerr << "Warning - classes_wrapper.cc line 436 - sufnm not large enough to hold string\n";
    }

    if(!(incfile = (files -> inc) = FILEcreate(fnm))) {
        return;
    }
    fprintf(files->inc, "\n// in the exp2cxx source code, this file is generally referred to as files->inc or incfile\n");

    fprintf(incfile, "#include \"schema.h\"\n");
    fprintf(incfile, "#include \"sc_memmgr.h\"\n");

    np = fnm + strlen(fnm) - 1;   /*  point to end of constant part of string  */

    /* 1.9 open/init unity files which allow faster compilation with fewer translation units */
    initUnityFiles(sufnm, files);
    /*  2.  class source file            */
    sprintf(np, "cc");
    if(!(libfile = (files -> lib) = FILEcreate(fnm))) {
        return;
    }
    fprintf(files->lib, "\n// in the exp2cxx source code, this file is generally referred to as files->lib or libfile\n");

//TODO: Looks like this switches between 'schema.h' and a non-generic name. What is that name,
//and how do we fully enable this feature (i.e. how to write the file with different name)?
#ifdef SCHEMA_HANDLING
    sprintf(np, "h");
    fprintf(libfile, "#include <%s.h> \n", sufnm);
#else
    fprintf(libfile, "#include \"schema.h\"\n");
#endif
    fprintf(libfile, "#include \"sc_memmgr.h\"\n");

    fprintf(libfile,
            "\n#ifdef  SC_LOGGING \n"
            "#include <fstream.h>\n"
            "    extern ofstream *logStream;\n"
            "#define SCLLOGFILE \"scl.log\"\n"
            "#endif \n");

    fprintf(libfile, "\n#include \"%s.h\"\n", schnm);

    // 3. header for namespace to contain all formerly-global variables
    ocnt = snprintf(fnm, MAX_LEN, "%sNames.h", schnm);
    if(ocnt > MAX_LEN) {
        std::cerr << "Warning - classes_wrapper.cc line 480 - fnm not large enough to hold schnm\n";
    }

    if(!(files->names = FILEcreate(fnm))) {
        return;
    }
    fprintf(libfile, "#include \"%sNames.h\"\n", schnm);
    fprintf(files->names, "\n// In the exp2cxx source code, this file is referred to as files->names.\n// This line printed at %s:%d (one of two possible locations).\n\n", __FILE__, __LINE__);
    fprintf(files->names, "//this file contains a namespace for all formerly-global variables\n\n");
    fprintf(files->names, "namespace %s {\n\n", SCHEMAget_name(schema));
    fprintf(files->names, "    extern Schema * schema;\n\n");

    /*  4.  source code to initialize entity registry   */
    /*  prints header of file for input function    */

    if(suffix <= 1) {
        /* I.e., if this is our first pass with schema */
        ocnt = snprintf(fnm, MAX_LEN, "%s.init.cc", schnm);
        if(ocnt > MAX_LEN) {
            std::cerr << "Warning - classes_wrapper.cc line 499 - fnm not large enough to hold string\n";
        }

        /* Note - We use schnm (without the "_x" suffix sufnm has) since we
        ** only generate a single init.cc file. */
        if(!(initfile = (files -> init) = FILEcreate(fnm))) {
            return;
        }
        fprintf(files->init, "\n// in the exp2cxx source code, this file is generally referred to as files->init or initfile\n");
#ifdef SCHEMA_HANDLING
        if(suffix == 0) {
            fprintf(initfile, "#include <%s.h>\n", schnm);
        } else {
            fprintf(initfile, "#include <%s_%d.h>\n", schnm, suffix);
        }
#else
        fprintf(initfile,
                "#ifndef  SCHEMA_H\n"
                "#include \"schema.h\"\n"
                "#endif\n");
#endif
        fprintf(initfile, "#include <Registry.h>\n#include <string>\n");
        fprintf(initfile, "#include <sc_memmgr.h>\n");

        fprintf(initfile, "\nvoid %sInit (Registry& reg) {\n", schnm);

        fprintf(createall, "// Schema:  %s\n", schnm);
        fprintf(createall, "    %s::schema = new Schema(\"%s\");\n", SCHEMAget_name(schema), PrettyTmpName(SCHEMAget_name(schema)));

        /* Add the SdaiModel_contents_<schema_name> class constructor to the
           schema descriptor create function for it */
        fprintf(createall, "    %s::schema->AssignModelContentsCreator( (ModelContentsCreator) create_SdaiModel_contents_%s);\n",
                SCHEMAget_name(schema), SCHEMAget_name(schema));

        fprintf(createall, "    reg.AddSchema (*%s::schema);\n", SCHEMAget_name(schema));
        /**************/
        /* add global RULEs to Schema dictionary entry */
        DICTdo_type_init(schema->symbol_table, &de, OBJ_RULE);
        while(0 != (r = (Rule)DICTdo(&de))) {
            fprintf(createall, "    str.clear();\n");
            format_for_std_stringout(createall, RULEto_string(r));
            fprintf(createall, "gr = new Global_rule(\"%s\",%s::schema, str );\n", r->symbol.name, SCHEMAget_name(schema));
            fprintf(createall, "%s::schema->AddGlobal_rule(gr);\n", SCHEMAget_name(schema));
        }
        /**************/
        /* add FUNCTIONs to Schema dictionary entry */
        DICTdo_type_init(schema->symbol_table, &de, OBJ_FUNCTION);
        while(0 != (f = (Function)DICTdo(&de))) {
            fprintf(createall, "    str.clear();\n");
            format_for_std_stringout(createall, FUNCto_string(f));
            fprintf(createall, "%s::schema->AddFunction( str );\n", SCHEMAget_name(schema));
        }

        /* add PROCEDUREs to Schema dictionary entry */
        DICTdo_type_init(schema->symbol_table, &de, OBJ_PROCEDURE);
        while(0 != (p = (Procedure)DICTdo(&de))) {
            fprintf(createall, "    str.clear();\n");
            format_for_std_stringout(createall, PROCto_string(p));
            fprintf(createall, "%s::schema->AddProcedure( str );\n", SCHEMAget_name(schema));
        }

        fprintf(files->classes, "\n// Schema:  %s", schnm);
        fprintf(files->classes, "\n#include \"%sNames.h\"\n", schnm);
    } else {
        /* Just reopen the .init.cc (in append mode): */
        ocnt = snprintf(fnm, MAX_LEN, "%s.init.cc", schnm);
        if(ocnt > MAX_LEN) {
            std::cerr << "Warning - classes_wrapper.cc line 558 - sufnm not large enough to hold string\n";
        }

        initfile = files->init = fopen(fnm, "a");
    }

    /**********  record in files relating to entire input   ***********/

    /*  add to schema's include and initialization file */
    fprintf(schemafile, "#include \"%sNames.h\"\n", schnm);
    fprintf(schemafile, "#include \"%s.h\" \n", sufnm);
    if(schema->search_id == PROCESSED) {
        fprintf(schemafile, "extern void %sInit (Registry & r);\n", schnm);
        fprintf(schemainit, "     extern void %sInit (Registry & r);\n", schnm);
        fprintf(schemainit, "     %sInit (reg); \n", schnm);
    }

    /**********  do the schemas ***********/

    /* really, create calls for entity constructors */
    SCOPEPrint(schema, files, schema, (ComplexCollect *)complexCol, suffix);


    /**********  close the files    ***********/
    closeUnityFiles(files);
    FILEclose(libfile);
    FILEclose(incfile);
    if(schema->search_id == PROCESSED) {
        INITFileFinish(initfile, schema);
    } else {
        fclose(initfile);
    }
}

/** ****************************************************************
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
void getMCPrint(Express express, FILE *schema_h, FILE *schema_cc)
{
    DictionaryEntry de;
    Schema schema;

    fprintf(schema_h, "\nSDAI_Model_contents_ptr GetModelContents(char *schemaName);\n");
    fprintf(schema_cc, "/*    Generated at %s:%d.    */\n\n", __FILE__, __LINE__);
    fprintf(schema_cc, "%s%s%s%s",
            "// Generate a function to be called by Model to help it\n",
            "// create the necessary Model_contents without the\n",
            "// dictionary (Registry) handle since it doesn't have a\n",
            "// predetermined way to access to the handle.\n");
    fprintf(schema_cc, "\nSDAI_Model_contents_ptr GetModelContents(char *schemaName) {\n");
    DICTdo_type_init(express->symbol_table, &de, OBJ_SCHEMA);
    schema = (Scope)DICTdo(&de);
    fprintf(schema_cc, "    if(!strcmp(schemaName, \"%s\"))\n", SCHEMAget_name(schema));
    fprintf(schema_cc, "        return (SDAI_Model_contents_ptr) new SdaiModel_contents_%s; \n", SCHEMAget_name(schema));
    while((schema = (Scope)DICTdo(&de)) != 0) {
        fprintf(schema_cc, "    else if(!strcmp(schemaName, \"%s\"))\n", SCHEMAget_name(schema));
        fprintf(schema_cc, "        return (SDAI_Model_contents_ptr) new SdaiModel_contents_%s; \n", SCHEMAget_name(schema));
    }
    fprintf(schema_cc, "    else return (SDAI_Model_contents_ptr) 0;\n}\n");
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
void EXPRESSPrint(Express express, ComplexCollect &col, FILES *files)
{
    char fnm [MAX_LEN], *np;
    const char   *schnm;  /* schnm is really "express name" */
    FILE *libfile;
    FILE *incfile;
    FILE *schemafile = files -> incall;
    FILE *schemainit = files -> initall;
    FILE *initfile;
    /* new */
    Schema schema;
    DictionaryEntry de;


    /**********  create files based on name of schema   ***********/
    /*  return if failure           */
    /*  1.  header file             */
    sprintf(fnm, "%s.h", schnm = ClassName(EXPRESSget_basename(express)));
    if(!(incfile = (files -> inc) = FILEcreate(fnm))) {
        return;
    }
    fprintf(files->inc, "\n// in the exp2cxx source code, this file is generally referred to as files->inc or incfile\n");

    fprintf(incfile, "#include <sdai.h> \n");

    np = fnm + strlen(fnm) - 1;   /*  point to end of constant part of string  */
    /*  1.9 init unity files (large translation units, faster compilation) */
    initUnityFiles(schnm, files);
    /*  2.  class source file            */
    sprintf(np, "cc");
    if(!(libfile = (files -> lib) = FILEcreate(fnm))) {
        return;
    }
    fprintf(files->lib, "\n// in the exp2cxx source code, this file is generally referred to as files->lib or libfile\n");

    fprintf(libfile, "#include \"%s.h\" n", schnm);

    // 3. header for namespace to contain all formerly-global variables
    sprintf(fnm, "%sNames.h", schnm);
    if(!(files->names = FILEcreate(fnm))) {
        return;
    }
    fprintf(libfile, "#include \"%sNames.h\"\n", schnm);
    fprintf(files->names, "\n// In the exp2cxx source code, this file is referred to as files->names.\n// This line printed at %s:%d (one of two possible locations).\n\n", __FILE__, __LINE__);
    fprintf(files->names, "//this file contains a namespace for all formerly-global variables\n\n");
    //the next line in this file depends on the schema name, so printing continues in the while loop ~25 lines below

    /*  4.  source code to initialize entity registry   */
    /*  prints header of file for input function    */

    sprintf(np, "init.cc");
    if(!(initfile = (files -> init) = FILEcreate(fnm))) {
        return;
    }
    fprintf(files->init, "\n// in the exp2cxx source code, this file is generally referred to as files->init or initfile\n");

    fprintf(initfile, "#include \"%s.h\"\n\n", schnm);
    fprintf(initfile, "void \n%sInit (Registry& reg)\n{\n", schnm);

    /**********  record in files relating to entire input   ***********/

    /*  add to schema's include and initialization file */
    fprintf(schemafile, "#include \"%sNames.h\"\n", schnm);
    fprintf(schemafile, "#include \"%s.h\"\n\n", schnm);
    fprintf(schemafile, "extern void %sInit (Registry & r);\n", schnm);
    fprintf(schemainit, "         extern void %sInit (Registry & r);\n", schnm);
    fprintf(schemainit, "         %sInit (reg);\n", schnm);

    /**********  do all schemas ***********/
    DICTdo_type_init(express->symbol_table, &de, OBJ_SCHEMA);
    while((schema = (Scope)DICTdo(&de)) != 0) {
        numberAttributes(schema);
    }

    DICTdo_init(express->symbol_table, &de);
    bool first = true;
    while(0 != (schema = (Scope)DICTdo(&de))) {
        if(!first) {
            fprintf(files->names, "} //namespace %s\n", SCHEMAget_name(schema));
        }
        first = false;
        fprintf(files->names, "namespace %s {\n\n", SCHEMAget_name(schema));
        fprintf(files->names, "    extern Schema * schema;\n\n");

        SCOPEPrint(schema, files, schema, &col, 0);
    }

    /**********  close the files    ***********/
    closeUnityFiles(files);
    FILEclose(libfile);
    FILEclose(incfile);
    INITFileFinish(initfile, schema);
}

/**
 * drives functions to generate code for all the schemas
 * in an Express model into one set of files  -- works with EXPRESSPrint
 * Side Effects:  generates code
 * Status:  24-Feb-1992 new -kcm
 */
void print_schemas_combined(Express express, ComplexCollect &col, FILES *files)
{
    EXPRESSPrint(express, col, files);
}

/** this function calls one of two different functions
 *  depending on whether the output should be combined into a single
 *  set of files or a separate set for each schema
 */
void print_file(Express express)
{
    extern void RESOLUTIONsucceed(void);
    int separate_schemas = 1;
    ComplexCollect col(express);
    File_holder files;

    resolution_success();

    print_file_header(&files);
    if(separate_schemas) {
        print_schemas_separate(express, &col, &files);
    } else {
        print_schemas_combined(express, col, &files);
    }
    print_file_trailer(&files);
    print_complex(col, "compstructs.cc");
}
