#include <Python.h>
#include "structmember.h"
#include "deps/http-header/http_parser.c"
#include <sys/socket.h>
#include "request.h"

/* Request Python __init__ method */
/* prepare initial object, setup parser struct. */
static int request_init(request_t * self, PyObject * args, PyObject * kwargs){
  self->parser.data = self;
  http_parser_init(&self->parser, HTTP_REQUEST);
  // writeback
  Py_INCREF(Py_None);
  self->writeback = Py_None;
  // processor
  self->processor = NULL;
  // method
  self->method = NULL;
  // url
  self->URI = NULL;
  // Query
  self->Query = NULL;
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

/* Request Python __del__ method */
void request_del(request_t * self){
  Py_DECREF(self->writeback);
  Py_XDECREF(self->processor);
  Py_DECREF(self->method);
  Py_DECREF(self->URI);
  Py_DECREF(self->Query);
  Py_DECREF(self->header);
  if(self->last_field != NULL)
    free(self->last_field);
  self->ob_type->tp_free((PyObject*)self);
}

/* Error occured */
static void on_error(request_t * self){
  // get parser
  http_parser* p = &self->parser;

  // get error description
  int errno_ = p->http_errno;
  char * name = http_errno_name(errno_);
  char * desc = http_errno_description(errno_);

  // error callback
  // check if error is available
  if(!PyObject_HasAttrString((PyObject*)self,"on_error")){
    printf("Warning: No error method\n");
    return;
  }

  // Parse error
  if(p->http_errno >= HPE_INVALID_EOF_STATE){
    PyObject * callback_return = PyObject_CallMethod((PyObject*)self, "on_error", "is", 400, "Parse error");
    Py_XDECREF(callback_return);
    return;
  }
  // Callback error
  if(p->http_errno >= HPE_CB_message_begin){
    PyObject * callback_return = PyObject_CallMethod((PyObject*)self, "on_error", "is", self->error, self->error_msg);
    Py_XDECREF(callback_return);
    return;
  }
}


/* Parser execution */
static int on_url(http_parser* p, const char *at, size_t length){
  // type casting
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
}

static int on_header_field(http_parser* p, const char *at, size_t length) {
  // type casting
  request_t * self = p->data;

  // free last stored field
  if(self->last_field != NULL) free(self->last_field);

  // alloc new space
  self->last_field = malloc(sizeof(char)*length);

  // copy
  memcpy(self->last_field, at, sizeof(char)*length);
  self->last_field_length = length;

  return 0;
}

static int on_header_value(http_parser* p, const char *at, size_t length) {
  // type casting
  request_t * data = p->data;

  // setup insertion
  PyObject * key = Py_BuildValue("s#", data->last_field, data->last_field_length);
  PyObject * value = Py_BuildValue("s#",at,length);

  // insert
  PyDict_SetItem(data->header, key, value);

  // free memory
  Py_DECREF(key);
  Py_DECREF(value);

  return 0;
}

static int on_headers_complete(http_parser* p) {
  // type casting
  request_t * self = p->data;

  // update info
  self->header_complete = 1; // DEPRECATED

  // Python callback
  // routing => Get data handler/processor
  if(!PyObject_HasAttrString((PyObject*)self,"routing")){
    self->error = 500;
    self->error_msg = "http class has no routing method";
    return 1;
  }
  PyObject * processor = PyObject_CallMethod((PyObject*)self, "routing", "");
  if(processor==Py_None){
    self->error = 404;
    self->error_msg = "http class routing method return None";
    Py_DECREF(processor);
    return 2;
  }

  // update processor
  self->processor = processor;

  // set writeback
  if(!PyObject_HasAttrString((PyObject*)processor,"set_writeback")){
    self->error = 500;
    self->error_msg = "http class has no set_writeback";
    return 3;
  }
  PyObject * callback_return = PyObject_CallMethod((PyObject*)processor, "set_writeback", "O", self->writeback);
  if(callback_return!=Py_None){
    self->error = 500;
    self->error_msg = "http class set_writeback failed";
    Py_DECREF(callback_return);
    return 4;
  }
  Py_DECREF(callback_return);

  return 0;
}

static int on_message_complete(http_parser* p) {
  // type casting
  request_t * self = p->data;

  // update info
  self->message_complete = 1; // DEPRECATED

  // Python callback, data transfer will be here

  // check if processor available
  if(self->processor==NULL){
    self->error = 404;
    self->error_msg = "http class has no processor";
    return 1;
  }
  if(!PyObject_HasAttrString(self->processor,"write_end")){
    self->error = 500;
    self->error_msg = "http class processor has no write_end";
    return 1;
  }

  // execute processor.process
  PyObject * callback_return = PyObject_CallMethod(self->processor, "write_end", "");

  // free memory
  if(callback_return==Py_None){
    // success
    Py_DECREF(callback_return);
    return 0;
  }else{
    // failure
    self->error = 500;
    self->error_msg = "http class processor write_end failed";
    Py_DECREF(callback_return);
    return 2;
  }
}

/* Callback to other C method like POST multipart, PUT, etc */
/* call "write" method,
  if success must be return None,
  otherwise are considered an failure.
  */
static int on_body(http_parser* p, const char *at, size_t length) {
  // type casting
  request_t * self = p->data;

  // check if processor has write method (receive body part)
  if(!PyObject_HasAttrString(self->processor,"write")){
    self->error = 500;
    self->error_msg = "http class has no write method";
    return 1;
  }

  // execute write method (transfer data)
  PyObject * callback_return = PyObject_CallMethod(self->processor, "write", "s#", at,length);

  // free memory & check if successful
  if(callback_return==Py_None){
    // success
    Py_DECREF(callback_return);
    return 0;
  }else{
    // failure
    self->error = 400;
    self->error_msg = "http class write failure";
    Py_DECREF(callback_return);
    return 2;
  }
}

/* Dummy function */
static int on_info(http_parser* p) {
  return 0;
}
static int on_data(http_parser* p, const char *at, size_t length) {
  return 0;
}

/* Parser Setup */
static http_parser_settings settings = {
  // INFO
  .on_message_begin = on_info,
  .on_headers_complete = on_headers_complete,
  .on_message_complete = on_message_complete,

  // DATA
  .on_header_field = on_header_field,
  .on_header_value = on_header_value,
  .on_url = on_url,
  .on_status = on_data,
  .on_body = on_body
};

/* Parse request
return 0 if success */
static int C_request_write(request_t * self, char * at, size_t length){
  // get parser
  http_parser* p = &self->parser;

  // execute
  int parsed = http_parser_execute(&self->parser, &settings, at, length);

  // check error
  if(parsed != length){
    return 1;
  }
  if(p->http_errno != HPE_OK) return 2;

  return 0;
}

/* Set writeback */
/* All output will be send to client would call this mechanism
  example:
    request.set_writeback( con.sendall )
*/
static PyObject * request_set_writeback(request_t * self, PyObject * writeback){
  // free old writeback
  Py_DECREF(self->writeback);

  // set new writeback
  Py_INCREF(writeback);
  self->writeback = writeback;

  Py_RETURN_NONE;
}

/* Write */
/* Write data to parser from PyString */
static PyObject* request_write(request_t * self, PyObject* buffer){
  // get char
  char * at;
  long int length;
  PyString_AsStringAndSize(buffer, &at, &length);

  // execute
  if(C_request_write(self, at,length)){
    // error occurred
    // call error callback
    on_error(self);
  }

  Py_RETURN_NONE;
}

/* Socket write */
/* C caller */
/* Write data to parser from socket file descriptor */
static void C_request_write_from_socket(request_t * self, int fd){
  // setup
  int length;
  char buffer[MAX_BUFFER];
  http_parser* p = &self->parser;

  // check initial error
  if(p->http_errno != HPE_OK)
    on_error(self);

  // execute
  while(!self->message_complete){
    length = recv(fd,buffer,MAX_BUFFER,0);
    if(C_request_write(self, buffer, length)){
      // error occurred
      on_error(self);
      break;
    }
  }
}
/* Socket write */
/* Python caller */
static PyObject * request_write_from_socket(request_t * self, PyObject * py_fd){
  // get socket file descriptor
  int fd = PyInt_AsLong(py_fd);

  // execute
  C_request_write_from_socket(self,fd);

  Py_RETURN_NONE;
}

/* Get Error status */
static PyObject * request_is_error(request_t * self){
  if(self->parser.http_errno == HPE_OK) Py_RETURN_FALSE;
  else Py_RETURN_TRUE;
}

static PyMemberDef request_members[] = {
  // static
  {"method", T_OBJECT, offsetof(request_t, method), READONLY, "HTTP Method"},
  {"URI", T_OBJECT, offsetof(request_t, URI), READONLY, "Request URI"},
  {"Query", T_OBJECT, offsetof(request_t, Query), READONLY, "Request Query. Used in GET variables."},
  {"header", T_OBJECT_EX, offsetof(request_t, header), READONLY, "HTTP Header"},

  // non-static
  {"respond", T_OBJECT, offsetof(request_t, writeback), READONLY, "Writeback method."},
  {NULL}
};
static PyMethodDef request_methods[] = {
  {"write", (PyCFunction)request_write, METH_O, "Write request stream." },
  {"set_writeback", (PyCFunction)request_set_writeback, METH_O, "Set writeback." },
  {"write_from_socket", (PyCFunction)request_write_from_socket, METH_O, "Write request stream from socket fd." },
  {"is_error", (PyCFunction)request_is_error, METH_NOARGS, "Check if request_t object is error." },
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

static PyMethodDef methods[] = {
  {NULL}
};

int prepare(PyObject* m){
  if (PyType_Ready(&request_type) < 0){
    printf("Type not ready\n");
    return 1;
  }
  Py_INCREF(&request_type);
  PyModule_AddObject(m, "request", (PyObject *)&request_type);
  return 0;
}

void initrequest(){
  PyObject* m = Py_InitModule3("request", methods,"HTTP/1.x request parser modules.");
  if (m == NULL)
    return;
  prepare(m);
}
