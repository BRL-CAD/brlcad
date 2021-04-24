#include <iostream>
#include <string>
#include "lazyInstMgr.h"
#include "SdaiSchemaInit.h"
#include "sc_memmgr.h"
#include "sc_benchmark.h"
#include <sc_cf.h>

#ifndef NO_REGISTRY
# include "schema.h"
#endif //NO_REGISTRY


void fileInfo(lazyInstMgr &mgr, fileID id)
{
    instancesLoaded_t *headerInsts = mgr.getHeaderInstances(id);
    SDAI_Application_instance *hdrInst;
    hdrInst = headerInsts->find(3);
    if((hdrInst != 0) && (hdrInst->STEPfile_id == 3)) {
        SdaiFile_schema *fs = dynamic_cast< SdaiFile_schema * >(hdrInst);
        if(fs) {
            StringAggregate_ptr p = fs->schema_identifiers_();
            StringNode *sn = (StringNode *) p->GetHead();
            std::cout << "Schema(s): ";
            while(sn) {
                std::cout << sn->value.c_str() << " ";
                sn = (StringNode *) sn->NextNode();
            }
            std::cout << std::endl;
        }
    }
}

void countTypeInstances(lazyInstMgr &mgr, std::string type)
{
    int count = mgr.countInstances(type);
    std::cout << type << " instances: " << count;
    if(count) {
        instanceID ex;
        ex = (* mgr.getInstances(type))[ 0 ];
        std::cout << " -- example: #" << ex;
    }
    std::cout << std::endl;
    return;
}

/// Called twice by printRefs. Returns the instanceID of one instance that has a reference.
instanceID printRefs1(instanceRefs_t *refs, bool forward)
{
    const char *d1 = forward ? "forward" : "reverse";
    const char *d2 = forward ? " refers to " : " is referred to by ";
    instanceID id = 0;
    instanceRefs_t::cpair p = refs->begin();
    instanceRefs_t::cvector *v = p.value;
    if(!v) {
        std::cout << "No " << d1 << " references" << std::endl;
    } else {
        instanceRefs_t::cvector::const_iterator it(v->begin()), end(v->end());
        std::cout << "Example of " << d1 << " references - Instance #" << p.key << d2 << v->size() << " other instances: ";
        for(; it != end; it++) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;
        id = p.key;
    }
    return id;
}

///prints references; returns the instanceID for one instance that has a forward reference
instanceID printRefs(lazyInstMgr &mgr)
{
    instanceID id;
    std::cout << "\nReferences\n==============\n";
    id = printRefs1(mgr.getFwdRefs(), true);
    printRefs1(mgr.getRevRefs(), false);
    std::cout << std::endl;
    return id;
}

/// prints dependencies of an instance
void printDeps(lazyInstMgr &mgr)
{
    const int displayInstances = 10;
    instanceRefs_t *refs = mgr.getFwdRefs();
    instanceRefs_t::cpair p = refs->end();

    instanceID id = p.key;
    instanceSet *dependencies = mgr.instanceDependencies(id);

    std::cout << std::endl << "Dependencies" << std::endl << "==============" << std::endl;
    instanceSet::const_iterator it(dependencies->begin()), end(dependencies->end());

    std::cout << "Example: Instance #" << id << " is recursively dependent on " << dependencies->size() << " instances: ";
    int i;
    for(i = 0; it != end && i < displayInstances; it++, i++) {
        std::cout << *it << " ";
    }

    if(dependencies->size() > displayInstances) {
        std::cout << ".... (Displaying only " << displayInstances << " instances) ";
    }

    std::cout << std::endl;
    std::cout << std::endl;

    delete dependencies;
}

