#include <Python.h>
#include "structmember.h"
#include "deps/multipart-parser-c/multipart_parser.c"
#include "py_http_multipart.h"
#include <sys/socket.h>

int on_header_field(multipart_parser* p, const char *at, size_t length){
  multipart * self = p->data;
  Py_XDECREF(self->last_field);
  self->last_field = Py_BuildValue("s#", at,length);
  return 0;
}
int on_header_value(multipart_parser* p, const char *at, size_t length){
  multipart * self = p->data;
  PyObject * value = Py_BuildValue("s#", at,length);
  PyDict_SetItem(self->header, self->last_field, value);
  Py_XDECREF(value);
  return 0;
}
int on_part_data(multipart_parser* p, const char *at, size_t length){
  multipart * self = p->data;
  PyObject * r = PyObject_CallMethod(self->data_object, "write", "s#", at,length);
  if(r!=Py_None) return 1;
  else return 0;
}
int on_part_data_begin(multipart_parser* p){
  return 0;
}
int on_headers_complete(multipart_parser* p){
  multipart * self = p->data;
  // callback
  PyObject * r = PyObject_CallMethod(self->data_object, "prepare", "O", self->header);
  if(r!=Py_None) return 1;
  else return 0;
}
int on_part_data_end(multipart_parser* p){
  multipart * self = p->data;
  // free header
  Py_DECREF(self->header);
  self->header = PyDict_New();
  // callback
  PyObject * r = PyObject_CallMethod(self->data_object, "finalize", NULL);
  if(r!=Py_None) return 1;
  else return 0;
}
int on_body_end(multipart_parser* p){
  multipart * self = p->data;
  PyObject_CallMethod(self->post_handler, "on_end", NULL);
  return 0;
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

static int multipart_init(multipart * self, PyObject * args, PyObject * kwargs){
  char * boundary;
  int boundary_length;
  PyObject * handler;
  PyArg_ParseTuple(args,"Os#", &handler, &boundary,&boundary_length);
  // parser
  self->parser = multipart_parser_init(boundary, &settings);
  self->parser->data = self;
  // last field
  self->last_field = NULL;
  // header
  self->header = PyDict_New();
  // boundary
  self->boundary = Py_BuildValue("s",boundary);
  // handler
  self->post_handler = handler;
  // data object
  self->data_object = NULL;
  // finished
  Py_INCREF(Py_False);
  self->finished = Py_False;
  // error
  Py_INCREF(Py_False);
  self->error = Py_False;
  return 0;
}
void multipart_del(multipart * self){
  free(self->parser);
  Py_XDECREF(self->last_field);
  Py_DECREF(self->header);
  Py_DECREF(self->boundary);
  Py_DECREF(self->post_handler);
  Py_XDECREF(self->data_object);
  Py_DECREF(self->finished);
  self->ob_type->tp_free((PyObject*)self);
}
static void c_multipart_execute(multipart * self, char * buffer, int length){
  int k = multipart_parser_execute(self->parser, buffer, length);
  if(k != length){
    Py_DECREF(self->error);
    Py_INCREF(Py_True);
    self->error = Py_True;
  }
  if(self->parser->state == s_end){
    Py_DECREF(self->finished);
    Py_INCREF(Py_True);
    self->finished = Py_True;
  }
}
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

static PyMemberDef multipart_members[] = {
    {"finished", T_OBJECT_EX, offsetof(multipart, finished), READONLY, "True if request is finished, False otherwise."},
    {"erorr", T_OBJECT_EX, offsetof(multipart, error), READONLY, "True if error occurred, False otherwise."},
    {NULL}
};
static PyMethodDef multipart_methods[] = {
    {"fetch_socket", (PyCFunction)multipart_fetch_socket, METH_O, "Fetching data, return parser_data object." },
    {"send", (PyCFunction)multipart_send, METH_O, "Check if parser_data object is error." },
    {NULL}
};

static PyTypeObject multipart_type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "http_parser.multipart",             /*tp_name*/
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

void inithttp_parser(){
  if (PyType_Ready(&multipart_type) < 0){
    printf("Type not ready\n");
    return;
  }
  PyObject* m = Py_InitModule3("http_parser", methods,"HTTP/1.x POST parser modules.");
  if (m == NULL)
    return;
  Py_INCREF(&multipart_type);
  PyModule_AddObject(m, "multipart", (PyObject *)&multipart_type);
}
