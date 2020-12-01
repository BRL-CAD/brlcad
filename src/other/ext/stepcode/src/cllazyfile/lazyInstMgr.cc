#include "lazyTypes.h"
#include "lazyInstMgr.h"
#include "Registry.h"
#include <SubSuperIterators.h>
#include "SdaiSchemaInit.h"
#include "instMgrHelper.h"
#include "lazyRefs.h"

#include "sdaiApplication_instance.h"

lazyInstMgr::lazyInstMgr() {
    _headerRegistry = new Registry( HeaderSchemaInit );
    _instanceTypes = new instanceTypes_t( 255 ); //NOTE arbitrary max of 255 chars for a type name
    _lazyInstanceCount = 0;
    _loadedInstanceCount = 0;
    _longestTypeNameLen = 0;
    _mainRegistry = 0;
    _errors = new ErrorDescriptor();
    _ima = new instMgrAdapter( this );
}

lazyInstMgr::~lazyInstMgr() {
    delete _headerRegistry;
    delete _errors;
    delete _ima;
    //loop over files, sections, instances; delete header instances
    lazyFileReaderVec_t::iterator fit = _files.begin();
    for( ; fit != _files.end(); ++fit ) {
        delete *fit;
    }
    dataSectionReaderVec_t::iterator sit = _dataSections.begin();
    for( ; sit != _dataSections.end(); ++sit ) {
        delete *sit;
    }
    _instancesLoaded.clear();
    _instanceStreamPos.clear();
}

sectionID lazyInstMgr::registerDataSection( lazyDataSectionReader * sreader ) {
    _dataSections.push_back( sreader );
    return _dataSections.size() - 1;
}

void lazyInstMgr::addLazyInstance( namedLazyInstance inst ) {
    _lazyInstanceCount++;
    assert( inst.loc.begin > 0 && inst.loc.instance > 0 );
    int len = strlen( inst.name );
    if( len > _longestTypeNameLen ) {
        _longestTypeNameLen = len;
        _longestTypeName = inst.name;
    }
    _instanceTypes->insert( inst.name, inst.loc.instance );
    /* store 16 bits of section id and 48 of instance offset into one 64-bit int
    ** TODO: check and warn if anything is lost (in calling code?)
    ** does 32bit need anything special?
    **
    ** create conversion class?
    **  could then initialize conversion object with number of bits
    **  also a good place to check for data loss
    */
    positionAndSection ps = inst.loc.section;
    ps <<= 48;
    ps |= ( inst.loc.begin & 0xFFFFFFFFFFFFULL );
    _instanceStreamPos.insert( inst.loc.instance, ps );

    if( inst.refs ) {
        if( inst.refs->size() > 0 ) {
            //forward refs
            _fwdInstanceRefs.insert( inst.loc.instance, *inst.refs );
            instanceRefs::iterator it = inst.refs->begin();
            for( ; it != inst.refs->end(); ++it ) {
                //reverse refs
                _revInstanceRefs.insert( *it, inst.loc.instance );
            }
        } else {
            delete inst.refs;
        }
    }
}

unsigned long lazyInstMgr::getNumTypes() const {
    unsigned long n = 0 ;
    instanceTypes_t::cpair curr, end;
    end = _instanceTypes->end();
    curr = _instanceTypes->begin();
    if( curr.value != 0 ) {
        n = 1;
        while( curr.value != end.value ) {
            n++;
            curr = _instanceTypes->next();
        }
    }
    return n ;
}

void lazyInstMgr::openFile( std::string fname ) {
    //don't want to hold a lock for the entire time we're reading the file.
    //create a place in the vector and remember its location, then free lock
    ///FIXME begin atomic op
    size_t i = _files.size();
    _files.push_back( (lazyFileReader * ) 0 );
    ///FIXME end atomic op
    lazyFileReader * lfr = new lazyFileReader( fname, this, i );
    _files[i] = lfr;
    /// TODO resolve inverse attr references
    //between instances, or eDesc --> inst????
}

SDAI_Application_instance * lazyInstMgr::loadInstance( instanceID id, bool reSeek ) {
    assert( _mainRegistry && "Main registry has not been initialized. Do so with initRegistry() or setRegistry()." );
    std::streampos oldPos;
    positionAndSection ps;
    sectionID sid;
    SDAI_Application_instance * inst = _instancesLoaded.find( id );
    if( inst ) {
        return inst;
    }
    instanceStreamPos_t::cvector * cv;
    if( 0 != ( cv = _instanceStreamPos.find( id ) ) ) {
        switch( cv->size() ) {
            case 0:
                std::cerr << "Instance #" << id << " not found in any section." << std::endl;
                break;
            case 1:
                long int off;
                ps = cv->at( 0 );
                off = ps & 0xFFFFFFFFFFFFULL;
                sid = ps >> 48;
                assert( _dataSections.size() > sid );
                if( reSeek ) {
                    oldPos = _dataSections[sid]->tellg();
                }
                inst = _dataSections[sid]->getRealInstance( _mainRegistry, off, id );
                if( reSeek ) {
                    _dataSections[sid]->seekg( oldPos );
                }
                break;
            default:
                std::cerr << "Instance #" << id << " exists in multiple sections. This is not yet supported." << std::endl;
                break;
        }
        if( !isNilSTEPentity( inst ) ) {
            _instancesLoaded.insert( id, inst );
            _loadedInstanceCount++;
            lazyRefs lr( this, inst );
            lazyRefs::referentInstances_t insts = lr.result();
        } else {
            std::cerr << "Error loading instance #" << id << "." << std::endl;
        }
    } else {
        std::cerr << "Instance #" << id << " not found in any section." << std::endl;
    }
    return inst;
}


instanceSet * lazyInstMgr::instanceDependencies( instanceID id ) {
    instanceSet * checkedDependencies = new instanceSet();
    instanceRefs dependencies; //Acts as queue for checking duplicated dependency

    instanceRefs_t * _fwdRefs = getFwdRefs();
    instanceRefs_t::cvector * _fwdRefsVec = _fwdRefs->find( id );
    //Initially populating direct dependencies of id into the queue
    if( _fwdRefsVec != 0 ) {
        dependencies.insert( dependencies.end(), _fwdRefsVec->begin(), _fwdRefsVec->end() );
    }

    size_t curPos = 0;
    while( curPos < dependencies.size() ) {

        bool isNewElement = ( checkedDependencies->insert( dependencies.at( curPos ) ) ).second;
        if( isNewElement ) {
            _fwdRefsVec = _fwdRefs->find( dependencies.at( curPos ) );
            
            if( _fwdRefsVec != 0 ) {
                dependencies.insert( dependencies.end(), _fwdRefsVec->begin(), _fwdRefsVec->end() );
            }
        }

        curPos++;
    }

    return checkedDependencies;
}

