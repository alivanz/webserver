#ifndef _TEXTPLAIN
#define _TEXTPLAIN

#include "charmap.h"
#include "textplain.h"
#include <stdlib.h>
#include <string.h>

void textplain_setup(textplain_t * p){
  p->state = textplain_key;
  p->buffer = NULL;
  p->buffer_length = 0;
}

static inline void textplain_free_buffer(textplain_t * p){
  if(p->buffer!=NULL){
    free(p->buffer);
  }
  p->buffer = NULL;
  p->buffer_length = 0;
}
static inline void textplain_append(textplain_t * p, char * at, int length){
  int new_len = p->buffer_length + length;
  p->buffer = realloc(p->buffer, new_len);
  memcpy(p->buffer + p->buffer_length,at, length);
  p->buffer_length = new_len;
}

int textplain_execute(textplain_t * p, textplain_action * action, char * at, int length){
  int i = 0;
  char * b = at;
  int blen = 0;
  while(i<length){
    switch (p->state) {
      case textplain_key:
      if(at[i]=='='){
        textplain_append(p,b,blen);
        action->on_key(p,p->buffer,p->buffer_length);
        textplain_free_buffer(p);
        p->state = textplain_equalmark;
        break;
      }else{
        blen += 1;
        break;
      }

      case textplain_equalmark:
      if(CR(at[i])){
        return i;
      }else if(LF('\n')){
        return i;
      }else{
        b = at+i;
        blen = 1;
        p->state = textplain_value;
        break;
      }

      case textplain_value:
      if(CR(at[i])){
        textplain_append(p,b,blen);
        action->on_data(p,p->buffer,p->buffer_length);
        textplain_free_buffer(p);
        p->state = textplain_cr;
        break;
      }else if(LF(at[i])){
        textplain_append(p,b,blen);
        action->on_data(p,p->buffer,p->buffer_length);
        textplain_free_buffer(p);
        p->state = textplain_lf;
        break;
      }else{
        b = at+i;
        blen = 1;
        p->state = textplain_value;
        break;
      }

      case textplain_cr:
      if(LF(at[i])){
        p->state = textplain_lf;
        break;
      }else{
        return i;
      }

      case textplain_lf:
      b = at+i;
      blen = 1;
      p->state = textplain_key;
      break;

      default:
      return i;
    }
    i++;
  }
  switch (p->state) {
    case textplain_key:
    textplain_append(p,b,blen);
    break;

    case textplain_value:
    textplain_append(p,b,blen);
    action->on_data(p,p->buffer,p->buffer_length);
    textplain_free_buffer(p);
    break;
  }
  return i;
}

#endif
