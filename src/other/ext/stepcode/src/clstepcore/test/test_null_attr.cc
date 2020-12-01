#include <ExpDict.h>
#include <STEPattribute.h>
#include <sdaiString.h>

AttrDescriptor   *ad  = 0;
EntityDescriptor *ed  = 0;
TypeDescriptor   *td;
Schema           *sch = 0;

int main () {
    SDAI_String _description;
    sch = new Schema( "Ifc2x3" );
    td = new TypeDescriptor( "Ifctext", sdaiSTRING, sch, "STRING" );
    ed = new EntityDescriptor( "Ifcroot", sch, LTrue, LFalse );
    ad = new AttrDescriptor( "description", td, LTrue, LFalse, AttrType_Explicit, *ed );
    ed->AddExplicitAttr( ad );
    STEPattribute *a = new STEPattribute(*ad,  &_description);
    a -> set_null();
    delete a;
    //delete ad; //this is deleted in EntityDescriptor dtor
    delete ed;
    delete td;
    delete sch;
}
