
#ifndef gennodearray_h
#define gennodearray_h

/*
* NIST Utils Class Library
* clutils/gennode.h
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: gennodearray.h,v 3.0.1.3 1997/11/05 22:33:47 sauderd DP3.1 $  */ 

/*
 * GenNodeArray - dynamic array object of GenericNodes.
 * the array part of this is copied from Unidraws UArray - dynamic array object
 * Copyright (c) 1990 Stanford University
 */

/*
#ifdef __OSTORE__
#include <ostore/ostore.hh>    // Required to access ObjectStore Class Library
#endif
*/

#ifdef __O3DB__
#include <OpenOODB.h>
#endif

#include <string.h>
#include <memory.h>
#include <stdlib.h> // to get bcopy for CenterLine

#include <gennode.h>

	// the initial size of the array
#define ARRAY_DEFAULT_SIZE (1024)

//////////////////////////////////////////////////////////////////////////////
// GenNodeArray
// If you delete this object, it does not delete the entries it points to.
// If you want it to delete the entries it points to you need to call
// DeleteEntries().
//////////////////////////////////////////////////////////////////////////////

class GenNodeArray 
{
public:

//#ifdef __OSTORE__
//    GenNodeArray (os_database *db, int defaultSize = ARRAY_DEFAULT_SIZE);
//#else
    GenNodeArray(int defaultSize = ARRAY_DEFAULT_SIZE);
//#endif
    virtual ~GenNodeArray();

    GenericNode*& operator[](int index);
    virtual int Index(GenericNode* gn);
    virtual int Index(GenericNode** gn);

    int Count();

    virtual void Append(GenericNode* gn);
    virtual int Insert(GenericNode* gn);
    virtual int Insert(GenericNode* gn, int index);
    virtual void Remove(int index);
    virtual void ClearEntries();
    virtual void DeleteEntries();

/*
#ifdef __OSTORE__
    static os_typespec* get_os_typespec();
#endif
*/
protected:
    virtual void Check(int index);

    GenericNode** _buf;	// the array
    int _bufsize;	// the possible number of entries in the array
    int _count;		// the number of entries in the array
};

//////////////////////////////////////////////////////////////////////////////
// class GenNodeArray inline public functions
//////////////////////////////////////////////////////////////////////////////

inline GenericNode*& GenNodeArray::operator[] (int index) 
{
    Check(index);
    return _buf[index];
}

inline int GenNodeArray::Count ()
{
    return _count;
}

#endif
