typedef struct{
  PyObject_HEAD
  char * data;
  Py_ssize_t length;
  Py_ssize_t max_length;
} string_t;

typedef struct{
  PyObject_HEAD
  long data;
} integer_t;
