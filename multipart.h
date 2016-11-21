#ifndef MAX_BUFFER
  #define MAX_BUFFER 4096
#endif

typedef struct{
  PyObject_HEAD
  multipart_parser* parser;
  PyObject * boundary;

  // header
  PyObject * last_field;
  PyObject * header;

  // handler
  PyObject * var_handler;

  // writeback
  PyObject * writeback;

  // error
  PyObject * error;
} multipart;

multipart_parser_settings settings;

int on_header_field(multipart_parser* p, const char *at, size_t length);
int on_header_value(multipart_parser* p, const char *at, size_t length);
int on_headers_complete(multipart_parser* p);
int on_part_data_begin(multipart_parser* p);
int on_part_data(multipart_parser* p, const char *at, size_t length);
int on_part_data_end(multipart_parser* p);
int on_body_end(multipart_parser* p);

static int multipart_init(multipart * self, PyObject * args, PyObject * kwargs);
void multipart_del(multipart * self);

PyObject * multipart_set_writeback(multipart * self, PyObject * writeback);
static int C_multipart_write(multipart * self, char * buffer, int length);
PyObject * multipart_write(multipart * self, PyObject * args);
