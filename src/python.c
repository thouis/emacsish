#include <config.h>
#include <Python.h>

#include "lisp.h"
#include "buffer.h"

static PyObject *main_module;
static PyObject *global_dict;

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

    PyObject *local_dict, *code, *obj, *result;

    local_dict  = PyDict_New();
    if (PyCheckError("creating new locals")) goto fail_local;

    code = Py_CompileString(SSDATA(expression), "", Py_eval_input);
    if (PyCheckError("compiling")) goto fail_compile;

    obj = PyEval_EvalCode(code, global_dict, local_dict);
    if (PyCheckError("evaluating expression")) goto fail_eval;

    result = PyObject_Str(obj);
    if (PyCheckError("converting to string")) goto fail_convert_string;

    char *str_result = PyString_AsString(result);
    if (PyCheckError("getting string")) goto fail_get_string;

    Py_DECREF(local_dict);
    Py_DECREF(code);
    Py_DECREF(obj);
    Py_DECREF(result);

    PyGILState_Release(gstate);
    printf("result %s\n", str_result);

    return build_unibyte_string(str_result);

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


static PyObject *point(PyObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "")) {
        return NULL;
    }
    return PyInt_FromLong(PT);
}

static PyObject *mark(PyObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "")) {
        return NULL;
    }
    return PyInt_FromLong(XMARKER(BVAR(current_buffer, mark))->charpos);
}

static PyObject *goto_char(PyObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "")) {
        return NULL;
    }
    return PyInt_FromLong(PT);
}

static PyObject *set_mark(PyObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, "")) {
        return NULL;
    }
    return PyInt_FromLong(XMARKER(BVAR(current_buffer, mark))->charpos);
}

static PyMethodDef emacs_funcs[] = {
    {"point",  point, METH_VARARGS, "Current emacs point."},
    {"mark",  mark, METH_VARARGS, "Current emacs mark."},
    {"goto_char",  goto_char, METH_VARARGS, "Go to (set point at) a particular position."},
    {"set_mark",  set_mark, METH_VARARGS, "Set mark to a particular position."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyObject *init_emacs_module(void)
{
    return Py_InitModule3("emacs", emacs_funcs,
                          "Emacs extensions functions.");
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

    PyGILState_Release(gstate);
}
