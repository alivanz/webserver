#ifndef _REQUEST
#define _REQUEST

#include <Python.h>
#include "structmember.h"
#include "deps/http-header/http_parser.c"
#include "deps/constant.h"
#include <sys/socket.h>
#include "request.h"

/* TRACEBACK */
//#include <frameobject.h>
//#include <traceback.h>

/* Request Python __init__ method */
/* prepare initial object, setup parser struct. */
static int request_init(request_t * self, PyObject * args, PyObject * kwargs){
  http_parser_init(&self->parser, HTTP_REQUEST);
  self->parser.data = self;

  self->method = NULL;
  self->URI = NULL;
  self->URL = NULL;
  self->Query = NULL;
  self->version = NULL;

  self->field = NULL;
  self->header = PyDict_New();

  self->writeback = NULL;
  self->handler = NULL;
  self->client = NULL;

  self->completed = 0;
  self->error = 0;

  return 0;
}

/* Request Python __del__ method */
void request_del(request_t * self){
  Py_XDECREF(self->method);
  Py_XDECREF(self->URI);
  Py_XDECREF(self->URL);
  Py_XDECREF(self->Query);
  Py_XDECREF(self->version);

  Py_XDECREF(self->field);
  Py_DECREF(self->header);

  // Add opt free (bug fix)
  Py_XDECREF(self->writeback);
  Py_XDECREF(self->handler);
  Py_XDECREF(self->client);

  self->ob_type->tp_free((PyObject*)self);
}

/* Call */
/*
static inline PyObject * call_routing(request_t * self){
  Py_EnterRecursiveCall("Call request.routing()");
  PyObject * result = PyObject_CallMethod((PyObject*)self,"routing","");
  Py_LeaveRecursiveCall();
  return result;
}
static inline PyObject * call_routing_set_writeback(request_t * self){
  PyObject * handler = self->handler;
  Py_EnterRecursiveCall("Call (request->routing).set_writeback( writeback )");
  PyObject * result = PyObject_CallMethod((PyObject*)handler,"set_writeback","O",self->writeback);
  Py_LeaveRecursiveCall();
  return result;
}
static inline PyObject * call_routing_write(request_t * self, char * at, int length){
  PyObject * handler = self->handler;
  Py_EnterRecursiveCall("Call (request->routing).write( buffer )");
  PyObject * result = PyObject_CallMethod((PyObject*)handler,"write","s#", at,length);
  Py_LeaveRecursiveCall();
  return result;
}
static inline PyObject * call_routing_write_end(request_t * self){
  Py_EnterRecursiveCall("Call (request->routing).write_end");
  PyObject * result = PyObject_CallMethod((PyObject*)self->handler,"write_end","");
  Py_LeaveRecursiveCall();
  return result;
}
static inline void call_request_on_error(request_t * self, int code, const char * msg){
  // store error
  PyErr_Clear();
  // call
  Py_EnterRecursiveCall("Call request.on_error( code, msg )");
  PyObject * result = PyObject_CallMethod((PyObject*)self,"on_error","is", code, msg);
  Py_LeaveRecursiveCall();
  // Dump
  if(result==NULL){
    printf("Fatal Error, Unable to dump error.\n");
  }else{
    //printf("JUST_TERMINATE\n");
    Py_DECREF(result);
    self->error = JUST_TERMINATE;
  }
}
*/

/* Parser execution */
static int request_on_url(http_parser* p, const char *at, size_t length){
  request_t * self = p->data;
  /* Method and URI */
  self->method  = PyString_FromString( http_method_str(p->method) );
  self->URI     = PyString_FromStringAndSize(at,length);
  /* URL & query */
  size_t i = 0;
  while(i<length && at[i]!='?') i += 1;
  if(i<length){
    i += 1;
    self->Query = PyString_FromStringAndSize(at+i,length-i);
  }else{
    self->Query = PyString_FromStringAndSize(NULL,0);
  }
  self->URL     = PyString_FromStringAndSize(at,i);
  return 0;
}
static int request_on_header_field(http_parser* p, const char *at, size_t length){
  /* Store field */
  request_t * self  = p->data;
  self->field       = PyString_FromStringAndSize(at,length);
  return 0;
}
static int request_on_header_value(http_parser* p, const char *at, size_t length) {
  /* Add to header */
  request_t * self = p->data;
  PyObject * value = PyString_FromStringAndSize(at,length);
  PyDict_SetItem(self->header, self->field, value);
  /* Free */
  Py_DECREF(self->field);
  self->field = NULL;
  Py_DECREF(value);
  return 0;
}
static int request_on_headers_complete(http_parser* p){
  request_t * self = p->data;
  /* Call get_handler */
  PyObject * result = PyObject_CallMethod((PyObject*)self, "get_handler", "");
  if(result==NULL){
    /* Exception thrown */
    return -1;
  }else if(result==Py_None){
    /* No Handler */
    Py_DECREF(result);
    PyErr_SetString(PyExc_TypeError,"get_handler return None");
    return -1;
  }else{
    Py_XDECREF(self->handler);
    self->handler = result;
  }
  /* Set handler writeback */
  result = PyObject_CallMethod(self->handler, "set_writeback", "O", self->writeback);
  if(result==NULL){
    /* Exception thrown */
    return -1;
  }
  Py_DECREF(result);
  return 0;
}
static int request_on_body(http_parser* p, const char *at, size_t length) {
  request_t * self = p->data;
  /* Write Handler */
  PyObject * result = PyObject_CallMethod(self->handler, "write", "s#", at,length);
  if(result == NULL){
    /* Exception thrown */
    return -1;
  }
  Py_DECREF(result);
  return 0;
}
static int request_on_message_complete(http_parser* p) {
  request_t * self = p->data;
  /* Close handler */
  PyObject * result = PyObject_CallMethod(self->handler, "close", "");
  if(result == NULL){
    /* Exception thrown */
    return -1;
  }
  Py_DECREF(result);
  return 0;
}

