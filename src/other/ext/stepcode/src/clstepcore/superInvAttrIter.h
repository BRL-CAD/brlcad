#ifndef SUPERINVATTRITER_H
#define SUPERINVATTRITER_H

#include "SubSuperIterators.h"
#include "ExpDict.h"

/**
 * this class implements an iterator for inverse attributes in an EntityDescriptor's supertypes
 * makes use of supertypesIterator and InverseAItr
 *
 * TODO verify that this iterates correctly!
 */
class superInvAttrIter {
protected:
    supertypesIterator sit;
    InverseAItr * invIter;
    const Inverse_attribute * nextInv;
    bool isempty; ///< if true, don't try to access invIter - it is not initialized
public:
    /// WARNING this will not iterate over the ia's in the first ed, only in its supertypes! change that?
    superInvAttrIter( const EntityDescriptor * ed ): sit( ed ), invIter(0), nextInv( 0 ), isempty( false ) {
        reset();
    }
    void reset( const EntityDescriptor * ed = 0 ) {
        sit.reset( ed );
        if( invIter ) {
            delete invIter;
            invIter = 0;
        }
        if( sit.empty() ) {
            isempty = true;
        } else {
            invIter = new InverseAItr( &( sit.current()->InverseAttr() ) );
            nextInv = invIter->NextInverse_attribute();
            if( !nextInv ) {
                next();
            }
        }
    }
    ~superInvAttrIter() {
        if( invIter ) {
            delete invIter;
            invIter = 0;
        }
    }
    const EntityDescriptor * currentEDesc() {
        if( isempty ) {
            return NULL;
        }
        return sit.current();
    }
    bool empty() {
        if( isempty ) {
            return true;
        }
        return ( !sit.hasNext() && !nextInv );
    }
    const Inverse_attribute * next() {
        if( isempty ) {
            return NULL;
        }
        const Inverse_attribute * ia = nextInv;
        /* if we're on the last inverse attr for the current super, go to the next super
         * keep going until we find an ia or run out of supers */
        while( ( 0 == ( nextInv = invIter->NextInverse_attribute() ) ) && sit.hasNext() ) {
            invIter->ResetItr( &( sit.next()->InverseAttr() ) );
        }
        return ia;
    }
};

#endif //SUPERINVATTRITER_H
