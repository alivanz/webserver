#include "request.c"
#include "storage.c"
#include "datatype.c"

static PyMethodDef alivanz_module[] = {
  {NULL}
};
static PyMethodDef error_module[] = {
  {NULL}
};
static PyMethodDef datatype_module[] = {
  {NULL}
};

void initalivanz(void){
  PyObject* m = Py_InitModule3("alivanz", alivanz_module,"Alivanz Webserver modules.");
  if (m == NULL)
    return;
  PyObject* e = Py_InitModule3("alivanz.error", error_module,"Alivanz Webserver error modules.");
  if (e == NULL)
    return;
  PyObject* d = Py_InitModule3("alivanz.datatype", error_module,"Alivanz Webserver datatype modules.");
  if (d == NULL)
    return;
  request_prepare(m);
  storage_prepare(m);
  constant_init(e);
  PyModule_AddObject(m, "error", e);
  datatype_prepare(d);
  PyModule_AddObject(m, "datatype", d);
}
