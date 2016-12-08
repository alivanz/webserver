#ifndef _CONSTANT
#define _CONSTANT

#include <Python.h>

static PyObject * http_error;
static PyObject * client_error;
static PyObject * server_error;
static PyObject * parse_error;
static int constant_initialized = 0;

static void constant_init(PyObject * m){
  if(constant_initialized) return;
  constant_initialized = 1;
  http_error = PyErr_NewExceptionWithDoc(
    "alivanz.error",
    "base http_error",
    NULL,NULL /* base,dict */
  );
  client_error = PyErr_NewExceptionWithDoc(
    "alivanz.client_error",
    "Client error",
    http_error,NULL /* base,dict */
  );
  server_error = PyErr_NewExceptionWithDoc(
    "alivanz.server_error",
    "Server error",
    http_error,NULL /* base,dict */
  );
  parse_error = PyErr_NewExceptionWithDoc(
    "alivanz.parse_error",
    "Parse error",
    client_error,NULL /* base,dict */
  );
  Py_INCREF(http_error);
  Py_INCREF(client_error);
  Py_INCREF(server_error);
  Py_INCREF(parse_error);
  PyModule_AddObject(m, "error", http_error);
  PyModule_AddObject(m, "client_error", client_error);
  PyModule_AddObject(m, "server_error", server_error);
  PyModule_AddObject(m, "parse_error", parse_error);
}
#endif
