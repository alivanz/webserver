typedef struct{
  int state;
  char * buffer;
  int buffer_length;
  void * data;
} textplain_t;

enum{
  textplain_key, textplain_equalmark, textplain_value, textplain_cr, textplain_lf
} textplain_state;

typedef int (*textplain_data)(textplain_t*, char*, int);

typedef struct{
  textplain_data on_key;
  textplain_data on_data;
} textplain_action;