///prints info about a complex instance
void dumpComplexInst(STEPcomplex *c)
{
    int depth = 0;
    if(c) {
//         std::cout << "attr list size: " << c->_attr_data_list.size() << ", depth " << depth << std::endl;
//         STEPcomplex_attr_data_list::iterator it;
//         for( it = c->_attr_data_list.begin(); it != c->_attr_data_list.end(); it++ ) {
//             std::cout << "*** Not printing complex instance attribute info - many eDesc pointers are invalid. ***" << std::endl; //FIXME!
//             SDAI_Application_instance * attr = ( SDAI_Application_instance * ) *it;
        STEPcomplex *complex = c->head;
        while(complex) {
            if(complex->IsComplex()) {
                std::cout << "Complex component " << complex->eDesc->Name() << " at depth " << depth << " with attr list size ";
                std::cout << complex->_attr_data_list.size() << std::endl;
//                 dumpComplexInst( complex, depth + 1 );
            } else {
                //probably won't ever get here...
                SDAI_Application_instance *ai = dynamic_cast< SDAI_Application_instance * >(complex);
                if(ai) {
                    std::cout << "non-complex component at depth " << depth << ", " << ai->eDesc->Name() << std::endl;
                } else {
                    std::cout << "unknown component at depth " << depth << ": " << complex << std::endl;
                }
            }
            complex = complex->sc;
            depth++;
        }
    }
}

int main(int argc, char **argv)
{
    if(argc != 2) {
        std::cerr << "Expected one argument, given " << argc - 1 << ". Exiting." << std::endl;
        exit(EXIT_FAILURE);
    }
    lazyInstMgr *mgr = new lazyInstMgr;
#ifndef NO_REGISTRY
    //init schema
    mgr->initRegistry(SchemaInit);
#endif //NO_REGISTRY

    instanceID instWithRef;
    benchmark stats;
    std::cout << "================ p21 lazy load: scanning the file ================\n";
    mgr->openFile(argv[1]);
    stats.stop();
    benchVals scanStats = stats.get();
    stats.out();

    std::cout << "================ p21 lazy load: gathering statistics ================\n";
    stats.reset();

    int instances = mgr->totalInstanceCount();
    std::cout << "Total instances: " << instances << " (" << (float)(scanStats.userMilliseconds * 1000) / instances << "us per instance, ";
    std::cout << (float)(scanStats.physMemKB * 1000) / instances << " bytes per instance)" << std::endl << std::endl;

    fileInfo(*mgr, 0);
    //these are just common types
    countTypeInstances(*mgr, "CARTESIAN_POINT");
    countTypeInstances(*mgr, "POSITIVE_LENGTH_MEASURE");
    countTypeInstances(*mgr, "VERTEX_POINT");

    //complex instances
    std::cout << "Complex";
    countTypeInstances(*mgr, "");

    std::cout << "Longest type name: " << mgr->getLongestTypeName() << std::endl;
//     std::cout << "Total types: " << mgr->getNumTypes() << std::endl;

    instWithRef = printRefs(*mgr);
    printDeps(*mgr);

#ifndef NO_REGISTRY
    if(instWithRef) {
        std::cout << "Number of data section instances fully loaded: " << mgr->loadedInstanceCount() << std::endl;
        std::cout << "Loading #" << instWithRef;
        SDAI_Application_instance *inst = mgr->loadInstance(instWithRef);
        std::cout << " which is of type " << inst->EntityName() << std::endl;
        std::cout << "Number of instances loaded now: " << mgr->loadedInstanceCount() << std::endl;
    }

    instanceTypes_t::cvector *complexInsts = mgr->getInstances("");
    if(complexInsts && complexInsts->size() > 0) {
        std::cout << "loading complex instance #" << complexInsts->at(0) << "." << std::endl;
        STEPcomplex *c = dynamic_cast<STEPcomplex *>(mgr->loadInstance(complexInsts->at(0)));
        dumpComplexInst(c);
        std::cout << "Number of instances loaded now: " << mgr->loadedInstanceCount() << std::endl;
    }
#else
    (void) instWithRef; // unused
#endif //NO_REGISTRY

    stats.out();
    std::cout << "================ p21 lazy load: freeing memory ================\n";
    stats.reset();
    delete mgr;
    //stats will print from its destructor
}

