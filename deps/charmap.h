#ifndef _CHARMAP
#define _CHARMAP

#define CR(c) (c=='\r')
#define LF(c) (c=='\n')

#define is_alphabet(c) (('a'<=c && c<='z') || ('A'<=c && c<='Z'))
#define is_numeric(c)  ('0'<=c && c<='9')
#define is_alphanumeric(c) (is_alphabet(c)||is_numeric(c))
#define valid_unreserved(c) (is_alphanumeric(c) || c=='-' || c=='_' || c=='.' || c=='~')
#define valid_unreserved_alt(c) (valid_unreserved(c) || c=='/')

#define valid_hexadecimal(c) (is_numeric(c) || ('a'<=c && c<='f') || ('A'<=c && c<='F'))
char hexadecimal_decode(char * c2){
  char l = 0,h = 0;
  switch (c2[0]) {
    case '0'...'9': h = c2[0]-'0'; break;
    case 'a'...'z': h = c2[0]-'a'+10; break;
    case 'A'...'Z': h = c2[0]-'A'+10; break;
  }
  switch (c2[1]) {
    case '0'...'9': l = c2[1]-'0'; break;
    case 'a'...'z': l = c2[1]-'a'+10; break;
    case 'A'...'Z': l = c2[1]-'A'+10; break;
  }
  return (h<<4)+l;
}
char hexadecimal_byte(char c){
  switch (c) {
    case '0'...'9': return c-'0';
    case 'a'...'z': return c-'a'+10;
    case 'A'...'Z': return c-'A'+10;
  }
  return 0;
}


/* Check chars, chars_check("apaya?", at,length) */
int chars_check(char* src, char* dst, int length){
  int i = 0;
  while(i<length){
    if(src[i]==dst[i]) i++;
    else return 0;
  }
  if(src[i]=='\0') return 1;
  else return 0;
}

#endif
