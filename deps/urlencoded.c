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
  int new_length = ue_buffer_length(p) + length;
  ue_buffer(p) = realloc(ue_buffer(p), new_length);
  memcpy(ue_buffer(p) + ue_buffer_length(p), at, length);
  ue_buffer_length(p) = new_length;
}
static inline void urlencoded_free(urlencoded_t * p){
  if(p->buffer_length>0){
    free(p->buffer);
    p->buffer = NULL;
    p->buffer_length = 0;
  }
}
#define urlencoded_action_data(p,action) action(p,p->buffer,p->buffer_length)

#define do_append_char {\
  buffer[blen] = c;\
  blen += 1;\
}
#define do_store {\
  ue_buffer(p) = realloc(ue_buffer(p), ue_buffer_length(p)+blen);\
  memcpy(ue_buffer(p) + ue_buffer_length(p), buffer, blen);\
  ue_buffer_length(p) += blen;\
  blen = 0;\
}
#define do_free {\
    if(ue_buffer(p)!=NULL){\
      free(ue_buffer(p));\
      ue_buffer(p) = NULL;\
      ue_buffer_length(p) = 0;\
    }\
}

int urlencoded_execute(urlencoded_t * p, urlencoded_action * action, char * at, int length){
  if(p->state == ue_final) return 0;
  if(length==0){
    /* End signal */
    if(p->state==ue_value){
      p->state = ue_final;
      action->on_data_end(p);
    }else if(p->state==ue_key){
      p->state = ue_final;
      int r = urlencoded_action_data(p,action->on_key);
      //urlencoded_free(p);
      do_free
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
    switch (p->state) {
      case ue_key:
      if(c=='='){
        /* Store data */
        if(blen>0) do_store
        /* callback */
        if(urlencoded_action_data(p,action->on_key)) return i;
        /* Free data */
        do_free
        /* Next state */
        p->state = ue_value;
        break;
      }else if(c=='%'){
        /* Next state */
        p->state = ue_key_percent;
        break;
      }else if(valid_unreserved(c)){
        /* store char */
        do_append_char
        break;
      }else{
        /* invalid */
        return i;
      }

      case ue_key_percent:
      if(valid_hexadecimal(c)){
        /* set char */
        p->cc = hexadecimal_byte(c)<<4;
        /* Next state */
        p->state = ue_key_percent_data;
        break;
      }else{
        /* invalid */
        return i;
      }

      case ue_key_percent_data:
      if(valid_hexadecimal(c)){
        /* store char */
        p->cc |= hexadecimal_byte(c);
        buffer[blen] = p->cc;
        blen += 1;
        /* Next state */
        p->state = ue_key;
        break;
      }else{
        /* invalid */
        return i;
      }

      case ue_value:
      if(c=='&'){
        /* Store data */
        if(blen>0) do_store
        /* callback */
        if(ue_buffer_length(p)>0){
          if(urlencoded_action_data(p,action->on_data)) return i;
          /* free data */
          do_free
        }
        /* callback */
        action->on_data_end(p);
        /* Next state */
        p->state = ue_key;
        break;
      }else if(c=='%'){
        /* Next state */
        p->state = ue_value_percent;
        break;
      }else if(valid_unreserved(c)){
        /* store char */
        do_append_char
        break;
      }else{
        /* invalid */
        return i;
      }

      case ue_value_percent:
      if(valid_hexadecimal(c)){
        /* store char */
        buffer[blen] = hexadecimal_byte(c)<<4;
        /* Next state */
        p->state = ue_value_percent_data;
        break;
      }else{
        /* invalid */
        return i;
      }

      case ue_value_percent_data:
      if(valid_hexadecimal(c)){
        /* store char */
        buffer[blen] |= hexadecimal_byte(c);
        blen += 1;
        /* Next state */
        p->state = ue_value;
        break;
      }else{
        /* invalid */
        return i;
      }
    }
    i++;
  }
  if(blen>0){
    /* store data */
    do_store
    /* callback */
    if(ue_value<=p->state && p->state<=ue_value_percent_data && ue_buffer_length(p)>0){
      /* call */
      int r = urlencoded_action_data(p,action->on_data);
      /* free data */
      do_free
      /* check validity */
      if(r) return i-1;
    }
  }
  return i;
}


#endif
