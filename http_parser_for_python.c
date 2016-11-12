#include <Python.h>
#include "structmember.h"
#include "deps/http-header/http_parser.c"
#include <sys/socket.h>

typedef struct{
  PyObject_HEAD
  int fd;
  struct http_parser parser;
  PyObject * method;
  PyObject * URI;
  PyObject * Query;
  PyObject * header;
  char * last_field;
  size_t last_field_length;
  int header_complete;
  int message_complete;
  int error;
} parser_data;
static int parser_data_init(parser_data * self, PyObject * args, PyObject * kwargs){
  http_parser_init(&self->parser, HTTP_REQUEST);
  PyArg_ParseTuple(args,"i",&self->fd);
  // method
  Py_INCREF(Py_None);
  self->method = Py_None;
  // url
  Py_INCREF(Py_None);
  self->URI = Py_None;
  // Query
  Py_INCREF(Py_None);
  self->Query = Py_None;
  // header
  self->header = PyDict_New();
  // misc
  self->last_field = NULL;
  self->last_field_length = 0;
  self->message_complete = 0;
  self->header_complete = 0;
  self->error = 0;
  return 0;
}

void parser_data_del(parser_data * self){
  Py_DECREF(self->method);
  Py_DECREF(self->URI);
  Py_DECREF(self->Query);
  Py_DECREF(self->header);
  free(self->last_field);
  self->ob_type->tp_free((PyObject*)self);
}
static int on_url(http_parser* p, const char *at, size_t length){
  parser_data * self = p->data; // type casting
  // method
  Py_DECREF(self->method);
  self->method = Py_BuildValue("s", http_method_str(p->method) );
  // URI
  Py_DECREF(self->URI);
  self->URI = Py_BuildValue("s#",at,length);
  // query
  size_t i = 0;
  while(i<length && at[i]!='?') i += 1;
  if(i<length){
    i += 1;
    Py_DECREF(self->Query);
    self->Query = Py_BuildValue("s#",at+i,length-i);
  }else{
    self->Query = Py_BuildValue("s","");
  }
  return 0;
}
static int on_header_field(http_parser* p, const char *at, size_t length) {
  parser_data * self = p->data; // type casting
  if(self->last_field != NULL) free(self->last_field);
  self->last_field = malloc(sizeof(char)*length);
  memcpy(self->last_field, at, sizeof(char)*length);
  self->last_field_length = length;
  return 0;
}
static int on_header_value(http_parser* p, const char *at, size_t length) {
  parser_data * data = p->data; // type casting
  PyObject * key = Py_BuildValue("s#", data->last_field, data->last_field_length);
  PyObject * value = Py_BuildValue("s#",at,length);
  PyDict_SetItem(data->header, key, value);
  Py_DECREF(key);
  Py_DECREF(value);
  return 0;
}
static int on_headers_complete(http_parser* p) {
  parser_data * data = p->data; // type casting
  data->header_complete = 1;
  return 0;
}
static int on_message_complete(http_parser* p) {
  parser_data * data = p->data; // type casting
  data->message_complete = 1;
  return 0;
}
static int on_info(http_parser* p) {
  return 0;
}
static int on_data(http_parser* p, const char *at, size_t length) {
  return 0;
}

static http_parser_settings settings = {
  .on_message_begin = on_info,
  .on_headers_complete = on_headers_complete,
  .on_message_complete = on_message_complete,
  .on_header_field = on_header_field,
  .on_header_value = on_header_value,
  .on_url = on_url,
  .on_status = on_data,
  .on_body = on_data
};

#define MAX_BUFFER 4096
static PyObject * fetch_data(parser_data * self){
  // specification : receive socket fd
  int fd = self->fd;
  int buffer_length;
  int parsed;
  char buffer[MAX_BUFFER];
  // prepare
  self->parser.data = self;
  // start
  while(1){
    buffer_length = recv(fd,buffer,MAX_BUFFER,0);
    parsed = http_parser_execute(&self->parser, &settings, buffer, buffer_length);
    if(!buffer_length) break;
    if(parsed != buffer_length){
      self->error = 1;
      break;
    };
    if(self->header_complete) break;
    if(self->message_complete) break;
  }
  Py_RETURN_NONE;
}

static PyObject * is_error(parser_data * self){
  if(self->error) Py_RETURN_TRUE;
  else Py_RETURN_FALSE;
}

static PyMemberDef parser_data_members[] = {
    {"method", T_OBJECT_EX, offsetof(parser_data, method), READONLY, "HTTP Method"},
    {"URI", T_OBJECT_EX, offsetof(parser_data, URI), READONLY, "Request URI"},
    {"Query", T_OBJECT_EX, offsetof(parser_data, Query), READONLY, "Request Query. Used in GET variables."},
    {"header", T_OBJECT_EX, offsetof(parser_data, header), READONLY, "HTTP Header"},
    {NULL}
};
static PyMethodDef parser_data_methods[] = {
    {"fetch_data", (PyCFunction)fetch_data, METH_NOARGS, "Fetching data, return parser_data object." },
    {"is_error", (PyCFunction)is_error, METH_NOARGS, "Check if parser_data object is error." },
    {NULL}
};
static PyTypeObject parser_data_type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "http_parser.parser",             /*tp_name*/
    sizeof(parser_data), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)parser_data_del,                         /*tp_dealloc*/
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
    "HTTP setup",           /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    parser_data_methods,             /* tp_methods */
    parser_data_members,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)parser_data_init,      /* tp_init */
    0,                         /* tp_alloc */
    PyType_GenericNew,                 /* tp_new */
};

static PyMethodDef methods[] = {
  {NULL}
};

void inithttp_parser(){
  if (PyType_Ready(&parser_data_type) < 0){
    printf("Type not ready\n");
    return;
  }
  PyObject* m = Py_InitModule3("http_parser", methods,"HTTP/1.x parser modules.");
  if (m == NULL)
    return;
  Py_INCREF(&parser_data_type);
  PyModule_AddObject(m, "parser", (PyObject *)&parser_data_type);
}
