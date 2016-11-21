#include <Python.h>
#include "structmember.h"
#include "deps/http-header/http_parser.c"
#include <sys/socket.h>
#include "request.h"

/* Request Python __init__ method */
/* prepare initial object, setup parser struct. */
static int request_init(request_t * self, PyObject * args, PyObject * kwargs){
  http_parser_init(&self->parser, HTTP_REQUEST);
  self->parser.data = self;

  self->method = NULL;
  self->URI = NULL;
  self->Query = NULL;
  self->version = NULL;

  self->field = NULL;
  self->header = PyDict_New();

  Py_INCREF(Py_None);
  self->writeback = Py_None;
  Py_INCREF(Py_None);
  self->handler = Py_None;

  self->completed = 0;
  self->error = 0;

  return 0;
}

/* Request Python __del__ method */
void request_del(request_t * self){
  Py_XDECREF(self->method);
  Py_XDECREF(self->URI);
  Py_XDECREF(self->Query);
  Py_XDECREF(self->version);
  Py_XDECREF(self->field);
  Py_DECREF(self->header);
  self->ob_type->tp_free((PyObject*)self);
}

/* Parser execution */
static int on_url(http_parser* p, const char *at, size_t length){
  request_t * self = p->data;
  // method
  self->method = Py_BuildValue("s", http_method_str(p->method) );
  // URI
  self->URI = Py_BuildValue("s#",at,length);
  // query
  size_t i = 0;
  while(i<length && at[i]!='?') i += 1;
  if(i<length){
    i += 1;
    self->Query = Py_BuildValue("s#",at+i,length-i);
  }else{
    self->Query = Py_BuildValue("s","");
  }

  return 0;

  /*PyObject * result = PyObject_CallMethod((PyObject*)self,"on_url","s#", at,length);
  if(result==NULL) return -1;
  Py_DECREF(result);
  return 0;*/
}
static int on_header_field(http_parser* p, const char *at, size_t length){
  request_t * self = p->data;
  self->field = Py_BuildValue("s#",at,length);
  return 0;
  /*PyObject * result = PyObject_CallMethod((PyObject*)self,"on_header_field","s#", at,length);
  if(result==NULL) return -1;
  Py_DECREF(result);
  return 0;*/
}
static int on_header_value(http_parser* p, const char *at, size_t length) {
  request_t * self = p->data;
  PyObject * value = Py_BuildValue("s#",at,length);
  PyDict_SetItem(self->header, self->field, value);
  Py_DECREF(self->field);
  self->field = NULL;
  Py_DECREF(value);
  return 0;
  /*PyObject * result = PyObject_CallMethod((PyObject*)self,"on_header_value","s#", at,length);
  if(result==NULL) return -1;
  Py_DECREF(result);
  return 0;*/
}
static int on_headers_complete(http_parser* p){
  request_t * self = p->data;
  /*PyObject * result = PyObject_CallMethod((PyObject*)self,"on_headers_complete","");
  if(result==NULL) return -1;
  Py_DECREF(result);
  return 0;*/

  // version
  //char tmp[16];
  //sprintf(tmp,"HTTP/%i.%i", p->http_major, p->http_minor);
  //self->version = Py_BuildValue("s", tmp);

  // get handler
  // Add history here, for debugging (NOT IMPLEMENTED YET)
  PyObject * handler = PyObject_CallMethod((PyObject*)self,"routing","");
  if(handler==NULL){
    PyErr_SetString(exception,"request.routing failed.");
    return -1;
  }else if(handler==Py_None){
    self->error = 404;
    Py_DECREF(handler);
    return -1;
  }
  Py_DECREF(self->handler);
  self->handler = handler;

  // set_writeback
  PyObject * result = PyObject_CallMethod((PyObject*)handler,"set_writeback","O",self->writeback);
  if(result==NULL){
    PyErr_SetString(exception,"(request->routing).set_writeback failed.");
    return -1;
  }

  return 0;
}
static int on_body(http_parser* p, const char *at, size_t length) {
  request_t * self = p->data;
  /*PyObject * result = PyObject_CallMethod((PyObject*)self,"on_body","s#", at,length);
  if(result==NULL) return -1;
  Py_DECREF(result);
  return 0;*/

  PyObject * handler = self->handler;
  // get handler
  PyObject * result = PyObject_CallMethod((PyObject*)handler,"write","s#", at,length);
  if(result==NULL){
    PyErr_SetString(exception,"(request->routing).write failed.");
    return -1;
  }else if(result==Py_None){
    Py_DECREF(result);
    return 0;
  }else{
    PyErr_SetString(exception,"(request->routing).write return something (not None)..");
    Py_DECREF(result);
    return -1;
  }
}
static int on_message_complete(http_parser* p) {
  request_t * self = p->data;
  /*PyObject * result = PyObject_CallMethod((PyObject*)self,"on_message_complete","");
  if(result==NULL) return -1;
  Py_DECREF(result);
  return 0;*/

  self->completed = 1;

  PyObject * result = PyObject_CallMethod((PyObject*)self->handler,"write_end","");
  if(result==NULL){
    PyErr_SetString(exception,"(request->routing).write_end failed.");
    return -1;
  }else if(result!=Py_None){
    PyErr_SetString(exception,"(request->routing).write_end return something (non-None).");
    Py_DECREF(result);
    return -1;
  }
  return 0;
}