/* interface */
static PyObject * request_write(request_t * self, PyObject * buffer){
  /* Get string */
  if(!PyString_CheckExact(buffer)){
    PyErr_SetString(PyExc_TypeError,"Invalid data type, string expected.");
    return NULL;
  }
  char * at;
  long int length;
  PyString_AsStringAndSize(buffer, &at, &length);
  /* Parse */
  struct http_parser * p = &self->parser;
  long int parsed = http_parser_execute(p, &settings, at, length);
  /* Check Error */
  if(PyErr_Occurred()!=NULL) return NULL;
  /* Parse Error */
  if(p->http_errno >= HPE_INVALID_EOF_STATE || parsed!=length){
    /* Call bad request */
    PyObject * result = PyObject_CallMethod((PyObject*)self, "on_bad_request", "");
    if(result==NULL) return NULL;
    else Py_DECREF(result);
  }
  /* Callback Error */
  if(p->http_errno > HPE_OK){
    /* Bad programming */
    PyErr_SetString(PyExc_SystemError,"Callback error, but no exception is thrown. (considered bad programming in C)");
    return NULL;
  }
  /* Finally */
  Py_RETURN_NONE;
}

/* OPTIONS */
static PyObject * set_writeback(request_t * self, PyObject * writeback){
  Py_XDECREF(self->writeback);
  Py_INCREF(writeback);
  self->writeback = writeback;
  Py_RETURN_NONE;
}
static PyObject * set_client(request_t * self, PyObject * client){
  if(!PyString_CheckExact(client)){
    PyErr_SetString(PyExc_TypeError,"Invalid data type, string expected ('host:port' or 'host').");
    return NULL;
  }
  Py_XDECREF(self->client);
  Py_INCREF(client);
  self->client = client;
  Py_RETURN_NONE;
}

static PyMemberDef request_members[] = {
  // static
  {"method", T_OBJECT, offsetof(request_t, method), READONLY, "HTTP Method"},
  {"URI", T_OBJECT, offsetof(request_t, URI), READONLY, "Request URI"},
  {"URL", T_OBJECT, offsetof(request_t, URL), READONLY, "Request URL"},
  {"Query", T_OBJECT, offsetof(request_t, Query), READONLY, "Request Query. Used in GET variables."},
  {"version", T_OBJECT, offsetof(request_t, version), READONLY, "HTTP Version."},
  {"header", T_OBJECT_EX, offsetof(request_t, header), READONLY, "HTTP Header"},

  // non-static
  {"respond", T_OBJECT, offsetof(request_t, writeback), READONLY, "Writeback method."},
  {"client", T_OBJECT, offsetof(request_t, client), READONLY, "Client info. Set up by set_client method."},
  {NULL}
};
static PyMethodDef request_methods[] = {
  {"write", (PyCFunction)request_write, METH_O, "Write request stream." },
  {"set_writeback", (PyCFunction)set_writeback, METH_O, "Set writeback." },
  {"set_client", (PyCFunction)set_client, METH_O, "Set client info." },
  //{"is_error", (PyCFunction)request_is_error, METH_NOARGS, "Check if request_t object is error." }
  {NULL}
};
static PyTypeObject request_type = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "alivanz.request",             /*tp_name*/
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

int request_prepare(PyObject* m){
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

  PyModule_AddObject(m, "request", (PyObject *)&request_type);
  PyModule_AddObject(m, "status_name", status_name);
  return 0;
}

void initrequest(void){
  PyObject* m = Py_InitModule3("request", methods,"HTTP/1.x request parser modules.");
  if (m == NULL)
    return;
  request_prepare(m);
}

#endif
