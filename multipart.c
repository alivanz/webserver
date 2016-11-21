#include <Python.h>
#include "structmember.h"
#include "deps/multipart-parser-c/multipart_parser.c"
#include "multipart.h"
#include <sys/socket.h>

static int multipart_init(multipart * self, PyObject * args, PyObject * kwargs){
  char * boundary;
  int boundary_length;
  if(!PyArg_ParseTuple(args,"s#", &boundary,&boundary_length)){
    return 1;
  }

  // parser
  self->parser = multipart_parser_init(boundary, &settings);
  self->parser->data = self;
  self->boundary = Py_BuildValue("s#",boundary,boundary_length);

  // header
  self->last_field = NULL;
  self->header = PyDict_New();

  // var_handler
  self->var_handler = NULL;

  // error
  Py_INCREF(Py_None);
  self->error = Py_None;
  return 0;
}
void multipart_del(multipart * self){
  // parser
  free(self->parser);
  Py_DECREF(self->boundary);

  // header
  Py_XDECREF(self->last_field);
  Py_DECREF(self->header);

  Py_XDECREF(self->var_handler);

  Py_DECREF(self->error);

  self->ob_type->tp_free((PyObject*)self);
}

/* ---------- EVENT ---------- */

int on_part_data_begin(multipart_parser* p){
  printf("on_part_data_begin\n");
  return 0;
}
/* Header */
int on_header_field(multipart_parser* p, const char *at, size_t length){
  // type casting
  multipart * self = p->data;

  // set last field
  self->last_field = Py_BuildValue("s#", at,length);

  return 0;
}
int on_header_value(multipart_parser* p, const char *at, size_t length){
  // type casting
  multipart * self = p->data;

  // set value
  PyObject * value = Py_BuildValue("s#", at,length);
  PyDict_SetItem(self->header, self->last_field, value);

  // free memory
  Py_DECREF(self->last_field);
  Py_DECREF(value);
  self->last_field = NULL;

  return 0;
}
int on_headers_complete(multipart_parser* p){
  printf("on_headers_complete\n");
  // type casting
  multipart * self = p->data;

  // callback
  if(!PyObject_HasAttrString((PyObject*)self,"get_var_handler")){
    printf("Multipart get_var_handler method undefined\n");
    return 1;
  }
  PyObject * var_handler = PyObject_CallMethod((PyObject*)self, "get_var_handler", "O", self->header);
  if(var_handler == Py_None){
    Py_DECREF(var_handler);
    return 1;
  }

  // set var handler
  Py_XDECREF(self->var_handler);
  self->var_handler = var_handler;

  return 0;
}

/* Data */
int on_part_data(multipart_parser* p, const char *at, size_t length){
  printf("on_part_data\n");
  // type casting
  multipart * self = p->data;

  // write var handler
  if(!PyObject_HasAttrString((PyObject*)self->var_handler,"write"))
    return 1;
  PyObject * callback_return = PyObject_CallMethod((PyObject*)self->var_handler, "write", "s#", at,length);
  if(callback_return != Py_None){
    Py_DECREF(callback_return);
    return 2;
  }
  Py_DECREF(callback_return);

  return 0;
}
int on_part_data_end(multipart_parser* p){
  printf("on_part_data_end\n");
  // type casting
  multipart * self = p->data;

  // reset header
  PyObject * new = PyDict_New();
  Py_DECREF(self->header);
  self->header = new;

  // finalize var handler
  if(PyObject_HasAttrString((PyObject*)self->var_handler,"write_end")){
    PyObject * callback_return = PyObject_CallMethod((PyObject*)self->var_handler, "write_end", "");
    if(callback_return != Py_None){
      Py_DECREF(callback_return);
      return 2;
    }
    Py_DECREF(callback_return);
  }

  return 0;
}

/* End of data transfer */
static int call_write_end(multipart * self){
  if(!PyObject_HasAttrString((PyObject*)self,"write_end"))
    return 1;
  PyObject * callback_return = PyObject_CallMethod((PyObject*)self, "write_end", "");
  if(callback_return != Py_None){
    Py_DECREF(callback_return);
    return 2;
  }
  Py_DECREF(callback_return);
  return 0;
}
int on_body_end(multipart_parser* p){
  // type casting
  multipart * self = p->data;
  // write_end
  return call_write_end(self);
}

