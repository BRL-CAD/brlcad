#include "dictSchema.h"

#include "typeDescriptor.h"
#include "entityDescriptor.h"

Schema::Schema( const char * schemaName )
: _use_interface_list( new Interface_spec__set ),
_ref_interface_list( new Interface_spec__set ),
_function_list( 0 ), _procedure_list( 0 ), _global_rules( 0 ) {
    _name = schemaName;
}

Schema::~Schema() {
    TypeDescLinkNode * node;

    if( _use_interface_list != 0 ) {
        delete _use_interface_list;
    }
    if( _ref_interface_list != 0 ) {
        delete _ref_interface_list;
    }
    if( _global_rules != 0 ) {
        delete _global_rules;
    }
    node = ( TypeDescLinkNode * ) _unnamed_typeList.GetHead();
    while( node ) {
        delete node->TypeDesc();
        node = ( TypeDescLinkNode * ) node->NextNode();
    }
}

void Schema::AddFunction( const std::string & f ) {
    _function_list.push_back( f );
}

void Schema::AddGlobal_rule( Global_rule_ptr gr ) {
    if( _global_rules == 0 ) {
        _global_rules = new Global_rule__set;
    }
    _global_rules->Append( gr );
}

/// hope I did this right (MP) - was "not implemented"
void Schema::global_rules_( Global_rule__set_var & grs ) {
    if( _global_rules ) {
        if( _global_rules->Count() > 0 ) {
            std::cerr << "In " << __FILE__ << ", Schema::global_rules_(): overwriting non-empty global rule set!" << std::endl;
        }
        delete _global_rules;
    }
    _global_rules = grs;
}

void Schema::AddProcedure( const std::string & p ) {
    _procedure_list.push_back( p );
}

/// the whole schema
void Schema::GenerateExpress( ostream & out ) const {
    std::string tmp;
    out << endl << "(* Generating: " << Name() << " *)" << endl;
    out << endl << "SCHEMA " << StrToLower( Name(), tmp ) << ";" << endl;
    GenerateUseRefExpress( out );
    // print TYPE definitions
    out << endl << "(* ////////////// TYPE Definitions *)" << endl;
    GenerateTypesExpress( out );

    // print Entity definitions
    out << endl << "(* ////////////// ENTITY Definitions *)" << endl;
    GenerateEntitiesExpress( out );

    int count, i;
    if( _global_rules != 0 ) {
        out << endl << "(* *************RULES************* *)" << endl;
        count = _global_rules->Count();
        for( i = 0; i <  count; i++ ) {
            out << endl << ( *_global_rules )[i]->rule_text_() << endl;
        }
    }
    if( !_function_list.empty() ) {
        out << "(* *************FUNCTIONS************* *)" << endl;
        count = _function_list.size();
        for( i = 0; i <  count; i++ ) {
            out << endl << _function_list[i] << endl;
        }
    }
    if( !_procedure_list.empty() ) {
        out << "(* *************PROCEDURES************* *)" << endl;
        count = _procedure_list.size();
        for( i = 0; i <  count; i++ ) {
            out << endl << _procedure_list[i] << endl;
        }
    }
    out << endl << "END_SCHEMA;" << endl;
}

/// USE, REFERENCE definitions
void Schema::GenerateUseRefExpress( ostream & out ) const {
    int i, k;
    int intf_count;
    int count;
    Interface_spec_ptr is;
    int first_time;
    std::string tmp;

    /////////////////////// print USE statements

    intf_count = _use_interface_list->Count();
    if( intf_count ) { // there is at least 1 USE interface to a foreign schema
        for( i = 0; i < intf_count; i++ ) { // print out each USE interface
            is = ( *_use_interface_list )[i]; // the 1st USE interface

            // count is # of USE items in interface
            count = is->explicit_items_()->Count();

            if( count > 0 ) {
                out << endl << "    USE FROM "
                << StrToLower( is->foreign_schema_id_().c_str(), tmp ) << endl;
                out << "       (";

                first_time = 1;
                for( k = 0; k < count; k++ ) { // print out each USE item
                    if( first_time ) {
                        first_time = 0;
                    } else {
                        out << "," << endl << "\t";
                    }
                    if( !( ( *( is->explicit_items_() ) )[k]->original_id_().size() ) ) {
                        // not renamed
                        out << ( *( is->explicit_items_() ) )[k]->new_id_();
                    } else { // renamed
                        out << ( *( is->explicit_items_() ) )[k]->original_id_();
                        out << " AS " << ( *( is->explicit_items_() ) )[k]->new_id_();
                    }
                }
                out << ");" << endl;
            } else if( is->all_objects_() ) {
                out << endl << "    USE FROM "
                << StrToLower( is->foreign_schema_id_().c_str(), tmp ) << ";"
                << endl;
            }
        }
    }

    /////////////////////// print REFERENCE stmts

    intf_count = _ref_interface_list->Count();
    if( intf_count ) { //there is at least 1 REFERENCE interface to a foreign schema
        for( i = 0; i < intf_count; i++ ) { // print out each REFERENCE interface
            is = ( *_ref_interface_list )[i]; // the 1st REFERENCE interface

            // count is # of REFERENCE items in interface
            count = is->explicit_items_()->Count();


            if( count > 0 ) {
                out << endl << "    REFERENCE FROM "
                << StrToLower( is->foreign_schema_id_().c_str(), tmp ) << endl;
                out << "       (";

                first_time = 1;
                for( k = 0; k < count; k++ ) { // print out each REFERENCE item
                    if( first_time ) {
                        first_time = 0;
                    } else {
                        out << "," << endl << "\t";
                    }
                    if( ( !( *( is->explicit_items_() ) )[k]->original_id_().size() ) ) {
                        // not renamed
                        out << ( *( is->explicit_items_() ) )[k]->new_id_();
                    } else { // renamed
                        out << ( *( is->explicit_items_() ) )[k]->original_id_();
                        out << " AS "
                        << ( *( is->explicit_items_() ) )[k]->new_id_();
                    }
                }
                out << ");" << endl;
            } else if( is->all_objects_() ) {
                out << endl << "    REFERENCE FROM "
                << StrToLower( is->foreign_schema_id_().c_str(), tmp ) << ";"
                << endl;
            }
        }
    }
}

/// TYPE definitions
void Schema::GenerateTypesExpress( ostream & out ) const {
    TypeDescItr tdi( _typeList );
    tdi.ResetItr();
    std::string tmp;

    const TypeDescriptor * td = tdi.NextTypeDesc();
    while( td ) {
        out << endl << td->GenerateExpress( tmp );
        td = tdi.NextTypeDesc();
    }
}

/// Entity definitions
void Schema::GenerateEntitiesExpress( ostream & out ) const {
    EntityDescItr edi( _entList );
    edi.ResetItr();
    std::string tmp;

    const EntityDescriptor * ed = edi.NextEntityDesc();
    while( ed ) {
        out << endl << ed->GenerateExpress( tmp );
        ed = edi.NextEntityDesc();
    }
}
