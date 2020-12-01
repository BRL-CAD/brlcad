
/*
* NIST STEP Core Class Library
* clstepcore/STEPaggregate.cc
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <stdio.h>

#include <read_func.h>
#include <STEPaggregate.h>
#include <STEPattribute.h>
#include <instmgr.h>
#include <ExpDict.h>
#include "sc_memmgr.h"


/**
 *    \file STEPaggregate.cc Functions for manipulating aggregate attributes
 *
 * Most of the classes from here were moved into smaller files STEPaggr*.h,.cc 2/12/15 -- MP
 *
 *  FIXME KNOWN BUGs:
 *     -- treatment of aggregates of reals or ints is inconsistent with
 *        other aggregates (there's no classes for these)
 *     -- no two- dimensional aggregates are implemented
 */

STEPaggregate NilSTEPaggregate;


STEPaggregate::STEPaggregate() {
    _null = true;
}

STEPaggregate::~STEPaggregate() {
    STEPnode * node;

    node = ( STEPnode * ) head;
    while( node ) {
        head = node->NextNode();
        delete node;
        node = ( STEPnode * ) head;
    }
}

STEPaggregate & STEPaggregate::ShallowCopy( const STEPaggregate & a ) {
    (void) a; // unused
    cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__
         << "\n" << _POC_ "\n";
    cerr << "function:  STEPaggregate::ShallowCopy \n" << "\n";
    return *this;
}

/// do not require exchange file format
Severity STEPaggregate::AggrValidLevel( const char * value, ErrorDescriptor * err,
                                        const TypeDescriptor * elem_type, InstMgrBase * insts,
                                        int optional, char * tokenList, int addFileId,
                                        int clearError ) {
    std::string buf;
    if( clearError ) {
        err->ClearErrorMsg();
    }

    istringstream in( ( char * )value ); // sz defaults to length of s

    ReadValue( in, err, elem_type, insts, addFileId, 0, 0 );
    elem_type->AttrTypeName( buf );
    CheckRemainingInput( in, err, buf, tokenList );
    if( optional && ( err->severity() == SEVERITY_INCOMPLETE ) ) {
        err->severity( SEVERITY_NULL );
    }
    return err->severity();
}

/// require exchange file format
Severity STEPaggregate::AggrValidLevel( istream & in, ErrorDescriptor * err,
                                        const TypeDescriptor * elem_type, InstMgrBase * insts,
                                        int optional, char * tokenList, int addFileId,
                                        int clearError ) {
    std::string buf;
    if( clearError ) {
        err->ClearErrorMsg();
    }

    ReadValue( in, err, elem_type, insts, addFileId, 0, 1 );
    elem_type->AttrTypeName( buf );
    CheckRemainingInput( in, err, buf, tokenList );
    if( optional && ( err->severity() == SEVERITY_INCOMPLETE ) ) {
        err->severity( SEVERITY_NULL );
    }
    return err->severity();
}

/// if exchangeFileFormat == 1 then paren delims are required.
Severity STEPaggregate::ReadValue( istream & in, ErrorDescriptor * err,
                                   const TypeDescriptor * elem_type, InstMgrBase * insts,
                                   int addFileId, int assignVal, int exchangeFileFormat,
                                   const char * ) {
    (void) insts; //not used in ReadValue() for this class
    (void) addFileId; //not used in ReadValue() for this class

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

    if( in.eof() || c == '$' ) {
        _null = true;
        err->GreaterSeverity( SEVERITY_INCOMPLETE );
        return SEVERITY_INCOMPLETE;
    }

    if( c == '(' ) {
        in.get( c );
    } else if( exchangeFileFormat ) {
        // error did not find opening delim
        // cannot recover so give up and let STEPattribute recover
        err->GreaterSeverity( SEVERITY_INPUT_ERROR );
        return SEVERITY_INPUT_ERROR;
    } else if( !in.good() ) {
        // this should actually have been caught by skipping white space above
        err->GreaterSeverity( SEVERITY_INCOMPLETE );
        return SEVERITY_INCOMPLETE;
    }

    STEPnode * item = 0;

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
        item = ( STEPnode * )NewNode();
    }

    // ')' is the end of the aggregate
    while( in.good() && ( c != ')' ) ) {
        value_cnt++;
        if( assignVal ) { // create a new node each time through the loop
            item = ( STEPnode * )NewNode();
        }

        errdesc.ClearErrorMsg();

        if( exchangeFileFormat ) {
            item->STEPread( in, &errdesc );
        } else {
            item->StrToVal( in, &errdesc );
        }

        // read up to the next delimiter and set errors if garbage is
        // found before specified delims (i.e. comma and quote)
        elem_type->AttrTypeName( buf );
        CheckRemainingInput( in, &errdesc, buf, ",)" );

        if( errdesc.severity() < SEVERITY_INCOMPLETE ) {
            sprintf( errmsg, "  index:  %d\n", value_cnt );
            errdesc.PrependToDetailMsg( errmsg );
            err->AppendFromErrorArg( &errdesc );
        }
        if( assignVal ) { // pass the node to STEPaggregate
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
        _null = false;
    } else { // expectation for end paren delim has not been met
        err->GreaterSeverity( SEVERITY_INPUT_ERROR );
        err->AppendToUserMsg( "Missing close paren for aggregate value" );
        return SEVERITY_INPUT_ERROR;
    }
    return err->severity();
}

Severity STEPaggregate::StrToVal( const char * s, ErrorDescriptor * err,
                                  const TypeDescriptor * elem_type, InstMgrBase * insts,
                                  int addFileId ) {
    istringstream in( ( char * )s );
    return ReadValue( in, err, elem_type, insts, addFileId, 1, 0 );
}

