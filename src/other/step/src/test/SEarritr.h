/*
 * SEarritr.h
 *
 * Ian Soboroff, NIST
 * June, 1994
 *
 * This is an iterator class for arrays of STEPentities.  Given a
 * STEPentity** and its size, you can create one of these, with which
 * you can neatly iterate through the array.  Since it's an array of
 * _pointers_ to STEPentities, they can point to anything derived from
 * that class, i.e., whatever entities you've drawn up in the schema.
 *
 */

// SEarrIterator -- an iterator class to walk arrays of pointers to STEP
// entities (class STEPEntity).
// Best used in the form:  for (SEitr=0; !SEitr; ++SEitr) { ... }
// (this for loop walks the whole array)

class SEarrIterator 
{
 private:
    const STEPentity** SEarr;  // Array of pointers to STEPentity's
    int index;           // current index
    int size;            // size of array
 public:
    // Construct the iterator with a given array and its size
    SEarrIterator(const STEPentity** entarr, int sz)
        { SEarr = entarr; index = 0; size = sz; }
    
    // set the value of the index: SEitr = 3
    void operator= (int newindex) { index = newindex; }
    
    // check if we're out of range: if (!SEitr)...
    int operator! () { return (index < size); }

    // return current element: SEptr = SEitr()
    STEPentity* operator() () { return (STEPentity*)SEarr[index]; }

    // PREFIX increment operator: ++SEitr
    int operator++ () { index++; return operator! (); }
};
