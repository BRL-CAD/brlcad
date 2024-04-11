#include <string>

#include "entityDescriptor.h"
#include "Registry.h"
#include "attrDescriptor.h"
#include "inverseAttribute.h"
#include "SubSuperIterators.h"

EntityDescriptor::EntityDescriptor( )
    : _abstractEntity( LUnknown ), _extMapping( LUnknown ),
      _uniqueness_rules( ( Uniqueness_rule__set_var )0 ), NewSTEPentity( 0 ) {
}

EntityDescriptor::EntityDescriptor( const char * name, // i.e. char *
                                    Schema * origSchema,
                                    Logical abstractEntity, // F U or T
                                    Logical extMapping,
                                    Creator f
                                  )
    : TypeDescriptor( name, ENTITY_TYPE, origSchema, name ),
      _abstractEntity( abstractEntity ), _extMapping( extMapping ),
      _uniqueness_rules( ( Uniqueness_rule__set_var )0 ), NewSTEPentity( f ) {
}

EntityDescriptor::~EntityDescriptor() {
    delete _uniqueness_rules;
}

// initialize one inverse attr; used in InitIAttrs, below
void initIAttr( Inverse_attribute * ia, Registry & reg, const char * schNm, const char * name ) {
    const AttrDescriptor * ad;
    const char * aid = ia->inverted_attr_id_();
    const char * eid = ia->inverted_entity_id_();
    const EntityDescriptor * e = reg.FindEntity( eid, schNm );
    AttrDescItr adl( e->ExplicitAttr() );
    while( 0 != ( ad = adl.NextAttrDesc() ) ) {
        if( !strcmp( aid, ad->Name() ) ) {
            ia->inverted_attr_( ad );
            return;
        }
    }
    supertypesIterator sit( e );
    for( ; !sit.empty(); ++sit ) {
        AttrDescItr adi( sit.current()->ExplicitAttr() );
        while( 0 != ( ad = adi.NextAttrDesc() ) ) {
            if( !strcmp( aid, ad->Name() ) ) {
                ia->inverted_attr_( ad );
                return;
            }
        }
    }
    std::cerr << "Inverse attr " << ia->Name() << " for " << name << ": cannot find AttrDescriptor " << aid << " for entity " << eid << "." << std::endl;
    //FIXME should we abort? or is there a sensible recovery path?
    abort();
}

/** initialize inverse attrs
 * call once per eDesc (once per EXPRESS entity type)
 * must be called _after_ init_Sdai* functions for any ia->inverted_entity_id_'s
 *
 */
void EntityDescriptor::InitIAttrs( Registry & reg, const char * schNm ) {
    InverseAItr iai( &( InverseAttr() ) );
    Inverse_attribute * ia;
    while( 0 != ( ia = iai.NextInverse_attribute() ) ) {
        initIAttr( ia, reg, schNm, _name );
    }
}

