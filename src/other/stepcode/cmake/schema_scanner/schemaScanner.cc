
/** \file schemaScanner.cc
 * This file, along with part of libexpress, are compiled (at configure time)
 * into a static executable. This executable is a schema scanner that is used
 * by CMake to determine what files exp2cxx will create. Otherwise, we'd need
 * to use a few huge files - there is no other way to tell CMake what the
 * generated files will be called.
 */

/* WARNING
 * If you modify this file, you must re-run cmake. It doesn't seem to be possible
 * to re-run cmake automatically, as CMake's configure_file() gets confused by
 * the '${' and '}' in writeLists() below.
 */

extern "C" {
#  include "expparse.h"
#  include "expscan.h"
#  include "express/scope.h"
#  include "genCxxFilenames.h"
#  include "sc_mkdir.h"

#  include <string.h>

#  ifdef _WIN32
#    include <direct.h>
#    define getcwd _getcwd
#  else
#    include <unistd.h>
#  endif
}

#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <iostream>

int multiple_inheritance = 0;

using std::string;
using std::stringstream;
using std::endl;
using std::ofstream;
using std::cerr;
using std::cout;

/** \return true for types that exp2cxx won't generate code for */
bool notGenerated( const Type t ) {
    switch( TYPEget_body( t )->type ) {
        case integer_:
        case real_:
        case string_:
        case binary_:
        case boolean_:
        case number_:
        case logical_:
        case aggregate_:
        case bag_:
        case set_:
        case list_:
        case array_:
            return true;
        default:
            break;
    }
    /* from addRenameTypedefs() in multpass.c - check for renamed
     * enums and selects - exp2cxx prints typedefs for them */
    if( ( TYPEis_enumeration( t ) || TYPEis_select( t ) ) && ( TYPEget_head( t ) ) ) {
            return true;
    }
    return false;
}

/** \return a short name for the schema
 * this name is intended to be unique, but no special care is put into its generation
 * the official schema name seems most likely to not be unique, but it is generally the longest
 *
 * picks the shortest of:
 *  * the name of the dir containing the file (if in sc/data),
 *  * the name of the file containing the schema (minus extension),
 *  * the official name of the schema, passed in arg 'longName'
 *
 * TODO: maybe also consider an acronym, such that 'test_array_bounds_expr' becomes 'tabe', 'TABE', 'T_A_B_E', or similar
 */
string makeShortName( const char * longName ) {
    //input_filename is path to file. we will extract dir name and file name from it.
    string dirname = input_filename, filename = input_filename, schname = longName;
    const char slash = '/';

    //for filename, get rid of dir name(s), if any, as well as the extension
    size_t sl = filename.rfind( slash );
    if( sl != string::npos ) {
        filename.erase( 0, sl + 1 );
    }
    size_t dot = filename.rfind( '.' );
    if( dot != string::npos ) {
        filename.erase( dot );
    }

    //for dirname, get rid of the filename and ensure it's in data/
    sl = dirname.rfind( slash );
    if( sl != string::npos ) {
        dirname.erase( sl );
    }
    const char * dat = "data";
    size_t data = dirname.find( dat );
    if( ( data == string::npos ) || ( dirname[ data + strlen( dat ) ] != slash ) ) {
        //doesn't contain 'data/'. clear it so it'll be ignored.
        dirname.clear();
    } else {
        //get rid of all but last dir name
        dirname.erase( 0, dirname.rfind( slash ) + 1 );
    }

    //use dir name if it's at least 3 chars and shorter than file name
    //length requirement gets rid of short, undescriptive dir names - including '.' and '..'
    if( ( dirname.size() > 2 ) && ( dirname.size() < filename.size() ) ) {
        filename = dirname;
    }
    if( strlen( longName ) < filename.size() ) {
        filename = longName;
    }
    filename.insert( 0, "sdai_" );
    return filename;
}

