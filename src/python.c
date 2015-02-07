#include <config.h>
#include <Python.h>

#include "lisp.h"
#include "character.h"
#include "buffer.h"

// forwad declaration
static PyObject *wrap_PyLispObject(Lisp_Object);

static PyObject *main_module;
static PyObject *global_dict;
static PyObject *emacs_dict;

int PyCheckError(const char * err){
    // Should use xsignal
  if(PyErr_Occurred()){
    fprintf(stderr, "ERROR IN: %s\n", err);
    PyErr_Print();
    return 1;
  }
  return 0;
}

DEFUN ("python-call", Fpython_call, Spython_call, 1, 1, "s",
       doc: /* Call a python function with no arguments. */)
   (Lisp_Object expression)
{
    CHECK_STRING (expression);

    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    PyObject *local_dict, *obj, *code, *result;

    local_dict  = PyDict_New();
    if (PyCheckError("creating new locals")) goto fail_local;

    code = Py_CompileString(SSDATA(expression), "", Py_eval_input);
    if (PyCheckError("compiling")) goto fail_compile;

    obj = PyEval_EvalCode((PyCodeObject *) code, global_dict, local_dict);
    if (PyCheckError("evaluating expression")) goto fail_eval;

    result = PyObject_Str(obj);
    if (PyCheckError("converting to string")) goto fail_convert_string;

    char *str_result = PyString_AsString(result);
    if (PyCheckError("getting string")) goto fail_get_string;

    Lisp_Object lisp_result = build_unibyte_string(str_result);

    Py_DECREF(local_dict);
    Py_DECREF(code);
    Py_DECREF(obj);
    Py_DECREF(result);

    PyGILState_Release(gstate);

    return lisp_result;

 fail_get_string:
    Py_DECREF(result);
 fail_convert_string:
    Py_DECREF(obj);
 fail_eval:
    Py_DECREF(code);
 fail_compile:
    Py_DECREF(local_dict);
 fail_local:
    PyGILState_Release(gstate);

    return Qnil;
}

void
syms_of_python (void)
{
    defsubr (&Spython_call);
}

// Add every symbol in an obarray to the globals dictionary, with
// proper name translation
static void add_obj(Lisp_Object sym, Lisp_Object ignore)
{
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    // don't clobber the refresh function
    if (strcmp(SSDATA(XSYMBOL(sym)->name), "refresh")) {
        PyObject *py_sym = wrap_PyLispObject(sym);

        printf("wrapping %s == %d\n", SSDATA(XSYMBOL(sym)->name), (int)sym);
        if (! PyDict_SetItemString(emacs_dict, SSDATA(XSYMBOL(sym)->name), py_sym))
            PyCheckError("adding symbol to emacs module dictionary.");
        // Need to fail better
    }
    PyGILState_Release(gstate);
}

static PyObject *refresh(PyObject *self, PyObject *args)
{
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    if (!PyArg_ParseTuple(args, "")) {
        return NULL;
    }
    map_obarray(Vobarray, add_obj, Qnil);

    PyGILState_Release(gstate);
    Py_RETURN_NONE;
}

static PyMethodDef emacs_funcs[] = {
    {"refresh",  refresh, METH_VARARGS, "Populate emacs module with elisp symbols"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};


//////////////////////////////////////////////////
// Emacs Lisp_Object wrapper
//////////////////////////////////////////////////

typedef struct {
    PyObject_HEAD
    /* Type-specific fields go here. */
    Lisp_Object o;
} PyLispObject;

static PyObject *
PyLispObject_repr(PyLispObject *obj)
{
    printf("printing %d\n", (int) (obj->o));
    if (SYMBOLP(obj->o))
        return PyString_FromFormat("Elisp symbol: %s",
                                   SSDATA(XSYMBOL(obj->o)->name));
    return PyString_FromFormat("Elisp: '%s'",
                               SDATA (Fprin1_to_string (obj->o, Qnil)));
}

static PyObject *
PyLispObject_call(PyLispObject *obj, PyObject *args, PyObject *other)
{
    if (!PyArg_ParseTuple(args, ":call"))
        return NULL;

    return PyString_FromFormat("Calling: '%s'",
                               SDATA (Fprin1_to_string (obj->o, Qnil)));
}


static PyTypeObject _PyLispObjectType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "emacs.PyLispObject",             /*tp_name*/
    sizeof(PyLispObject),             /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0, /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    (reprfunc) PyLispObject_repr,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    (ternaryfunc) PyLispObject_call,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT, /*tp_flags*/
    "Python wrapper for an emacs Lisp_Object",
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    0,             /* tp_methods */
    0,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,      /* tp_init */
    0,                         /* tp_alloc */
    0,                 /* tp_new */
};


PyTypeObject *PyLispObjectType;

static PyObject *
wrap_PyLispObject(Lisp_Object o)
{
    PyLispObject *wrapped = PyObject_New(PyLispObject, (PyTypeObject *) PyLispObjectType);
    printf("wr %lx %lx\n", wrapped->ob_type, PyLispObjectType);
    wrapped->o = o;
    return wrapped;
}



PyObject *init_emacs_module(void)
{
    PyLispObjectType = PyMem_Malloc(sizeof(PyTypeObject));
    *PyLispObjectType = _PyLispObjectType;

    // Set up the Lisp_Object type wrapper
    PyLispObjectType->tp_new = PyType_GenericNew;

    printf("alloc %x %x\n", PyType_GenericAlloc, PyLispObjectType->tp_alloc);

    if (PyType_Ready(PyLispObjectType) < 0)
        Py_RETURN_NONE;
    printf("alloc %x %x\n", PyType_GenericAlloc, PyLispObjectType->tp_alloc);

    PyObject *mod = Py_InitModule3("emacs", emacs_funcs,
                                   "Emacs extensions functions.");

    Py_INCREF(PyLispObjectType);
    PyModule_AddObject(mod, "PyLispObject", (PyObject *) PyLispObjectType);
    printf("ealloc %x %x\n", PyType_GenericAlloc, PyLispObjectType->ob_refcnt);

    return mod;
}

void
init_python (void)
{
    // Why doesn't this work?
    //static char pySearchPath[] = "/usr/lib/python2.7/";
    //Py_SetPythonHome(pySearchPath);

    // We need to do this before emacs creates more threads, I believe.
    Py_Initialize();
    PyEval_InitThreads();
    PyEval_SaveThread();

    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();
    main_module = PyImport_AddModule("__main__");
    global_dict = PyModule_GetDict(main_module);
    PyDict_SetItemString (global_dict, "__builtins__", PyEval_GetBuiltins());

    PyObject *emacs_mod = init_emacs_module();
    PyDict_SetItemString(global_dict, "emacs", emacs_mod);
    PyCheckError("emacs module");
    emacs_dict = PyModule_GetDict(emacs_mod);

    PyGILState_Release(gstate);
}
