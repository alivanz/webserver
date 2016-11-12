#ifndef MAX_BUFFER
  #define MAX_BUFFER 4096
#endif

typedef struct{
  PyObject_HEAD
  multipart_parser* parser;
  PyObject * last_field;
  PyObject * header;
  PyObject * boundary;
  PyObject * post_handler;
  PyObject * data_object;
  PyObject * finished;
  PyObject * error;
} multipart;
