#include <Python.h>
#include "structmember.h"
#include "deps/http-header/http_parser.c"
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

  Py_INCREF(Py_None);
  self->writeback = Py_None;
  Py_INCREF(Py_None);
  self->handler = Py_None;
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

/* Parser execution */
static int on_url(http_parser* p, const char *at, size_t length){
  request_t * self = p->data;
  self->method  = Py_BuildValue("s", http_method_str(p->method) );
  self->URI     = Py_BuildValue("s#",at,length);
  // URL & query
  size_t i = 0;
  while(i<length && at[i]!='?') i += 1;
  if(i<length){
    i += 1;
    self->Query = Py_BuildValue("s#",at+i,length-i);
  }else{
    self->Query = Py_BuildValue("s","");
  }
  self->URL     = Py_BuildValue("s#",at,i);

  return 0;
}
static int on_header_field(http_parser* p, const char *at, size_t length){
  request_t * self  = p->data;
  self->field       = Py_BuildValue("s#",at,length);
  return 0;
}
static int on_header_value(http_parser* p, const char *at, size_t length) {
  request_t * self = p->data;
  PyObject * value = Py_BuildValue("s#",at,length);
  PyDict_SetItem(self->header, self->field, value);
  Py_DECREF(self->field);
  self->field = NULL;
  Py_DECREF(value);
  return 0;
}
static int on_headers_complete(http_parser* p){
  request_t * self = p->data;

  // get handler
  // Add history here, for debugging (NOT IMPLEMENTED YET)
  PyObject * result = call_routing(self);
  if(result==NULL){
    // call_request_on_error(self,500,"Routing failure.");
    //printf("routing failed\n");
    //self->error = 500;
    return -1;
  }else if(result==Py_None){
    call_request_on_error(self,404,"No routing available.");
    //self->error = 404;

    Py_DECREF(result);
    //PyErr_SetString(exception,"No routing found");
    return -1;
  }else{
    Py_DECREF(self->handler);
    self->handler = result;
  }
  /*Py_EnterRecursiveCall("Call request.routing()");
  PyObject * handler = PyObject_CallMethod((PyObject*)self,"routing","");
  Py_LeaveRecursiveCall();
  if(handler==NULL){ // exception
    // printf("request.routing failed :\n");
    self->error = 500;
    return -1;
  }else if(handler==Py_None){
    self->error = 404;
    Py_DECREF(handler);
    PyErr_SetString(exception,"No routing found");
    return -1;
  }
  Py_DECREF(self->handler);
  self->handler = handler;*/

  // set_writeback
  // Also fix bug, (unfreed result)
  result = call_routing_set_writeback(self);
  if(result==NULL){
    // call_request_on_error(self,500,"Failed to set writeback on Routing object.");
    //self->error = 500;
    //PyErr_SetString(exception,"(request->routing).set_writeback failed.");
    return -1;
  }else{
    Py_DECREF(result);
    return 0;
  }
  /*Py_EnterRecursiveCall("Call (request->routing).set_writeback( writeback )");
  PyObject * result = PyObject_CallMethod((PyObject*)handler,"set_writeback","O",self->writeback);
  Py_LeaveRecursiveCall();
  if(result==NULL){
    self->error = 500;
    PyErr_SetString(exception,"(request->routing).set_writeback failed.");
    return -1;
  }

  return 0;*/
}

static int on_body(http_parser* p, const char *at, size_t length) {
  request_t * self = p->data;

  // PyObject * handler = self->handler; <=== disabled
  // get handler
  PyObject * result = call_routing_write(self, at, length);
  if(result == NULL){
    // call_request_on_error(self,500,"Failed to write stream on Routing object.");
    return -1;
  }else if(result == Py_True){
    Py_DECREF(result);
    return 0;
  }else if(result == Py_None){
    // stop without exception
    self->error = JUST_TERMINATE;
    Py_DECREF(result);
    return -1;
  }else{
    // Note that maybe Py_False
    // call_request_on_error(self,500,"Routing object return unexpected value while writing.");
    //self->error = 500;
    //PyErr_SetString(exception,"(request->routing).write return unexpected value.");
    Py_DECREF(result);
    return -1;
  }
  /*Py_EnterRecursiveCall("Call (request->routing).write( buffer )");
  PyObject * result = PyObject_CallMethod((PyObject*)handler,"write","s#", at,length);
  Py_LeaveRecursiveCall();
  if(result==NULL){
    self->error = 500;
    //printf("(request->routing).write failed :\n");
    return -1;
  }else if(result==Py_True){
    Py_DECREF(result);
    return 0;
  }else if(result==Py_None){
    // stop without exception
    Py_DECREF(result);
    return -1;
  }else{ // maybe Py_False
    self->error = 500;
    PyErr_SetString(exception,"(request->routing).write return unexpected value.");
    Py_DECREF(result);
    return -1;
  }*/
}

