typedef struct{
  int state;
  char percent_e[2];
  char * buffer;
  int buffer_length;
  void * data;
} urlencoded_t;

enum{
  ue_key, ue_key_percent, ue_key_percent_data,
  ue_value, ue_value_percent, ue_value_percent_data
} urlencoded_state;

typedef int (*urlencoded_data)(urlencoded_t*, char*, int);

typedef struct{
  urlencoded_data on_key;
  urlencoded_data on_data;
} urlencoded_action;
