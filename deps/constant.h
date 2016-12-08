#ifndef _CONSTANT
#define _CONSTANT

#include <Python.h>

static PyObject * http_error;
static int constant_initialized = 0;

static void constant_init(){
  if(constant_initialized) return;
  constant_initialized = 1;
  http_error = PyErr_NewExceptionWithDoc(
    "http_error",
    "base http_error",
    NULL,NULL /* base,dict */
  );
}
#endif
