#include <Python.h>
#include "structmember.h"

typedef struct{
  PyObject_HEAD

  /* General info */
  int encoding;
  PyObject * py_encoding;
  void * setup; // encoding-specific setup
  PyObject * writeback;

  /* multipart only */
  PyObject * boundary;

  /* Per storage field */
  PyObject * field_key;
  PyObject * field_filename;
  PyObject * field_content_type;

  /* Handler */
  PyObject * room;

  /* Reserved */
  int valid;
} storage_t;

enum{
  encoding_urlencoded, encoding_formdata
} encoding_list;
