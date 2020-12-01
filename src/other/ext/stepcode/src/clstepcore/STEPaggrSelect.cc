#include "STEPaggrSelect.h"
#include "typeDescriptor.h"
#include <sstream>

/** \file STEPaggrSelect.cc
 * implement classes SelectAggregate, SelectNode
 */

SelectAggregate::SelectAggregate() {
}

SelectAggregate::~SelectAggregate() {
}


/// if exchangeFileFormat == 1 then delims are required.
Severity SelectAggregate::ReadValue( istream & in, ErrorDescriptor * err,
                                     const TypeDescriptor * elem_type, InstMgrBase * insts,
                                     int addFileId, int assignVal,
                                     int exchangeFileFormat, const char * currSch ) {
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

    SelectNode * item = 0;

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
        item = ( SelectNode * ) NewNode();
    }

    while( in.good() && ( c != ')' ) ) {
        value_cnt++;
        if( assignVal ) { // create a new node each time through the loop
            item = ( SelectNode * ) NewNode();
        }

        errdesc.ClearErrorMsg();

        if( exchangeFileFormat ) {
            item->STEPread( in, &errdesc, elem_type, insts, addFileId, currSch );
        } else {
            item->StrToVal( in, &errdesc, elem_type, insts, addFileId, currSch );
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


STEPaggregate & SelectAggregate::ShallowCopy( const STEPaggregate & a ) {
    const SelectNode * tmp = ( const SelectNode * ) a.GetHead();
    while( tmp ) {
        AddNode( new SelectNode( tmp -> node ) );

        tmp = ( const SelectNode * ) tmp -> NextNode();
    }
    if( head ) {
        _null = 0;
    } else {
        _null = 1;
    }

    return *this;
}


SingleLinkNode * SelectAggregate::NewNode() {
    return new SelectNode();
}


SelectNode::SelectNode( SDAI_Select  * s ) :  node( s ) {
}

SelectNode::SelectNode() {
}

SelectNode::~SelectNode() {
    delete node;
}

SingleLinkNode * SelectNode::NewNode() {
    return new SelectNode();
}

Severity SelectNode::StrToVal( const char * s, ErrorDescriptor * err,
                               const TypeDescriptor * elem_type,
                               InstMgrBase * insts, int addFileId ) {
    (void) elem_type; //unused
    (void) addFileId; //unused
    istringstream in( ( char * )s );
    if( err->severity( node->STEPread( in, err, insts ) ) != SEVERITY_NULL ) {
        err->AppendToDetailMsg( node ->Error() );
    }
    return err->severity();
}

Severity SelectNode::StrToVal( istream & in, ErrorDescriptor * err,
                               const TypeDescriptor * elem_type,
                               InstMgrBase * insts, int addFileId, const char * currSch ) {
    return STEPread( in, err, elem_type, insts, addFileId, currSch );
}

Severity SelectNode::STEPread( const char * s, ErrorDescriptor * err,
                               const TypeDescriptor * elem_type,
                               InstMgrBase * insts, int addFileId ) {
    istringstream in( ( char * )s );
    return STEPread( in, err, elem_type, insts, addFileId );
}

Severity SelectNode::STEPread( istream & in, ErrorDescriptor * err,
                               const TypeDescriptor * elem_type,
                               InstMgrBase * insts, int addFileId, const char * currSch ) {
    (void) elem_type; //unused
    if( !node )  {
        cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n"
             << _POC_ "\n";
        cerr << "function:  SelectNode::STEPread \n" << "\n";
        return SEVERITY_BUG;
    }
    err->severity( node->STEPread( in, err, insts, 0, addFileId, currSch ) );
    CheckRemainingInput( in, err, "select", ",)" );
    return err->severity();
}

const char * SelectNode::asStr( std::string & s ) {
    s.clear();
    if( !node || ( node->is_null() ) ) {  //  nothing
        return "";
    } else { // otherwise return entity id
        node -> STEPwrite( s );
        return const_cast<char *>( s.c_str() );
    }
}

const char * SelectNode::STEPwrite( std::string & s, const char * currSch ) {
    s.clear();
    if( !node || ( node->is_null() ) ) {  //  nothing
        s = "$";
        return "$";
    }
    node -> STEPwrite( s, currSch );
    return const_cast<char *>( s.c_str() );
}

void SelectNode::STEPwrite( ostream & out ) {
    if( !node || ( node->is_null() ) ) {  //  nothing
        out << "$";
    }
    std::string s;
    out << asStr( s );
}

