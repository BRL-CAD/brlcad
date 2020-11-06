
#ifndef mgrnodearray_h
#define mgrnodearray_h

/*
* NIST STEP Editor Class Library
* cleditor/mgrnodearray.h
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id$ */

#include "export.h"

#include <string.h>

#include "utils/gennode.h"
#include "utils/gennodelist.h"

#include "core/mgrnode.h"
#include "core/mgrnodelist.h"

#include "utils/gennodearray.h"

#define ARRAY_DEFAULT_SIZE (1024)

//////////////////////////////////////////////////////////////////////////////
// class MgrNodeArray
// This class implements the array of MgrNodes representing the
// master list for the seInstMgr which will have a one to one
// mapping by index to the display list of corresponding STEPentities.
// If you delete this object it deletes all of the entries it points to.
//////////////////////////////////////////////////////////////////////////////

class STEPCODE_CORE_EXPORT MgrNodeArray : public GenNodeArray {
    public:
        MgrNodeArray( int defaultSize = ARRAY_DEFAULT_SIZE );
        ~MgrNodeArray();

// REDEFINED functions
        // need to redefine Append() & Insert(GenericNode *) so they call
        //  MgrNodeArray::Insert(GenericNode *, int index);
        virtual int Insert( GenericNode * gn, int index );
        virtual void Append( GenericNode * gn )    {
            Insert( gn, _count );
        }
        virtual int Insert( GenericNode * gn )     {
            return Insert( gn, _count );
        }
        virtual void Remove( int index );
        virtual void ClearEntries();
        virtual void DeleteEntries();

// ADDED functions
        virtual int MgrNodeIndex( int fileId );
        void AssignIndexAddress( int index );
};

//////////////////////////////////////////////////////////////////////////////
// class MgrNodeArraySorted
// This class implements the array of MgrNodes representing the
// sorted master list for the seInstMgr.  This list is sorted by
// STEPentity::fileId.
// If you delete this object it won't delete the entries it points to.
//////////////////////////////////////////////////////////////////////////////

class STEPCODE_CORE_EXPORT MgrNodeArraySorted : public GenNodeArray {
    public:
        MgrNodeArraySorted( int defaultSize = ARRAY_DEFAULT_SIZE );
        ~MgrNodeArraySorted() { }

// REDEFINED functions
        virtual int Index( GenericNode * gn );
        virtual int Index( GenericNode ** gn );

        virtual int Insert( GenericNode * gn );
        virtual int Insert( GenericNode * gn, int index ) {
		std::cerr <<
                 "Call MgrNodeArraySorted::Insert() without index argument instead.\n"
                 << "index argument: " << index << " being ignored.\n";
            return Insert( gn );
        }
        virtual void Append( GenericNode * gn )        {
            Insert( gn );
        }
        virtual void ClearEntries();
        virtual void DeleteEntries();

// ADDED functions
        virtual int MgrNodeIndex( int fileId );
        int FindInsertPosition( const int fileId );
};


//////////////////////////////////////////////////////////////////////////////
// class MgrNodeArray inline public functions
//////////////////////////////////////////////////////////////////////////////

#endif