/** write a CMakeLists.txt file for the schema; print its directory to stdout for CMake's add_subdirectory() command */
void writeLists( const char * schemaName, stringstream & eh, stringstream & ei, int ecount,
                                           stringstream & th, stringstream & ti, int tcount ) {
    string shortName = makeShortName( schemaName );
    if( mkDirIfNone( shortName.c_str() ) < 0 ) {
        cerr << "Error creating directory " << shortName << " at " << __FILE__ << ":" << __LINE__;
        perror( 0 );
        exit( EXIT_FAILURE );
    }
    size_t nameLen = strlen( schemaName );
    string schema_upper( nameLen, char() );
    for( size_t i = 0; i < nameLen; ++i) {
        schema_upper[i] = toupper(schemaName[i]);
    }
    string cmListsPath = shortName;
    cmListsPath += "/CMakeLists.txt";
    ofstream cmLists;
    cmLists.open( cmListsPath.c_str() );
    if( !cmLists.good() ) {
        cerr << "error opening file " << cmListsPath << " - exiting." << endl;
        exit( EXIT_FAILURE );
    }
    cmLists << "# -----  GENERATED FILE  -----" << endl;
    cmLists << "# -----   Do not edit!   -----" << endl << endl;

    cmLists << "# schema name: " << schemaName << endl;
    cmLists << "# (short name: " << shortName << ")" << endl;
    cmLists << "# " << ecount << " entities, " << tcount << " types" << endl << endl;

    cmLists << "# targets, logic, etc are within a set of macros shared by all schemas" << endl;
    cmLists << "include(${SC_CMAKE_DIR}/SC_CXX_schema_macros.cmake)" << endl;

    // * 2 for headers, + 10 other files
    cmLists << "set(" << shortName << "_file_count " << ( ( ecount + tcount ) * 2 ) + 10 << ")" << endl << endl;


    cmLists << "PROJECT(" << shortName << ")" << endl;
    cmLists << "# list headers so they can be installed - entity, type, misc" << endl;

    cmLists << "set(" << shortName << "_entity_hdrs" << endl;
    cmLists << eh.str();
    cmLists << "   )" << endl << endl;

    cmLists << "set(" << shortName << "_type_hdrs" << endl;
    cmLists << th.str();
    cmLists << "   )" << endl << endl;

    cmLists << "set(" << shortName << "_misc_hdrs" << endl;
    cmLists << "     Sdaiclasses.h   schema.h" << endl;
    cmLists << "     Sdai" << schema_upper << "Names.h" << endl;
    cmLists << "     Sdai" << schema_upper << ".h" << endl;
    cmLists << "   )" << endl << endl;

    cmLists << "# install all headers" << endl;
    cmLists << "set(all_headers ${" << shortName << "_entity_hdrs} ${" << shortName << "_type_hdrs} ${" << shortName << "_misc_hdrs})" << endl;
    cmLists << "foreach( header_file ${all_headers} )" << endl;
    cmLists << "  set(curr_dir)" << endl;
    cmLists << "  get_filename_component(curr_dir ${header_file} PATH)" << endl;
    cmLists << "  get_filename_component(curr_name ${header_file} NAME)" << endl;
    cmLists << "  if (curr_dir)" << endl;
    cmLists << "    install( FILES ${header_file} DESTINATION \"include/schemas/" << shortName << "/${curr_dir}\" )" << endl;
    cmLists << "  else (curr_dir)" << endl;
    cmLists << "    install( FILES ${header_file} DESTINATION \"include/schemas/" << shortName << "\" )" << endl;
    cmLists << "  endif (curr_dir)" << endl;
    cmLists << "endforeach()" << endl;

    cmLists << "# implementation files - 3 lists" << endl << endl;

    cmLists << "# unity build: #include small .cc files to reduce the number" << endl;
    cmLists << "# of translation units that must be compiled" << endl;
    cmLists << "if(SC_UNITY_BUILD)" << endl << "  # turns off include statements within type and entity .cc's - the unity T.U.'s include a unity header" << endl;
    cmLists << "  add_definitions( -DSC_SDAI_UNITY_BUILD)" << endl;
    cmLists << "  set(" << shortName << "_entity_impls Sdai" << schema_upper << "_unity_entities.cc)" << endl;
    cmLists << "  set(" << shortName << "_type_impls Sdai" << schema_upper << "_unity_types.cc)" << endl;
    cmLists << "else(SC_UNITY_BUILD)" << endl;
    cmLists << "  set(" << shortName << "_entity_impls" << endl;
    cmLists << ei.str();
    cmLists << "   )" << endl << endl;

    cmLists << "  set(" << shortName << "_type_impls" << endl;
    cmLists << ti.str();
    cmLists << "   )" << endl;
    cmLists << "endif(SC_UNITY_BUILD)" << endl << endl;

    cmLists << "set( " << shortName << "_misc_impls" << endl;
    cmLists << "     SdaiAll.cc    compstructs.cc    schema.cc" << endl;
    cmLists << "     Sdai" << schema_upper << ".cc" << endl;
    cmLists << "     Sdai" << schema_upper << ".init.cc   )" << endl << endl;

    cmLists << "set(schema_target_files ${" << shortName << "_entity_impls} " << "${" << shortName << "_type_impls} " << "${" << shortName << "_misc_impls})" << endl;
    cmLists << "SCHEMA_TARGETS(\"" << input_filename << "\" \"" << schemaName << "\"" << endl;
    cmLists << "                 \"${schema_target_files}\")" << endl;

    cmLists.close();

    char pwd[BUFSIZ] = {0};
    if( getcwd( pwd, BUFSIZ ) ) {
        cout << pwd << "/" << shortName << endl;
    } else {
        cerr << "Error encountered by getcwd() for " << shortName << " - exiting. Error was ";
        perror( 0 );
        exit( EXIT_FAILURE );
    }
}

