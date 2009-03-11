/*****************************************************************************
 * write.cc                                                                  *
 *                                                                           *
 * Description: Contains the write() functions through which the complex     *
 *              structures can write themselves to file.  More specifically, *
 *              the structures generate code in an output file which can be  *
 *              used to recreate the same structures.                        *
 *                                                                           *
 * Created by:  David Rosenfeld                                              *
 * Date:        01/09/97                                                     *
 *****************************************************************************/

#include "complexSupport.h"

// Local function prototypes:
static void writeheader( ostream &, int );

void print_complex( ComplexCollect &collect, char *filename )
    /*
     * Standalone function called from fedex_plus.  Takes a ComplexCollect
     * and writes its contents to a file (filename) which can be used to
     * recreate this CCollect.
     */
{
    ComplexList *cl;

#ifdef COMPLEX_INFO
    if ( collect.clists ) {
	// If there's something in this collect, print it out:
	cout << "\nHere's everything:\n";
	for ( cl = collect.clists; cl != NULL; cl = cl->next ) {
	    cout << *cl << endl;
	}
    }
#endif
    collect.write( filename );
}

void ComplexCollect::write( char *fname )
    /*
     * Generates C++ code in os which may be compiled and run to create a
     * ComplexCollect structure.  Functions are called to write out the
     * ComplexList structures contained in this.
     */
{
    ofstream complex;
    ComplexList *clist = clists;

    // Open the stream:
    complex.open( fname );
    if ( !complex ) {
	cerr << "ERROR: Could not create output file " << fname << endl;
	// yikes this is pretty drastic, Sun C++ doesn't like this anyway DAS
//	exit(-1);
	return;
    }
    writeheader( complex, clists == NULL );

    // If there's nothing in this, make function a stub (very little was
    // printed in writeheader() also):
    if ( clists == NULL ) {
	complex << "    return 0;" << endl;
	complex << "}" << endl;
	complex.close();
	return;
    }
	
    // First write to os the variables it will need:
    complex << "    ComplexCollect *cc;\n";
    complex << "    ComplexList *cl;\n";
    complex << "    EntList *node, *child, *next1, *next2, *next3, *next4,\n"
            << "            *next5, *next6, *next7, *next8, *next9, *next10,\n"
            << "            *next11, *next12, *next13, *next14, *next15,\n"
            << "            *next16, *next17, *next18, *next19, *next20,\n"
            << "            *next21, *next22, *next23, *next24, *next25,\n"
            << "            *next26, *next27, *next28, *next29, *next30;"
	    << endl << endl;
    // Frankly, I'd hate to see a schema which needs so many variables.  But
    // AP210 gets to `next23'.

    // Next create the CCollect and CLists:
    complex << "    cc = new ComplexCollect;\n";
    while ( clist ) {
	complex << endl;
	complex << "    // ComplexList with supertype \"" << clist->supertype()
		<< "\":\n";
        clist->write( complex );
        complex << "    cc->insert( cl );\n";
        clist = clist->next;
    }
    
    // Close up:
    complex << "\n    return cc;\n";
    complex << "}" << endl;
    complex.close();
}

static void writeheader( ostream &os, int noLists )
    /*
     * Writes the header for the complex file.
     */
{
    // If there are no ComplexLists in the ComplexCollect, make this function
    // a stub:
    if ( noLists ) {
	os << "/*" << endl
	   << " * This file normally contains instantiation statements to\n"
	   << " * create complex support structures.  For the current EXPRESS\n"
	   << " * file, however, there are no complex entities, so this\n"
	   << " * function is a stub.\n"
	   << " */" << endl << endl;
	os << "#include \"complexSupport.h\"\n\n";
	os << "ComplexCollect *gencomplex()" << endl;
	os << "{" << endl;
	return;
    }

    // Otherwise, write file and function comments:
    os << "/*" << endl
       << " * This file contains instantiation statements to create complex\n"
       << " * support structures.  The structures will be used in the SCL to\n"
       << " * validate user requests to instantiate complex entities.\n"
       << " */" << endl << endl;
    os << "#include \"complexSupport.h\"\n\n";
    os << "ComplexCollect *gencomplex()" << endl;
    os << "    /*" << endl
       << "     * This function contains instantiation statments for all the\n"
       << "     * ComplexLists and EntLists in a ComplexCollect.  The instan-\n"
       << "     * stiation statements were generated in order of lower to\n"
       << "     * higher, and last to first to simplify creating some of the\n"
       << "     * links between structures.  Because of this, the code is not\n"
       << "     * very readable, but does the trick.\n"
       << "     */" << endl;
    os << "{" << endl;
}

void ComplexList::write( ostream &os )
    /*
     * Generates C++ code in os which will create an instantiation of a CList
     * which will recreate this.
     */
{
    head->write( os );
    os << "    cl = new ComplexList((AndList *)node);\n";
    os << "    cl->buildList();\n";
    os << "    cl->head->setLevel( 0 );\n";
}

void MultList::write( ostream &os )
    /*
     * Writes to os code to instantiate a replica of this.  Does so by first
     * recursing to replicate this's children, and then instantiating this.
     * When write() is finished, the "node" variable in os will point to this.
     */
{
    EntList *child = getLast();

    // First write our children, from last to first.  (We go in backwards order
    // so that "node" (a variable name in the os) will = our first child when
    // this loop is done.  See below.)
    child->write( os );
    while ( child->prev ) {
        // Whenever an EntList::write() function is called, it writes to os
        // an instantiation statement basically of the form "node = new XXX-
        // List;".  So we know that in the output file (os) the newly-created
        // EntList is pointed to by variable node.
        os << "    next" << level+1 << " = node;\n";
        child = child->prev;
        child->write( os );
        os << "    next" << level+1 << "->prev = node;\n";
        os << "    node->next = next" << level+1 <<";\n";
    }
    
    // Now write out this:
    os << "    child = node;\n";
    // "node" was set to the last child list we just added (which is the first
    // of our children).  Now we set the variable "child" to it so we can reset
    // node.  We do this so that node will = this when we're done and return
    // to the calling function (so the calling fn can make the same assumption
    // we just did).
    if ( join == AND ) {
        os << "    node = new AndList;\n";
    } else if ( join == ANDOR ) {
        os << "    node = new AndOrList;\n";
    } else {
        os << "    node = new OrList;\n";
    }
    os << "    ((MultList *)node)->appendList( child );\n";
    // The above line will set node's childList and numchidren count.
}

void SimpleList::write( ostream &os )
    /*
     * Writes to os a statement to instantiate this.
     */
{
    os << "    node = new SimpleList( \"" << name << "\" );\n";
}
