#ifndef _URLENCODED
#define _URLENCODED

#include <stdlib.h>
#include <string.h>
#include "charmap.h"
#include "urlencoded.h"

void urlencoded_setup(urlencoded_t * p){
  p->state = ue_key;
  p->buffer = NULL;
  p->buffer_length = 0;
}
static inline void urlencoded_append(urlencoded_t * p, char * at, int length){
  int new_length = p->buffer_length + length;
  p->buffer = realloc(p->buffer, new_length);
  memcpy(p->buffer + p->buffer_length, at, length);
  p->buffer_length = new_length;
}
static inline void urlencoded_free(urlencoded_t * p){
  if(p->buffer_length>0){
    free(p->buffer);
    p->buffer = NULL;
    p->buffer_length = 0;
  }
}

int urlencoded_execute(urlencoded_t * p, urlencoded_action * action, char * at, int length){
  int i = 0;
  char buffer[length];
  int blen = 0;
  while(i<length){
    switch (p->state) {
      case ue_key:
      if(at[i]=='='){
        urlencoded_append(p,buffer,blen);
        if(action->on_key(p,p->buffer,p->buffer_length)) return i;
        urlencoded_free(p);
        blen = 0;
        p->state = ue_value;
        break;
      }else if(at[i]=='%'){
        p->state = ue_key_percent;
        break;
      }else if(valid_unreserved(at[i])){
        buffer[blen] = at[i];
        blen += 1;
        break;
      }else{
        return i;
      }

      case ue_key_percent:
      if(valid_hexadecimal(at[i])){
        p->percent_e[0] = at[i];
        p->state = ue_key_percent_data;
        break;
      }else{
        return i;
      }

      case ue_key_percent_data:
      if(valid_hexadecimal(at[i])){
        p->percent_e[1] = at[i];
        p->state = ue_key;
        buffer[blen] = hexadecimal_decode(p->percent_e);
        blen += 1;
        break;
      }else{
        return i;
      }

      case ue_value:
      if(at[i]=='&'){
        urlencoded_append(p,buffer,blen);
        if(action->on_data(p,p->buffer,p->buffer_length)) return i;
        urlencoded_free(p);
        blen = 0;
        p->state = ue_key;
        break;
      }else if(at[i]=='%'){
        p->state = ue_value_percent;
        break;
      }else if(valid_unreserved(at[i])){
        buffer[blen] = at[i];
        blen += 1;
        break;
      }else{
        return i;
      }

      case ue_value_percent:
      if(valid_hexadecimal(at[i])){
        p->percent_e[0] = at[i];
        p->state = ue_value_percent_data;
        break;
      }else{
        return i;
      }

      case ue_value_percent_data:
      if(valid_hexadecimal(at[i])){
        p->percent_e[1] = at[i];
        p->state = ue_value;
        buffer[blen] = hexadecimal_decode(p->percent_e);
        blen += 1;
        break;
      }else{
        return i;
      }
    }
    i += 1;
  }
  if(blen>0)
    urlencoded_append(p,buffer,blen);
  return i;
}
int urlencoded_finalize(urlencoded_t * p, urlencoded_action * action){
  switch (p->state) {
    case ue_key:
    case ue_key_percent:
    case ue_key_percent_data:
    if(action->on_key(p,p->buffer,p->buffer_length)) return -1;
    urlencoded_free(p);

    case ue_value:
    case ue_value_percent:
    case ue_value_percent_data:
    if(action->on_data(p,p->buffer,p->buffer_length)) return -1;
    urlencoded_free(p);
  }
  return 0;
}

#endif