static int on_message_complete(http_parser* p) {
  request_t * self = p->data;

  // finalize
  self->completed = 1;

  // bug fix, unfreed memory
  PyObject * result = call_routing_write_end(self);
  if(result == NULL){
    //printf("(request->routing).write_end failed. (request.on_error will not called)\n");

    // traceback
    //PyTracebackObject* traceback = get_the_traceback();
    //int line = traceback->tb_lineno;
    //const char* filename = PyString_AsString(traceback->tb_frame->f_code->co_filename);
    //printf("%s\n", filename);

    // self->error = JUST_TERMINATE;
    return -1;
  }else if(result == Py_None){
    Py_DECREF(result);
    return 0;
  }else{
    //self->error = 500;
    //printf("(request->routing).write_end return unexpected value.\n");
    //self->error = JUST_TERMINATE;
    PyErr_SetString(PyExc_TypeError,"handler write_end method return unexpected value");
    Py_DECREF(result);
    return -1;
  }
  /*Py_EnterRecursiveCall("Call (request->routing).write_end");
  PyObject * result = PyObject_CallMethod((PyObject*)self->handler,"write_end","");
  Py_LeaveRecursiveCall();
  if(result==NULL){
    // on_error not called
    printf("(request->routing).write_end failed. (request.on_error will not called)\n");
    return -1;
  }else if(result!=Py_None){
    self->error = 500;
    PyErr_SetString(exception,"(request->routing).write_end return unexpected value.");
    Py_DECREF(result);
    return -1;
  }
  return 0;*/
}

static PyObject * request_write(request_t * self, PyObject * buffer){
  struct http_parser * p = &self->parser;

  if(!PyString_CheckExact(buffer)){
    PyErr_SetString(PyExc_TypeError,"Invalid data type, string expected.");
    return NULL;
  }
  char * at;
  long int length;
  PyString_AsStringAndSize(buffer, &at, &length);

  //Py_EnterRecursiveCall("Call (request) C http_parser_execute");
  long int parsed = http_parser_execute(p, &settings, at, length);
  //Py_LeaveRecursiveCall();


  /* NEW MODEL */
  if(PyErr_Occurred()!=NULL) return NULL;

  /* Parse Error */
  if(p->http_errno >= HPE_INVALID_EOF_STATE){
    //printf("Parse error\n");
    //self->error = 400;
    //PyObject * result = call_request_on_error(self,400,"Parse Error");
    call_request_on_error(self,400,"Parse Error");
    if(self->error == JUST_TERMINATE){
      //PyErr_Clear();
      Py_RETURN_FALSE;
    }else{
      Py_RETURN_FALSE;
    }
  }

  /* Callback Error */
  if(p->http_errno >= HPE_OK){
    //printf("Callback error\n");
    //printf("%i\n", self->error);
    if(self->error == JUST_TERMINATE){
      //PyErr_Clear();
      Py_RETURN_NONE;
    }else{
      Py_RETURN_FALSE;
    }
    /*
    PyObject * err = PyErr_Occurred();
    if(err==NULL){
      // C Code error / Bad Style
      // Internal Server Error
      switch (self->error) {
        case JUST_TERMINATE:
          Py_RETURN_NONE;
        default:
          // Set error
          PyErr_SetString(PyExc_SystemError, "C Code error / Bad Style");
          // Continue to detected error
      }
      // Different from old logic, that possible callback error without exception, then call handler.write_end
    }
    // Detected Error
    // Dump to on_error
    self->error = 500;
    PyObject * result = call_request_on_error(self);
    if(result==NULL){
      // Fatal Error
      // Unable to dump error
      printf("Fatal Error, Unable to dump error.\n");
      Py_RETURN_FALSE;
    }else{
      Py_DECREF(result);
      Py_RETURN_FALSE;
    }
    */
  }

  // All is going well
  Py_RETURN_TRUE;

  /*if( self->parser.http_errno != HPE_OK || parsed!=length ){ // error detected
    PyObject * exc = PyErr_Occurred();
    if(exc==NULL){ // no exception
      Py_EnterRecursiveCall("Call (request->routing).write_end");
      PyObject * result = PyObject_CallMethod(self->handler,"write_end","");
      Py_LeaveRecursiveCall();
      if(result==NULL){
        exc = PyErr_Occurred();
      }else{
        Py_RETURN_NONE;
      }
    }
    if(exc!=NULL){ // exception
      PyObject *ptype, *pvalue, *ptraceback;
      PyErr_Fetch(&ptype, &pvalue, &ptraceback);
      PyErr_Clear();
      if(self->error){ // error != 0
        Py_EnterRecursiveCall("Call request.on_error( code, pvalue )");
        PyObject * result = PyObject_CallMethod((PyObject*)self,"on_error","iO", self->error, pvalue);
        Py_LeaveRecursiveCall();
        if(result==NULL) return NULL;
        Py_DECREF(result);
      }
      Py_RETURN_FALSE;
    }
  }

  if(self->completed) Py_RETURN_NONE;

  Py_RETURN_TRUE;*/
}

/* OPTIONS */
static PyObject * set_writeback(request_t * self, PyObject * writeback){
  Py_DECREF(self->writeback);
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