const char * EntityDescriptor::GenerateExpress( std::string & buf ) const {
    std::string sstr;
    int count;
    int i;
    int all_comments = 1;

    buf = "ENTITY ";
    buf.append( StrToLower( Name(), sstr ) );

    if( strlen( _supertype_stmt.c_str() ) > 0 ) {
        buf.append( "\n  " );
    }
    buf.append( _supertype_stmt );

    const EntityDescriptor * ed = 0;

    EntityDescItr edi_super( _supertypes );
    edi_super.ResetItr();
    ed = edi_super.NextEntityDesc();
    int supertypes = 0;
    if( ed ) {
        buf.append( "\n  SUBTYPE OF (" );
        buf.append( StrToLower( ed->Name(), sstr ) );
        supertypes = 1;
    }
    ed = edi_super.NextEntityDesc();
    while( ed ) {
        buf.append( ",\n\t\t" );
        buf.append( StrToLower( ed->Name(), sstr ) );
        ed = edi_super.NextEntityDesc();
    }
    if( supertypes ) {
        buf.append( ")" );
    }

    buf.append( ";\n" );

    AttrDescItr adi( _explicitAttr );

    adi.ResetItr();
    const AttrDescriptor * ad = adi.NextAttrDesc();

    while( ad ) {
        if( ad->AttrType() == AttrType_Explicit ) {
            buf.append( "    " );
            buf.append( ad->GenerateExpress( sstr ) );
        }
        ad = adi.NextAttrDesc();
    }

    adi.ResetItr();
    ad = adi.NextAttrDesc();

    count = 1;
    while( ad ) {
        if( ad->AttrType() == AttrType_Deriving ) {
            if( count == 1 ) {
                buf.append( "  DERIVE\n" );
            }
            buf.append( "    " );
            buf.append( ad->GenerateExpress( sstr ) );
            count++;
        }
        ad = adi.NextAttrDesc();
    }
    /////////

    InverseAItr iai( &_inverseAttr );

    iai.ResetItr();
    const Inverse_attribute * ia = iai.NextInverse_attribute();

    if( ia ) {
        buf.append( "  INVERSE\n" );
    }

    while( ia ) {
        buf.append( "    " );
        buf.append( ia->GenerateExpress( sstr ) );
        ia = iai.NextInverse_attribute();
    }
    ///////////////
    // count is # of UNIQUE rules
    if( _uniqueness_rules != 0 ) {
        count = _uniqueness_rules->Count();
        for( i = 0; i < count; i++ ) { // print out each UNIQUE rule
            if( !( *( _uniqueness_rules ) )[i]->_label.size() ) {
                all_comments = 0;
            }
        }

        if( all_comments ) {
            buf.append( "  (* UNIQUE *)\n" );
        } else {
            buf.append( "  UNIQUE\n" );
        }
        for( i = 0; i < count; i++ ) { // print out each UNIQUE rule
            if( !( *( _uniqueness_rules ) )[i]->_comment.empty() ) {
                buf.append( "    " );
                buf.append( ( *( _uniqueness_rules ) )[i]->comment_() );
                buf.append( "\n" );
            }
            if( ( *( _uniqueness_rules ) )[i]->_label.size() ) {
                buf.append( "    " );
                buf.append( ( *( _uniqueness_rules ) )[i]->label_() );
                buf.append( "\n" );
            }
        }
    }

    ///////////////
    // count is # of WHERE rules
    if( _where_rules != 0 ) {
        all_comments = 1;
        count = _where_rules->Count();
        for( i = 0; i < count; i++ ) { // print out each UNIQUE rule
            if( !( *( _where_rules ) )[i]->_label.size() ) {
                all_comments = 0;
            }
        }

        if( !all_comments ) {
            buf.append( "  WHERE\n" );
        } else {
            buf.append( "  (* WHERE *)\n" );
        }
        for( i = 0; i < count; i++ ) { // print out each WHERE rule
            if( !( *( _where_rules ) )[i]->_comment.empty() ) {
                buf.append( "    " );
                buf.append( ( *( _where_rules ) )[i]->comment_() );
                buf.append( "\n" );
            }
            if( ( *( _where_rules ) )[i]->_label.size() ) {
                buf.append( "    " );
                buf.append( ( *( _where_rules ) )[i]->label_() );
                buf.append( "\n" );
            }
        }
    }

    buf.append( "END_ENTITY;\n" );

    return const_cast<char *>( buf.c_str() );
}

const char * EntityDescriptor::QualifiedName( std::string & s ) const {
    s.clear();
    EntityDescItr edi( _supertypes );

    int count = 1;
    const EntityDescriptor * ed = edi.NextEntityDesc();
    while( ed ) {
        if( count > 1 ) {
            s.append( "&" );
        }
        s.append( ed->Name() );
        count++;
        ed = edi.NextEntityDesc();
    }
    if( count > 1 ) {
        s.append( "&" );
    }
    s.append( Name() );
    return const_cast<char *>( s.c_str() );
}

const TypeDescriptor * EntityDescriptor::IsA( const TypeDescriptor * td ) const {
    if( td -> NonRefType() == ENTITY_TYPE ) {
        return IsA( ( EntityDescriptor * ) td );
    } else {
        return 0;
    }
}

const EntityDescriptor * EntityDescriptor::IsA( const EntityDescriptor * other )  const {
    const EntityDescriptor * found = 0;
    const EntityDescLinkNode * link = ( const EntityDescLinkNode * )( GetSupertypes().GetHead() );

    if( this == other ) {
        return other;
    } else {
        while( link && ! found )  {
            found = link -> EntityDesc() -> IsA( other );
            link = ( EntityDescLinkNode * ) link -> NextNode();
        }
    }
    return found;
}
