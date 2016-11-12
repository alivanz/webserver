#include <Python.h>

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

#define MAX_BUFFER 4096
