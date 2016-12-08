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
  if(length==0) return;
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
#define urlencoded_action_data(p,action,at,length) action(p,p->buffer,p->buffer_length)

int urlencoded_execute(urlencoded_t * p, urlencoded_action * action, char * at, int length){
  if(p->state == ue_final) return 0;
  if(length<1){
    /* End signal */
    if(p->state==ue_value){
      p->state = ue_final;
      int r = urlencoded_action_data(p,action->on_data,buffer,blen);
      urlencoded_free(p);
      if(r) return -1;
      action->on_data_end(p);
    }else if(p->state==ue_key){
      p->state = ue_final;
      int r = urlencoded_action_data(p,action->on_key,buffer,blen);
      urlencoded_free(p);
      if(r) return -1;
    }else{
      return -1;
    }
    return 0;
  }
  /* Standard */
  int i = 0;
  // buffer
  char buffer[length];
  int blen = 0;
  // loop
  while(i<length){
    char c = at[i];
    printf("%c\n", c);
    switch (p->state) {
      case ue_key:
      if(c=='='){
        urlencoded_append(p,buffer,blen);
        if(urlencoded_action_data(p,action->on_key,buffer,blen)) return i;
        urlencoded_free(p);
        blen = 0;
        p->state = ue_value;
        break;
      }else if(c=='%'){
        p->state = ue_key_percent;
        break;
      }else if(valid_unreserved(c)){
        buffer[blen] = c;
        blen += 1;
        break;
      }else{
        return i;
      }

      case ue_key_percent:
      if(valid_hexadecimal(c)){
        p->percent_e[0] = c;
        p->state = ue_key_percent_data;
        break;
      }else{
        return i;
      }

      case ue_key_percent_data:
      if(valid_hexadecimal(c)){
        p->percent_e[1] = c;
        buffer[blen] = hexadecimal_decode(p->percent_e);
        blen += 1;
        p->state = ue_key;
        break;
      }else{
        return i;
      }

      case ue_value:
      if(c=='&'){
        urlencoded_append(p,buffer,blen);
        if(p->buffer_length>0){
          if(urlencoded_action_data(p,action->on_data,buffer,blen)) return i;
          urlencoded_free(p);
        }
        action->on_data_end(p);
        blen = 0;
        p->state = ue_key;
        break;
      }else if(c=='%'){
        p->state = ue_value_percent;
        break;
      }else if(valid_unreserved(c)){
        buffer[blen] = c;
        blen += 1;
        break;
      }else{
        return i;
      }

      case ue_value_percent:
      if(valid_hexadecimal(c)){
        p->percent_e[0] = c;
        p->state = ue_value_percent_data;
        break;
      }else{
        return i;
      }

      case ue_value_percent_data:
      if(valid_hexadecimal(c)){
        p->percent_e[1] = c;
        buffer[blen] = hexadecimal_decode(p->percent_e);
        blen += 1;
        p->state = ue_value;
        break;
      }else{
        return i;
      }
    }
    i++;
  }
  if(blen>0){
    urlencoded_append(p,buffer,blen);
    // on partial value
    if(ue_value<=p->state && p->state<=ue_value_percent_data){
      int r = urlencoded_action_data(p,action->on_data,buffer,blen);
      urlencoded_free(p);
      if(r) return i-1;
    }
  }
  return i;
}


#endif
