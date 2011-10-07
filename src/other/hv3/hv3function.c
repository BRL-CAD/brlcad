
/*
 * The code in this file overloads the default Function constructor
 * object provided by SEE. This is because in ECMAscript proper, 
 * functions declared using the "new Function()" syntax are created
 * with only the Global object in scope. However in browsers,
 * it is necessary to create them with the Global object and the
 * current Window context.
 *
 * The hackish fix here is to add a "Function" property to the Window
 * object that acts exactly like the "Function" property of the 
 * Global object, except that functions are created in the Window
 * scope.
 *
 * Much of the code here is cut'n'paste from the SEE source file
 * obj_Function.c.
 */
 

/* Internal SEE functions we need for this hack */
struct function *SEE_parse_function(
    struct SEE_interpreter *i,
    struct SEE_string *name, 
    struct SEE_input *param_input,
    struct SEE_input *body_input
);
struct SEE_object *SEE_function_inst_create(
    struct SEE_interpreter *i,
    struct function *f, 
    struct SEE_scope *scope
);


/* 15.3.2.1 new Function(...) */
static void
function_construct(interp, self, thisobj, argc, argv, res)
    struct SEE_interpreter *interp;
    struct SEE_object *self, *thisobj;
    int argc;
    struct SEE_value **argv, *res;
{
    struct SEE_string *P, *body;
    struct SEE_value r9, r13;
    struct SEE_input *paraminp, *bodyinp;
    int k;

    struct function *f;
    struct SEE_object *pFunction;
    struct SEE_scope *pScope = ((SeeInterp *)interp)->pScope;
    if (!pScope) {
        pScope = interp->Global_scope;
    }

    P = SEE_string_new(interp, 0);
    for (k = 0; k < argc - 1; k++) {
        if (k)
            SEE_string_addch(P, ',');
        SEE_ToString(interp, argv[k], &r9);
        SEE_string_append(P, r9.u.string);
    }
    if (argc) {
        SEE_ToString(interp, argv[argc - 1], &r13);
        body = r13.u.string;
    } else
        body = SEE_string_new(interp, 0);

    paraminp = SEE_input_string(interp, P);
    bodyinp = SEE_input_string(interp, body);

    f = SEE_parse_function(interp, NULL, paraminp, bodyinp);
    pFunction = SEE_function_inst_create(interp, f, pScope);
    SEE_SET_OBJECT(res, pFunction);
    SEE_INPUT_CLOSE(bodyinp);
    SEE_INPUT_CLOSE(paraminp);
}

/* object class for Function constructor */
static struct SEE_objectclass function_const_class = {
    "FunctionConstructor",                  /* Class */
    SEE_native_get,                         /* Get */
    SEE_native_put,                         /* Put */
    SEE_native_canput,                      /* CanPut */
    SEE_native_hasproperty,                 /* HasProperty */
    SEE_native_delete,                      /* Delete */
    SEE_native_defaultvalue,                /* DefaultValue */
    SEE_native_enumerator,                  /* Enumerator */
    function_construct,                     /* Construct */
    function_construct                      /* Call */
};

static void 
interpFunctionInit(pInterp, pObj)
    SeeInterp *pInterp; 
    SeeTclObject *pObj;
{
    struct SEE_interpreter *interp = &pInterp->interp;
    struct SEE_object *pFunction;
    struct SEE_object *pNative = (struct SEE_object *)(pObj->pNative);

    struct SEE_value v;

    pFunction = (struct SEE_object *)SEE_NEW(interp, struct SEE_native);
    SEE_native_init(
        (struct SEE_native *)(pFunction),
        &pInterp->interp,
        &function_const_class, 
        interp->Function_prototype
    );

    SEE_SET_OBJECT(&v, pFunction);
    SEE_OBJECT_PUTA(interp, pNative, "Function", &v, SEE_ATTR_DONTENUM);

    SEE_SET_NUMBER(&v, 1);
    SEE_OBJECT_PUTA(interp, pFunction, "length", &v, SEE_ATTR_LENGTH);
}

