#include <Python.h>
#include "structmember.h"

typedef struct{
  PyObject_HEAD

  /* Setup */
  unsigned int max_key_length;

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
  int ready;
} storage_t;

/*enum{
  encoding_urlencoded, encoding_formdata
} encoding_list;*/

//  typedef struct{
//    storage_t * self;
//    multipart_parser multipart_setup;
//    int field_type;
//    fields_t fields_setup;
//  } multipart_info;
