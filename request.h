typedef struct{
  PyObject_HEAD
  int fd;
  PyObject * writeback;
  struct http_parser parser;
  PyObject * method;
  PyObject * URI;
  PyObject * Query;
  PyObject * header;
  char * last_field;
  size_t last_field_length;
  int header_complete;
  int message_complete; // DEPRECATED
  int error;
  char * error_msg;
  PyObject * processor;
} request_t;

typedef struct{
  PyObject_HEAD

} multipart_data;

#define MAX_BUFFER 4096
