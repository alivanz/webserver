typedef struct{
  PyObject_HEAD

  struct http_parser parser;
  PyObject * method;
  PyObject * URI;
  PyObject * URL;
  PyObject * Query;
  PyObject * version;

  PyObject * field;
  PyObject * header;

  PyObject * writeback;
  PyObject * handler;
  PyObject * client;

  int completed;
  int error;
} request_t;

static int on_url(http_parser* p, const char *at, size_t length);
static int on_header_field(http_parser* p, const char *at, size_t length);
static int on_header_value(http_parser* p, const char *at, size_t length);
static int on_headers_complete(http_parser* p);
static int on_body(http_parser* p, const char *at, size_t length);
static int on_message_complete(http_parser* p);

/* Dummy function */
static int on_info(http_parser* p) {
  return 0;
}
static int on_data(http_parser* p, const char *at, size_t length) {
  printf("%.*s\n", length,at);
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
/*static http_parser_settings dummy_settings = {
  // INFO
  .on_message_begin = on_info,
  .on_headers_complete = on_info,
  .on_message_complete = on_info,

  // DATA
  .on_header_field = on_data,
  .on_header_value = on_data,
  .on_url = on_data,
  .on_status = on_data,
  .on_body = on_data
};*/

// exception
PyObject * exception;
//#define Raise(msg) if(PyErr_Occurred()==NULL) PyErr_SetString(exception,msg)
//#define ForceRaise(msg) if(PyErr_Occurred()==NULL) PyErr_SetString(exception,msg)

// SPECIAL ERROR CODE / SIGNAL
#define JUST_TERMINATE 1000

// OPT CONSTANT
#define REQUEST_OPT(XX) \
  XX(100,   WRITEBACK) \
  XX(101,   HOST)
