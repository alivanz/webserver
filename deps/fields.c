#ifndef _FIELDS
#define _FIELDS

#include "charmap.h"
#include "fields.h"

void fields_setup(fields_t * parser){
  parser->state = fields_init;
}

//#define alphabet ( (('a'<=line[i]) && (line[i]<='z')) || (('A'<=line[i]) && (line[i]<='Z')) || (line[i]=='/'))
#define numeric ( ('0'<=line[i]) && (line[i]<='9') )
#define space ( line[i]==' ' )
#define semicolon ( line[i]==';' )
#define comma ( line[i]==',' )
#define equal ( line[i]=='=' )

#define b_start   b = line+i; blen = 1;
#define b_add     blen += 1;
#define bb_start  bblen = 0;
#define bb_add    bb[bblen] = line[i]; bblen += 1;

int fields_execute(fields_t * parser, fields_action * action, char * line, int length){
  int i = 0;
  char * b = line;
  int blen = 0;
  char bb[length];
  int bblen = 0;
  while(i<length){
    switch (parser->state) {
      case fields_init:
      if(space){
        break;
      }else if(is_alphabet(line[i])){
        b = line +i;
        blen = 1;
        parser->state = fields_key;
        break;
      }else{
        return i;
      }

      case fields_key:
      if(space){
        if(action->on_key(parser,b,blen)) return i;
        parser->state = fields_key_space;
        break;
      }else if(line[i]==';'){
        if(action->on_key(parser,b,blen)) return i;
        parser->state = fields_semicolon;
        break;
      }else if(line[i]==','){
        if(action->on_key(parser,b,blen)) return i;
        parser->state = fields_init;
        break;
      }else if(valid_unreserved_alt(line[i])){
        blen += 1;
        break;
      }else{
        return i;
      }

      case fields_key_space:
      if(space){
        break;
      }else if(line[i]==';'){
        parser->state = fields_semicolon;
        break;
      }else if(line[i]==','){
        parser->state = fields_init;
        break;
      }else{
        return i;
      }

      case fields_semicolon:
      if(space){
        parser->state = fields_semicolon_space;
        break;
      }else if(valid_unreserved_alt(line[i])){
        parser->state = fields_param_key;
        b = line +i;
        blen = 1;
        break;
      }else{
        return i;
      }

      case fields_semicolon_space:
      if(space){
        break;
      }else if(valid_unreserved_alt(line[i])){
        parser->state = fields_param_key;
        b = line +i;
        blen = 1;
        break;
      }else{
        return i;
      }

      case fields_param_key:
      if(space){
        if(action->on_param(parser,b,blen)) return i;
        parser->state = fields_param_key_space;
        break;
      }else if(line[i]=='='){
        if(action->on_param(parser,b,blen)) return i;
        parser->state = fields_equal;
        break;
      }else if(line[i]==';'){
        if(action->on_param(parser,b,blen)) return i;
        parser->state = fields_semicolon;
        break;
      }else if(line[i]==','){
        if(action->on_param(parser,b,blen)) return i;
        parser->state = fields_init;
        break;
      }else if(valid_unreserved_alt(line[i])){
        blen += 1;
        break;
      }else{
        return i;
      }

      case fields_param_key_space:
      if(space){
        break;
      }else if(line[i]=='='){
        parser->state = fields_equal;
        break;
      }else if(line[i]==';'){
        parser->state = fields_semicolon;
        break;
      }else if(line[i]==','){
        parser->state = fields_init;
        break;
      }else{
        return i;
      }

      case fields_equal:
      if(space){
        parser->state = fields_equal_space;
        break;
      }else if(line[i]=='"'){
        bb_start
        parser->state = fields_value_bracket;
        break;
      }else if(valid_unreserved(line[i])){
        parser->state = fields_value;
        b = line +i;
        blen = 1;
        break;
      }else{
        return i;
      }

      case fields_equal_space:
      if(space){
        break;
      }else if(line[i]=='"'){
        bb_start
        parser->state = fields_value_bracket;
        break;
      }else if(valid_unreserved(line[i])){
        parser->state = fields_value;
        b = line +i;
        blen = 1;
        break;
      }else{
        return i;
      }

      case fields_value:
      if(space){
        if(action->on_param_value(parser,b,blen)) return i;
        parser->state = fields_value_space;
        break;
      }else if(line[i]==';'){
        if(action->on_param_value(parser,b,blen)) return i;
        parser->state = fields_semicolon;
        break;
      }else if(line[i]==','){
        if(action->on_param_value(parser,b,blen)) return i;
        parser->state = fields_init;
        break;
      }else if(valid_unreserved(line[i])){
        blen += 1;
        break;
      }else{
        return i;
      }

      case fields_value_bracket:
      if(line[i]=='"'){
        if(action->on_param_value(parser,bb,bblen)) return i;
        parser->state = fields_value_space;
        break;
      }else if(line[i]=='\\'){
        parser->state = fields_value_backlash;
        break;
      }else{
        bb_add
        break;
      }

      case fields_value_backlash:
      bb_add
      parser->state = fields_value_bracket;
      break;

      case fields_value_space:
      if(space){
        break;
      }else if(line[i]==';'){
        parser->state = fields_semicolon;
        break;
      }else if(line[i]==','){
        parser->state = fields_init;
        break;
      }else{
        return i;
      }

      default:
        return i;
    }
    i++;
  }
  /* Finalize */
  switch (parser->state) {
    case fields_init:
    case fields_equal:
    case fields_equal_space:
    case fields_value_bracket:
    case fields_value_backlash:
    i--;
    break;

    case fields_key:
    if(action->on_key(parser,b,blen)) i--;
    break;

    case fields_param_key:
    if(action->on_param(parser,b,blen)) i--;
    break;

    case fields_value:
    if(action->on_param_value(parser,b,blen)) i--;
    break;
  }
  parser->state = fields_init;
  return i;
}

#endif
