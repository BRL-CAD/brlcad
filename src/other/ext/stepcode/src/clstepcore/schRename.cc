#include "schRename.h"


/**
 * See if nm = one of our choices (either ours or that of a SchRename
 * later in the list.
 */
bool SchRename::choice( const char * nm ) const {
    if( !StrCmpIns( nm, newName ) ) {
        return true;
    }
    if( next ) {
        return ( next->choice( nm ) );
    }
    return false;
}

/**
 * Check if this SchRename represents the rename of its owning TypeDesc for
 * schema schnm.  (This will be the case if schnm = schName.)  If so, the
 * new name is returned and copied into newnm.  If not, ::rename is called
 * on next.  Thus, this function will tell us if this or any later SchRe-
 * name in this list provide a new name for TypeDesc for schema schnm.
 */
char * SchRename::rename( const char * schnm, char * newnm ) const {
    if( !StrCmpIns( schnm, schName ) ) {
        strcpy( newnm, newName );
        return newnm;
    }
    if( next ) {
        return ( next->rename( schnm, newnm ) );
    }
    return NULL;
}