///////////////////////////////////////////////////////////////////////////////

Severity STEPaggregate::STEPread( istream & in, ErrorDescriptor * err,
                                  const TypeDescriptor * elem_type, InstMgrBase * insts,
                                  int addFileId, const char * currSch ) {
    return ReadValue( in, err, elem_type, insts, addFileId, 1, 1, currSch );
}

const char * STEPaggregate::asStr( std::string & s ) const {
    s.clear();

    if( !_null ) {
        s = "(";
        STEPnode * n = ( STEPnode * ) head;
        std::string tmp;
        while( n ) {
            s.append( n->STEPwrite( tmp ) );
            n = ( STEPnode * ) n -> NextNode();
            if( n ) {
                s.append( "," );
            }
        }
        s.append( ")" );
    }
    return const_cast<char *>( s.c_str() );
}

void STEPaggregate::STEPwrite( ostream & out, const char * currSch ) const {
    if( !_null ) {
        out << '(';
        STEPnode * n = ( STEPnode * )head;
        std::string s;
        while( n ) {
            out << n->STEPwrite( s, currSch );
            n = ( STEPnode * ) n -> NextNode();
            if( n ) {
                out <<  ',';
            }
        }
        out << ')';
    } else {
        out << '$';
    }
}

SingleLinkNode * STEPaggregate::NewNode() {
    cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
    cerr << "function:  STEPaggregate::NewNode \n" << _POC_ << "\n";
    return 0;
}

void STEPaggregate::AddNode( SingleLinkNode * n ) {
    SingleLinkList::AppendNode( n );
    _null = false;
}

void STEPaggregate::Empty() {
    SingleLinkList::Empty();
    _null = true;
}


///////////////////////////////////////////////////////////////////////////////
// STEPnode
///////////////////////////////////////////////////////////////////////////////

Severity STEPnode::StrToVal( const char * s, ErrorDescriptor * err ) {
    // defined in subtypes
    (void) s; //unused
    cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
    err->AppendToDetailMsg(
        " function: STEPnode::StrToVal() called instead of virtual function.\n"
    );
    err->AppendToDetailMsg( "Aggr. attr value: '\n" );
    err->AppendToDetailMsg( "not assigned.\n" );
    err->AppendToDetailMsg( _POC_ );
    err->GreaterSeverity( SEVERITY_BUG );
    return SEVERITY_BUG;
}

Severity STEPnode::StrToVal( istream & in, ErrorDescriptor * err ) {
    // defined in subtypes
    (void) in; //unused
    cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
    err->AppendToDetailMsg(
        " function: STEPnode::StrToVal() called instead of virtual function.\n"
    );
    err->AppendToDetailMsg( "Aggr. attr value: '\n" );
    err->AppendToDetailMsg( "not assigned.\n" );
    err->AppendToDetailMsg( _POC_ );
    err->GreaterSeverity( SEVERITY_BUG );
    return SEVERITY_BUG;
}

Severity STEPnode::STEPread( const char * s, ErrorDescriptor * err ) {
    //  defined in subclasses
    (void) s; //unused
    cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
    cerr << "function:  STEPnode::STEPread called instead of virtual function.\n"
         << _POC_ << "\n";

    err->AppendToDetailMsg(
        " function: STEPnode::STEPread() called instead of virtual function.\n"
    );
    err->AppendToDetailMsg( _POC_ );
    err->GreaterSeverity( SEVERITY_BUG );

    return SEVERITY_BUG;
}

Severity STEPnode::STEPread( istream & in, ErrorDescriptor * err ) {
    (void) in; //unused
    cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
    cerr << "function:  STEPnode::STEPread called instead of virtual function.\n"
         << _POC_ << "\n";

    err->AppendToDetailMsg(
        " function: STEPnode::STEPread() called instead of virtual function.\n"
    );
    err->AppendToDetailMsg( _POC_ );
    err->GreaterSeverity( SEVERITY_BUG );
    return SEVERITY_BUG;
}

const char * STEPnode::asStr( std::string & s ) {
    //  defined in subclasses
    (void) s; //unused
    cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
    cerr << "function:  STEPnode::asStr called instead of virtual function.\n"
         << _POC_ << "\n";

    return "";
}

/**
 * NOTE - second argument will contain name of current schema.  We may need
 * this if STEPnode belongs to an aggregate of selects.  If so, each node
 * will be written out by calling STEPwrite on its select, and to write a
 * select out, the name of its underlying type will be written.  Finally,
 * the underlying type's name depends on the current schema.  This is be-
 * cause e.g. type X may be defined in schema A, and may be USEd in schema
 * B and renamed to Y (i.e., "USE from A (X as Y)").  Thus, if currSch = B,
 * Y will have to be written out rather than X.  Actually, this concern
 * only applies for SelectNode.  To accommodate those cases, all the signa-
 * tures of STEPwrite(std::string) contain currSch.  (As an additional note,
 * 2D aggregates should make use of currSch in case they are 2D aggrs of
 * selects.  But since currently (3/27/97) the SCL handles 2D+ aggrs using
 * SCLundefined's, this is not implemented.)
 */
const char * STEPnode::STEPwrite( std::string & s, const char * currSch ) {
    (void) s; //unused
    (void) currSch; //unused
    cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
    cerr << "function:  STEPnode::STEPwrite called instead of virtual function.\n"
         << _POC_ << "\n";
    return "";
}

void STEPnode::STEPwrite( ostream & out ) {
    (void) out; //unused
    cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
    cerr << "function:  STEPnode::STEPwrite called instead of virtual function.\n"
         << _POC_ << "\n";
}