static PyObject * request_write(request_t * self, PyObject * buffer){
  if(!PyString_CheckExact(buffer)){
    PyErr_SetString(exception,"Invalid data type, string expected.");
    return NULL;
  }
  char * at;
  long int length;
  PyString_AsStringAndSize(buffer, &at, &length);

  struct http_parser * p = &self->parser;
  long int parsed = http_parser_execute(p, &settings, at, length);

  // Parse Error
  if( self->parser.http_errno >= HPE_INVALID_EOF_STATE ){
    // call error handler
    PyObject * tmp = PyObject_CallMethod((PyObject*)self,"on_error","i", 400);
    if(tmp==NULL){
      PyErr_SetString(exception,"Parse error and request.on_error call failed");
      return NULL;
    }
    Py_DECREF(tmp);
    Py_RETURN_FALSE;
  }

  // Process error
  if( self->parser.http_errno > HPE_OK || parsed!=length ){
    PyObject * err = PyErr_Occurred();
    if(err==NULL){ // No exception
      PyObject * tmp;
      if(self->error==0) tmp = PyObject_CallMethod((PyObject*)self,"on_error","i", 500);
      else tmp = PyObject_CallMethod((PyObject*)self,"on_error","i", self->error);
      if(tmp==NULL){
        PyErr_SetString(exception,"Parse error and request.on_error call failed");
        return NULL;
      }
      Py_DECREF(tmp);
    }else{ // exception detected
      // BUG : Exception detected, cant call on_error
      // BUG fix
      PyErr_Print();
      PyErr_Clear();
      // BUG
      if(PyObject_HasAttrString((PyObject*)self,"on_error")){
        PyObject * tmp = PyObject_CallMethod((PyObject*)self,"on_error","i", 500);
        if(tmp!=NULL){
          Py_DECREF(tmp);
        }
      }
    }
    Py_RETURN_FALSE;
  }

  if(self->completed) Py_RETURN_NONE;

  Py_RETURN_TRUE;
}
static PyObject * set_writeback(request_t * self, PyObject * writeback){
  Py_DECREF(self->writeback);
  Py_INCREF(writeback);
  self->writeback = writeback;
  Py_RETURN_NONE;
}

static PyMemberDef request_members[] = {
  // static
  {"method", T_OBJECT, offsetof(request_t, method), READONLY, "HTTP Method"},
  {"URI", T_OBJECT, offsetof(request_t, URI), READONLY, "Request URI"},
  {"Query", T_OBJECT, offsetof(request_t, Query), READONLY, "Request Query. Used in GET variables."},
  {"version", T_OBJECT, offsetof(request_t, version), READONLY, "HTTP Version."},
  {"header", T_OBJECT_EX, offsetof(request_t, header), READONLY, "HTTP Header"},

  // non-static
  {"respond", T_OBJECT, offsetof(request_t, writeback), READONLY, "Writeback method."},
  {NULL}
};
static PyMethodDef request_methods[] = {
  {"write", (PyCFunction)request_write, METH_O, "Write request stream." },
  {"set_writeback", (PyCFunction)set_writeback, METH_O, "Set writeback." },
  //{"is_error", (PyCFunction)request_is_error, METH_NOARGS, "Check if request_t object is error." }
  {NULL}
};
static PyTypeObject request_type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "request.request",             /*tp_name*/
    sizeof(request_t), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)request_del,                         /*tp_dealloc*/
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
    request_methods,             /* tp_methods */
    request_members,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)request_init,      /* tp_init */
    0,                         /* tp_alloc */
    PyType_GenericNew,                 /* tp_new */
};

// HTTP ERROR NAME MAP
PyObject * status_name;

static PyMethodDef methods[] = {
  {NULL}
};

int prepare(PyObject* m){
  exception = PyErr_NewException("request.error", NULL, NULL);
  Py_INCREF(exception);

  if (PyType_Ready(&request_type) < 0){
    printf("Type not ready\n");
    return 1;
  }
  Py_INCREF(&request_type);

  status_name = PyDict_New();
  PyObject * key;
  PyObject * value;
  #define XX(num, name, string) \
    key = Py_BuildValue("i",num); \
    value = Py_BuildValue("s",#string); \
    PyDict_SetItem(status_name,key,value); \
    Py_DECREF(key); \
    Py_DECREF(value);
    HTTP_STATUS_MAP(XX)
  #undef XX

  PyModule_AddObject(m, "error", exception);
  PyModule_AddObject(m, "request", (PyObject *)&request_type);
  PyModule_AddObject(m, "status_name", status_name);
  return 0;
}

void initrequest(void){
  PyObject* m = Py_InitModule3("request", methods,"HTTP/1.x request parser modules.");
  if (m == NULL)
    return;
  prepare(m);
}
