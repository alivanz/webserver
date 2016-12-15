#ifndef _DATATYPE
#define _DATATYPE

#include "datatype.h"

/* STRING */
static int string_init(string_t * self, PyObject * args, PyObject * kwargs){
  self->data = NULL;
  self->length = 0;
  self->max_length = 128;
  return 0;
}
static void string_del(string_t * self){
  if(self->data != NULL) free(self->data);
  self->ob_type->tp_free(self);
}
static PyObject * string_write(string_t * self, PyObject * buffer){
  /* Get String */
  if(!PyString_CheckExact(buffer)){
    PyErr_SetString(PyExc_KeyError,"buffer must be str");
    return NULL;
  }
  char * at = PyString_AsString(buffer);
  Py_ssize_t length = PyString_Size(buffer);
  /* Check limit */
  Py_ssize_t newlength = self->length + length;
  if(self->max_length && newlength > self->max_length){
    PyErr_SetString(PyExc_OverflowError,"max_length achieved.");
    return NULL;
  }
  /* Append buffer */
  char * new = realloc(self->data, newlength);
  if(new==NULL){
    PyErr_NoMemory();
    return NULL;
  }
  memcpy(new+self->length, at,length);
  self->data = new;
  self->length = newlength;
  /* finally */
  Py_RETURN_NONE;
}
static PyObject * string_get(string_t * self){
  return PyString_FromStringAndSize(self->data, self->length);
}
static PyMemberDef string_members[] = {
  {"max_length", T_PYSSIZET, offsetof(string_t, max_length), 0, "Maximum key length. Default 128. Set to 0 means no limit"},
  {NULL}
};
static PyMethodDef string_methods[] = {
  {"write", (PyCFunction)string_write, METH_O, "Write to buffer." },
  {"getvalue", (PyCFunction)string_get, METH_NOARGS, "Get current string value." },
  {NULL}
};
static PyTypeObject string_type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "datatype.string",             /*tp_name*/
    sizeof(string_t), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)string_del,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /*tp_flags*/
    "Alivanz string.\n",           /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    string_methods,             /* tp_methods */
    string_members,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)string_init,      /* tp_init */
    0,                         /* tp_alloc */
    PyType_GenericNew,                 /* tp_new */
};

/* INTEGER */
static int integer_init(integer_t * self, PyObject * args, PyObject * kwargs){
  self->data = 0;
  return 0;
}
static void integer_del(integer_t * self){
  self->ob_type->tp_free(self);
}
static PyObject * integer_write(integer_t * self, PyObject * buffer){
  /* Get String */
  if(!PyString_CheckExact(buffer)){
    PyErr_SetString(PyExc_KeyError,"buffer must be str");
    return NULL;
  }
  char * at = PyString_AsString(buffer);
  Py_ssize_t length = PyString_Size(buffer);
  /* Parse */
  Py_ssize_t i = 0;
  long ndata = self->data;
  while(i<length){
    if('0'<=at[i] && at[i]<='9'){
      long v = at[i]-'0';
      ndata *= 10;
      ndata += v;
    }else{
      PyErr_SetString(PyExc_ValueError, "invalid char");
      return NULL;
    }
    i++;
  }
  /* finally */
  self->data = ndata;
  Py_RETURN_NONE;
}
static PyMemberDef integer_members[] = {
  {"value", T_LONG, offsetof(integer_t, data), READONLY, "Integer data result."},
  {NULL}
};
static PyMethodDef integer_methods[] = {
  {"write", (PyCFunction)integer_write, METH_O, "Write to buffer." },
  {NULL}
};
static PyTypeObject integer_type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "datatype.integer",             /*tp_name*/
    sizeof(integer_t), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)integer_del,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /*tp_flags*/
    "Alivanz integer.\n",           /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    integer_methods,             /* tp_methods */
    integer_members,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)integer_init,      /* tp_init */
    0,                         /* tp_alloc */
    PyType_GenericNew,                 /* tp_new */
};

int datatype_prepare(PyObject* m){
  if (PyType_Ready(&string_type) < 0){
    printf("String not ready\n");
    return 1;
  }
  if (PyType_Ready(&integer_type) < 0){
    printf("Integer not ready\n");
    return 1;
  }

  Py_INCREF(&string_type);
  Py_INCREF(&integer_type);

  PyModule_AddObject(m, "string", (PyObject *)&string_type);
  PyModule_AddObject(m, "integer", (PyObject *)&integer_type);
  return 0;
}

#endif
