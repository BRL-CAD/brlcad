#ifndef INSTMGRHELPER_H
#define INSTMGRHELPER_H

#include <lazyInstMgr.h>
#include <instmgr.h>

/**
 * \file instMgrHelper.h helper classes for the lazyInstMgr. Allows use of SDAI_Application_instance class
 * without modification.
 */


/**
 * This class is used when creating SDAI_Application_instance's and using a lazyInstMgr. It is returned
 * by instMgrAdapter. SDAI_Application_instance only uses the GetSTEPentity function.
 */
class mgrNodeHelper: protected MgrNode {
    protected:
        lazyInstMgr * _lim;
        instanceID _id;
    public:
        mgrNodeHelper( lazyInstMgr * lim ) {
            _lim = lim;
            _id = 0;
        }
        inline void setInstance( instanceID id ) {
            _id = id;
        }
        inline SDAI_Application_instance * GetSTEPentity() {
//         unsigned int c = _lim->countDataSections();
            return _lim->loadInstance( _id );
        }
};


/**
 * This class is used when creating SDAI_Application_instance's and using a lazyInstMgr.
 *
 * Instances need an InstMgr to look up the instances they refer to. This class pretends to be a normal InstMgr;
 * when an instance is looked up, this uses lazyInstMgr to load it, and then returns a pointer to it.
 */

class instMgrAdapter: public InstMgr {
    protected:
        mgrNodeHelper _mn;
    public:
        instMgrAdapter( lazyInstMgr * lim ): InstMgr( 0 ), _mn( lim ) {}

        inline mgrNodeHelper * FindFileId( int fileId ) {
            _mn.setInstance( fileId );
            return &_mn;
        }
};


#endif //INSTMGRHELPER_H