void printSchemaFilenames( Schema sch ){
    const int numColumns = 2;
    const int colWidth = 75;
    const char * tab = "     ";

    stringstream typeHeaders, typeImpls, entityHeaders, entityImpls;
    typeHeaders << tab;
    typeImpls << tab;
    entityHeaders << tab;
    entityImpls << tab;

    int ecount = 0, tcount = 0;

    DictionaryEntry de;
    void *x;
    filenames_t fn;
    DICTdo_init( sch->symbol_table, &de );
    while( 0 != ( x = DICTdo( &de ) ) ) {
        switch( DICT_type ) {
            case OBJ_ENTITY:
                fn = getEntityFilenames( ( Entity ) x );
                entityHeaders << std::left << std::setw( colWidth ) << fn.header << " ";
                entityImpls << std::left << std::setw( colWidth ) << fn.impl << " ";
                ++ecount;
                if( ( ecount % numColumns ) == 0 ) {
                    // columns
                    entityHeaders << endl << tab;
                    entityImpls << endl << tab;
                }
                break;
            case OBJ_TYPE: {
                Type t = ( Type ) x;
                if( TYPEis_enumeration( t ) && ( TYPEget_head( t ) ) ) {
                    /* t is a renamed enum type, for which exp2cxx
                     * will print a typedef in an existing file */
                    break;
                }
                if( notGenerated( t ) ) {
                    /* skip builtin types */
                    break;
                }
                fn = getTypeFilenames( t );
                typeHeaders << std::left << std::setw( colWidth ) << fn.header << " ";
                typeImpls << std::left << std::setw( colWidth ) << fn.impl << " ";
                ++tcount;
                if( ( tcount % numColumns ) == 0 ) {
                    typeHeaders << endl << tab;
                    typeImpls << endl << tab;
                }
                break;
            }
            /* case OBJ_FUNCTION:
             *            case OBJ_PROCEDURE:
             *            case OBJ_RULE: */
            default:
                /* ignore everything else */
                /* TODO: if DEBUG is defined, print the names of these to stderr */
                break;
        }
    }
    //write the CMakeLists.txt
    writeLists( sch->symbol.name, entityHeaders, entityImpls, ecount, typeHeaders, typeImpls, tcount );
}

int main( int argc, char ** argv ) {
    /* TODO init globals! */

    Schema schema;
    DictionaryEntry de;
    /* copied from fedex.c */
    Express model;
    if( ( argc != 2 ) || ( strlen( argv[1] ) < 1 ) ) {
        fprintf( stderr, "\nUsage: %s file.exp\nOutput: a CMakeLists.txt to build the schema,", argv[0] );
        fprintf( stderr, " containing file names for entities, types, etc\n" );
        fprintf( stderr, "also prints (to stdout) the absolute path to the directory CMakeLists.txt was created in\n" );
        exit( EXIT_FAILURE );
    }
    EXPRESSprogram_name = argv[0];
    input_filename = argv[1];

    EXPRESSinitialize();

    model = EXPRESScreate();
    EXPRESSparse( model, ( FILE * )0, input_filename );
    if( ERRORoccurred ) {
        EXPRESSdestroy( model );
        exit( EXIT_FAILURE );
    }
    EXPRESSresolve( model );
    if( ERRORoccurred ) {
        int result = EXPRESS_fail( model );
        EXPRESScleanup();
        EXPRESSdestroy( model );
        exit( result );
    }

    DICTdo_type_init( model->symbol_table, &de, OBJ_SCHEMA );
    while( 0 != ( schema = ( Schema )DICTdo( &de ) ) ) {
        printSchemaFilenames( schema );
    }

    EXPRESSdestroy( model );
    exit( EXIT_SUCCESS );
}
