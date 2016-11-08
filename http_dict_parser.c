typedef struct{
  int state;
  char * buffer;
  size_t buffer_length;
  void * data;    // Reserve
} http_dict_parser;
void http_dict_parser_init(http_dict_parser* parser){
  parser->state = initial_key_space;
  parser->buffer = NULL;
  parser->buffer_length = 0;
}

// STATEs
enum HTTP_DICT_STATE{
  initial_key_space, key, end_key_space,          //  key
  initial_subkey_space, subkey, end_subkey_space  // subkey
  initial_value_space, value, end_value_space     // value
  error_state                                     // error
}

typedef int fx_info(http_dict_parser*);
typedef int fx_data(http_dict_parser*, char*, size_t);

typedef struct{
  fx_data on_key : NULL;
  fx_data on_subkey : NULL;
  fx_data on_value : NULL;
} http_dict_parser_settings;

// MAIN parser
// return 0 if success
int parse_http_dict(http_dict_parser * parser, char * buffer, size_t * length){

}

void do_initial_key_space(http_dict_parser * parser, char * buffer, size_t * length){
  size_t i = 0;
  while(i<length && buffer[i]==' ') i += 1;
  if(i<length){ // non space
    parser->state = key;
  }
}
void do_key(http_dict_parser * parser, char * buffer, size_t * length){
  size_t i = 0;
  while(i<length && buffer[i]==' ') i += 1;
  if(i<length){ // non space
    parser->state = key;
  }
}
