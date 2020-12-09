#include "STEPaggrEntity.h"
#include "STEPattribute.h"
#include "typeDescriptor.h"
#include <sstream>

/** \file STEPaggrEntity.cc
 * implement classes EntityAggregate, EntityNode
 */


EntityAggregate::EntityAggregate() {
}

EntityAggregate::~EntityAggregate() {
}


/// if exchangeFileFormat == 1 then delims are required.
Severity EntityAggregate::ReadValue( istream & in, ErrorDescriptor * err,
                                     const TypeDescriptor * elem_type, InstMgrBase * insts,
                                     int addFileId, int assignVal,
                                     int exchangeFileFormat, const char * ) {
    ErrorDescriptor errdesc;
    char errmsg[BUFSIZ];
    int value_cnt = 0;
    std::string buf;

    if( assignVal ) {
        Empty();    // read new values and discard existing ones
    }

    char c;

    in >> ws; // skip white space

    c = in.peek(); // does not advance input

    if( in.eof() || ( c == '$' ) ) {
        _null = 1;
        err->GreaterSeverity( SEVERITY_INCOMPLETE );
        return SEVERITY_INCOMPLETE;
    }

    if( c == '(' ) {
        in.get( c );
    } else if( exchangeFileFormat ) {
        // error did not find opening delim
        // give up because you do not know where to stop reading.
        err->GreaterSeverity( SEVERITY_INPUT_ERROR );
        return SEVERITY_INPUT_ERROR;
    } else if( !in.good() ) {
        // this should actually have been caught by skipping white space above
        err->GreaterSeverity( SEVERITY_INCOMPLETE );
        return SEVERITY_INCOMPLETE;
    }

    EntityNode * item = 0;

    in >> ws;
    // take a peek to see if there are any elements before committing to an
    // element
    c = in.peek(); // does not advance input
    if( c == ')' ) {
        in.get( c );
    }
    // if not assigning values only need one node. So only one node is created.
    // It is used to read the values
    else if( !assignVal ) {
        item = new EntityNode();
    }

    while( in.good() && ( c != ')' ) ) {
        value_cnt++;
        if( assignVal ) { // create a new node each time through the loop
            item = new EntityNode();
        }

        errdesc.ClearErrorMsg();

        if( exchangeFileFormat ) {
            item->STEPread( in, &errdesc, elem_type, insts, addFileId );
        } else {
            item->StrToVal( in, &errdesc, elem_type, insts, addFileId );
        }

        elem_type->AttrTypeName( buf );
        // read up to the next delimiter and set errors if garbage is
        // found before specified delims (i.e. comma and quote)
        CheckRemainingInput( in, &errdesc, buf, ",)" );

        if( errdesc.severity() < SEVERITY_INCOMPLETE ) {
            sprintf( errmsg, "  index:  %d\n", value_cnt );
            errdesc.PrependToDetailMsg( errmsg );
            err->AppendFromErrorArg( &errdesc );
        }
        if( assignVal ) {
            AddNode( item );
        }

        in >> ws; // skip white space (although should already be skipped)
        in.get( c ); // read delim

        // CheckRemainingInput should have left the input right at the delim
        // so that it would be read in in.get() above.  Since it did not find
        // the delim this does not know how to find it either!
        if( ( c != ',' ) && ( c != ')' ) ) {
            // cannot recover so give up and let STEPattribute recover
            err->GreaterSeverity( SEVERITY_INPUT_ERROR );
            return SEVERITY_INPUT_ERROR;
        }
    }
    if( c == ')' ) {
        _null = 0;
    } else { // expectation for end paren delim has not been met
        err->GreaterSeverity( SEVERITY_INPUT_ERROR );
        err->AppendToUserMsg( "Missing close paren for aggregate value" );
        return SEVERITY_INPUT_ERROR;
    }
    return err->severity();
}


STEPaggregate & EntityAggregate::ShallowCopy( const STEPaggregate & a ) {
    const EntityNode * tmp = ( const EntityNode * ) a.GetHead();
    while( tmp ) {
        AddNode( new EntityNode( tmp -> node ) );
        tmp = ( const EntityNode * ) tmp -> NextNode();
    }
    if( head ) {
        _null = 0;
    } else {
        _null = 1;
    }

    return *this;
}


SingleLinkNode * EntityAggregate::NewNode() {
    return new EntityNode();
}

EntityNode::EntityNode() {
}

EntityNode::~EntityNode() {
}

EntityNode::EntityNode( SDAI_Application_instance  * e ) : node( e ) {
}

SingleLinkNode * EntityNode::NewNode() {
    return new EntityNode();
}
///////////////////////////////////////////////////////////////////////////////

Severity EntityNode::StrToVal( const char * s, ErrorDescriptor * err,
                               const TypeDescriptor * elem_type,
                               InstMgrBase * insts, int addFileId ) {
    SDAI_Application_instance * se = ReadEntityRef( s, err, ",)", insts,
                                     addFileId );
    if( se != S_ENTITY_NULL ) {
        ErrorDescriptor error;
        if( EntityValidLevel( se, elem_type, &error ) == SEVERITY_NULL ) {
            node = se;
        } else {
            node = S_ENTITY_NULL;
            err->AppendToDetailMsg( error.DetailMsg() );
            err->AppendToUserMsg( error.UserMsg() );
            err->GreaterSeverity( error.severity() );
        }
    } else {
        node = S_ENTITY_NULL;
    }
    return err->severity();
}

Severity EntityNode::StrToVal( istream & in, ErrorDescriptor * err,
                               const TypeDescriptor * elem_type,
                               InstMgrBase * insts, int addFileId ) {
    return STEPread( in, err, elem_type, insts, addFileId );
}

Severity EntityNode::STEPread( const char * s, ErrorDescriptor * err,
                               const TypeDescriptor * elem_type,
                               InstMgrBase * insts, int addFileId ) {
    istringstream in( ( char * )s );
    return STEPread( in, err, elem_type, insts, addFileId );
}

Severity EntityNode::STEPread( istream & in, ErrorDescriptor * err,
                               const TypeDescriptor * elem_type,
                               InstMgrBase * insts, int addFileId ) {
    SDAI_Application_instance * se = ReadEntityRef( in, err, ",)", insts,
                                     addFileId );
    if( se != S_ENTITY_NULL ) {
        ErrorDescriptor error;
        if( EntityValidLevel( se, elem_type, &error ) == SEVERITY_NULL ) {
            node = se;
        } else {
            node = S_ENTITY_NULL;
            err->AppendToDetailMsg( error.DetailMsg() );
            err->AppendToUserMsg( error.UserMsg() );
            err->GreaterSeverity( error.severity() );
        }
    } else {
        node = S_ENTITY_NULL;
    }
    return err->severity();
}

const char * EntityNode::asStr( std::string & s ) {
    s.clear();
    if( !node || ( node == S_ENTITY_NULL ) ) { //  nothing
        return "";
    } else { // otherwise return entity id
        char tmp [64];
        sprintf( tmp, "#%d", node->STEPfile_id );
        s = tmp;
    }
    return const_cast<char *>( s.c_str() );
}

const char * EntityNode::STEPwrite( std::string & s, const char * ) {
    if( !node || ( node == S_ENTITY_NULL ) ) { //  nothing
        s = "$";
        return const_cast<char *>( s.c_str() );
    }
    asStr( s );
    return const_cast<char *>( s.c_str() );
}

void EntityNode::STEPwrite( ostream & out ) {
    if( !node || ( node == S_ENTITY_NULL ) ) { //  nothing
        out << "$";
    }
    std::string s;
    out << asStr( s );
}


