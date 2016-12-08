typedef struct{
  int state;
  void * data;
} fields_t;

typedef int (*fields_data)(fields_t*, char*, int);

typedef struct{
  fields_data on_key;
  fields_data on_param;
  fields_data on_param_value;
} fields_action;

enum{
  fields_init,
  fields_key, fields_key_space,
  fields_semicolon, fields_semicolon_space,
  fields_param_key, fields_param_key_space,
  fields_equal, fields_equal_space,
  fields_value, fields_value_bracket, fields_value_backlash, fields_value_space
} fields_state;