multipart_parser_settings settings = {
  .on_header_field = on_header_field,
  .on_header_value = on_header_value,
  .on_part_data = on_part_data,

  .on_part_data_begin = on_part_data_begin,
  .on_headers_complete = on_headers_complete,
  .on_part_data_end = on_part_data_end,
  .on_body_end = on_body_end
};

PyObject * multipart_set_writeback(multipart * self, PyObject * writeback){
  Py_XDECREF(self->writeback);
  self->writeback = writeback;
  Py_RETURN_NONE;
}

static int C_multipart_write(multipart * self, char * buffer, int length){
  // execute
  int k = multipart_parser_execute(self->parser, buffer, length);

  // detect error
  if(k != length){
    return 1;
  }

  return 0;
}
PyObject * multipart_write(multipart * self, PyObject * args){
  // read buffer
  char * at;
  int length;
  PyArg_ParseTuple(args,"s#", &at, &length);

  // execute
  if(C_multipart_write(self, at,length)){
    if(self->error == Py_None){ // stop
      printf("Multipart stop\n");
      PyObject * callback_return = PyObject_CallMethod((PyObject*)self, "write_end", "");
      Py_DECREF(callback_return);
      Py_RETURN_NONE;
    }
    else Py_RETURN_FALSE; // error
  }

  Py_RETURN_TRUE; // continue
}
/* OLD Method */
/*
PyObject * multipart_fetch_socket(multipart * self, PyObject * args){
  if(self->error) Py_RETURN_NONE;
  if(self->finished) Py_RETURN_NONE;
  // if not finished
  int fd;
  PyObject * handler;
  PyArg_ParseTuple(args, "Oi", &handler, &fd);
  self->post_handler = handler;
  // start
  char buffer[MAX_BUFFER];
  int len;
  len = recv(fd,buffer,MAX_BUFFER,0);
  while(len>0){
    c_multipart_execute(self, buffer,len);
    if(self->finished == Py_True) Py_RETURN_NONE;
    len = recv(fd,buffer,MAX_BUFFER,0);
  }
  Py_RETURN_NONE;
}
PyObject * multipart_send(multipart * self, PyObject * py_buffer){
  if(self->error) Py_RETURN_NONE;
  if(self->finished) Py_RETURN_NONE;
  // if not finished
  char * buffer;
  long int len;
  PyString_AsStringAndSize(py_buffer, &buffer, &len);
  c_multipart_execute(self,buffer,len);
  Py_RETURN_NONE;
}
*/

PyObject * multipart_get_error(multipart * self){

  Py_RETURN_NONE;
}

static PyMemberDef multipart_members[] = {
  {"respond", T_OBJECT_EX, offsetof(multipart, writeback), READONLY, "Writeback."},
  //{"finished", T_OBJECT_EX, offsetof(multipart, finished), READONLY, "True if request is finished, False otherwise."},
  {"error", T_OBJECT_EX, offsetof(multipart, error), READONLY, "Error object, can be None."},
  {NULL}
};
static PyMethodDef multipart_methods[] = {
  {"set_writeback", (PyCFunction)multipart_set_writeback, METH_O, "Set writeback." },
  {"write", (PyCFunction)multipart_write, METH_VARARGS, "Write data." },
  {"get_error", (PyCFunction)multipart_get_error, METH_NOARGS, "Get error." },
  /* OLD
  {"fetch_socket", (PyCFunction)multipart_fetch_socket, METH_O, "Fetching data, return parser_data object." }, // DEPRECATED
  {"send", (PyCFunction)multipart_send, METH_O, "Check if parser_data object is error." }, // DEPRECATED
  */
  {NULL}
};

static PyTypeObject multipart_type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "multipart.multipart",             /*tp_name*/
    sizeof(multipart), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)multipart_del,                         /*tp_dealloc*/
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
    "HTTP POST multipart setup",           /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    multipart_methods,             /* tp_methods */
    multipart_members,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)multipart_init,      /* tp_init */
    0,                         /* tp_alloc */
    PyType_GenericNew,                 /* tp_new */
};

static PyMethodDef methods[] = {
  {NULL}
};

int prepare(PyObject* m){
  // prepare type
  if (PyType_Ready(&multipart_type) < 0){
    printf("Type not ready\n");
    return 2;
  }
  Py_INCREF(&multipart_type);

  // add
  PyModule_AddObject(m, "multipart", (PyObject *)&multipart_type);

  return 0;
}

void initmultipart(void){
  PyObject* m = Py_InitModule3("multipart", methods,"HTTP/1.x POST multipart parser modules.");
  if (m == NULL)
    return;
  prepare(m);
}
