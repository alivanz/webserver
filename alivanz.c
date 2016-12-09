#include "request.c"
#include "storage.c"

static PyMethodDef alivanz_module[] = {
  {NULL}
};

void initalivanz(void){
  PyObject* m = Py_InitModule3("alivanz", alivanz_module,"Alivanz Webserver modules.");
  if (m == NULL)
    return;

  constant_init(m);
  request_prepare(m);
  storage_prepare(m);
}
