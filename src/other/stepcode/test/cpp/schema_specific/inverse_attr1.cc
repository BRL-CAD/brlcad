/** \file inverse_attr.cc
** 1-Jul-2012
** Test inverse attributes; uses a tiny schema similar to a subset of IFC2x3
**
*/
#include <sc_cf.h>
#include "sc_version_string.h"
#include "SubSuperIterators.h"
#include <STEPfile.h>
#include <sdai.h>
#include <STEPattribute.h>
#include <ExpDict.h>
#include <Registry.h>
#include <errordesc.h>
#include <algorithm>
#include <string>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <sc_getopt.h>
#include "schema.h"

///first way of finding inverse attrs
bool findInverseAttrs1(InverseAItr iai, InstMgr &instList)
{
    const Inverse_attribute *ia;
    int j = 0;
    while(0 != (ia = iai.NextInverse_attribute())) {
        cout << "inverse attr #" << j << ", name: " << ia->Name() << ", inverted attr id: " << ia->inverted_attr_id_()
             << ", from entity: " << ia->inverted_entity_id_() << endl;

        //now find the entity containing the attribute in question

        SdaiReldefinesbytype *rdbt;
        int ent_id = 0;
        while(0 != (rdbt = (SdaiReldefinesbytype *) instList.GetApplication_instance("reldefinesbytype", ent_id))) {
            int i =  rdbt->StepFileId();
            if(i < ent_id) {
                break;
            }
            EntityAggregate *relObj = rdbt->relatedobjects_();
            if(!(relObj && (relObj->is_null() == 0))) {
                return false;
            } else {
                EntityNode *en = (EntityNode *) relObj->GetHead();
                SdaiObject *obj = (SdaiObject *) en->node;
                cout << "file id " << obj->StepFileId() << "; name "
                     << instList.GetApplication_instance(obj->StepFileId() - 1)->eDesc->Name() << endl;
            }
            ent_id = i;
        }
        j++;
    }
    return(j != 0);
}

int main(int argc, char *argv[])
{
    Registry  registry(SchemaInit);
    InstMgr   instance_list;
    STEPfile  sfile(registry, instance_list, "", false);
    bool inverseAttrsFound = false;
    if(argc != 2) {
        cout << "wrong args" << endl;
        exit(EXIT_FAILURE);
    }
    sfile.ReadExchangeFile(argv[1]);

    if(sfile.Error().severity() <= SEVERITY_INCOMPLETE) {
        sfile.Error().PrintContents(cout);
        exit(EXIT_FAILURE);
    }

//find inverse attribute descriptors
    //first, find inverse attrs unique to this entity (i.e. not inherited)
    const EntityDescriptor *ed = registry.FindEntity("window");
    InverseAItr iaIter(&(ed->InverseAttr()));     //iterator for inverse attributes
    if(findInverseAttrs1(iaIter, instance_list)) {
        inverseAttrsFound = true;
    }

    //now, find inherited inverse attrs
    supertypesIterator iter(ed);
    const EntityDescriptor *super;
    for(; !iter.empty(); iter++) {
        super = iter.current();
        cout << "supertype " << super->Name() << endl;
        InverseAItr superIaIter(&(super->InverseAttr()));
        if(findInverseAttrs1(superIaIter, instance_list)) {
            inverseAttrsFound = true;
        }
    }
    if(!inverseAttrsFound) {
        cout << "no inverse attrs found" << endl;
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}
