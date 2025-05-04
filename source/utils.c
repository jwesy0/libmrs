/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>

#include "zlib.h"

#ifdef _LIBMRS_DBG
#define _dbgprintf(...) printf(__VA_ARGS__);
#define dbgprintf(...) printf("%s: ", __FUNCTION__); \
                       printf(__VA_ARGS__); \
                       printf("\n")
#else
#define _dbgprintf(...)
#define dbgprintf(...)
#endif

void _hex_dump(const unsigned char* buf, size_t size) {
    unsigned i;
    _dbgprintf("Hex dump, %u bytes:\n", size);
    for (i = 0; i < size; i++) {
        if (i && i % 16 == 0)
            _dbgprintf("\n");
        _dbgprintf("%02x ", *(buf+i));
    }
    _dbgprintf("\n");
}

int _get_fnum(const char* s, unsigned* n, char** offset){
  char* par;
  unsigned num = 0;
  
  if(offset)
    *offset = NULL;
  
  if(s[strlen(s)-1] == ')'){
    par = strrchr(s, '(');
    if(par){
      if(*(par-1) != ' ')
        return 1;
      
      if(offset)
        *offset = par-1;
      
      par++;
      while(*par != ')'){
        if(!isdigit(*par)){
          if(offset)
            *offset = NULL;
          return 1;
        }
        num = num * 10 + (*par - '0');
        par++;
      }
      par++;
      if(*par){
        if(offset)
          *offset = NULL;
        return 1;
      }
      if(n)
        *n = num;
      return 0;
    }
  }
  
  return 1;
}

/// Replace slash with backslash
int _strbkslash(char* s, size_t size){
  unsigned i = 0;
  int r = 1;
  
  while(*(s+i)){
    if(size && i>=size)
      break;
    if(*(s+i) == '/'){
      *(s+i) = '\\';
      r = 0;
    }
    i++;
  }
  
  return r;
}

/// Replace backslash with slash
int _strslash(char* s, size_t size){
  unsigned i = 0;
  int r = 1;
  
  while(*(s+i)){
    if(size && i>=size)
      break;
    if(*(s+i) == '\\'){
      *(s+i) = '/';
      r = 0;
    }
    i++;
  }
  
  return r;
}

int _has_invalid_character(const char* s){
  while(*s){
    if(*(s) > 0 && *(s) < 32){
      return 1;
    }
    switch(*s){
    case '<':
    case '>':
    case ':':
    case '\"':
    case '|':
    case '?':
    case '*':
      return 1;
    }
    s++;
  }
  
  return 0;
}

/// TODO: make it UTF-8, so ¹ ² ³ characters can be verified correctly
int _has_invalid_dir_or_filename(const char* s){
  char* invalid_names[] = {
    "CON",  "PRN",  "AUX",  "NUL",  "COM0", "COM1", "COM2", "COM3",
    "COM4", "COM5", "COM6", "COM7", "COM8", "COM9", "COM¹", "COM²",
    "COM³", "LPT0", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6",
    "LPT7", "LPT8", "LPT9", "LPT¹", "LPT²", "LPT³"
  };
  char* s_temp;
  char* temp;
  unsigned i;
  
  s_temp = (char*)malloc(strlen(s)+1);
  strcpy(s_temp, s);
  
  temp = strtok(s_temp, "/");
  do{
    dbgprintf("[%s] is valid?", temp);
    if(temp[strlen(temp)-1] == '.'){
      free(s_temp);
      return 1; // Name ends with a period
    }
    for(i=0; i<sizeof(invalid_names)/sizeof(char*); i++){
      if(!stricmp(temp, invalid_names[i])){
        free(s_temp);
        return 1; // Invalid name
      }
    }
  }while((temp = strtok(NULL, "/")));
  
  free(s_temp);
  
  return 0;
}

/// Check if s is a valid output filename
int _is_valid_output_filename(const char* s){
  unsigned i = 0;
  size_t s_len;
  char* ext;
  char* temp;
  
  dbgprintf("(where s = %s)", s);
  
  if(!s || !strlen(s))
    return 4; // null or empty
  
  s_len = strlen(s);
  temp = (char*)malloc(s_len+1);
  strcpy(temp, s);
  
  _strslash(temp, 0);
  
  // Drive, we skip it
  if((temp[0] >= 'A' && temp[0] <= 'Z') && temp[1] == ':' && temp[2] == '/'){
    dbgprintf("Drive: %.*s", 3, temp);
    memmove(temp, temp+3, strlen(temp) - 2);
  }
  
  dbgprintf("%s", temp);
  
  ext = strrchr(temp, '.');
  if(ext){
    if(!strcmp(ext, ".")){
      free(temp);
      return 1; // Invalid extension
    }
    dbgprintf("Ext: %s", ext);
    *ext = 0;
  }
  
  if(_has_invalid_character(temp))
    return 2; // Invalid character
  
  if(_has_invalid_dir_or_filename(temp))
    return 3;
  
  dbgprintf("%s", temp);
  
  free(temp);
  
  return 0; // Valid file name
}

/// Check if s is a valid filename
int _is_valid_input_filename(const char* s){
  unsigned i = 0;
  size_t s_len;
  char* ext;
  char* temp;
  
  if(!s || !strlen(s))
    return 4; // null or empty
  
  dbgprintf("(where s = %s)", s);
  
  s_len = strlen(s);
  temp = (char*)malloc(s_len+1);
  strcpy(temp, s);
  
  _strslash(temp, 0);
  
  dbgprintf("%s", temp);
  
  ext = strrchr(temp, '.');
  if(ext){
    if(!strcmp(ext, ".")){
      free(temp);
      return 1; // Invalid extension
    }
    dbgprintf("Ext: %s", ext);
    *ext = 0;
  }
  
  if(_has_invalid_character(temp))
    return 2; // Invalid character
  
  if(_has_invalid_dir_or_filename(temp))
    return 3;
  
  dbgprintf("%s", temp);
  
  free(temp);
  
  return 0; // Valid file name
}

int _uncompress_file(unsigned char* inbuf, size_t total_in, unsigned char* outbuf, size_t uncompressed_size, size_t* out_size){
    z_stream zstream;
    int e;

    zstream.zalloc = Z_NULL;
    zstream.zfree  = Z_NULL;
    zstream.opaque = Z_NULL;
    zstream.next_in   = (Bytef*)inbuf;
    zstream.avail_in  = total_in;
    zstream.next_out  = (Bytef*)outbuf;
    zstream.avail_out = uncompressed_size;

    e = inflateInit2(&zstream, -MAX_WBITS);
    if(e != Z_OK)
        return 1;
    
    e = inflate(&zstream, Z_FINISH);
    if(e != Z_STREAM_END)
        return 1;
    
    dbgprintf("File inflated from %u bytes to %u", total_in, zstream.total_out);
    if(out_size)
        *out_size = zstream.total_out;

    inflateEnd(&zstream);

    return 0;
}

int _compress_file(unsigned char* inbuf, size_t total_in, unsigned char** outbuf, size_t* total_out){
    z_stream zstream;
    int e;

    *outbuf = (unsigned char*)malloc(total_in + 16);
    if(!(*outbuf))
        return 0;

    zstream.zalloc = Z_NULL;
    zstream.zfree  = Z_NULL;
    zstream.opaque = Z_NULL;
    zstream.next_in   = (Bytef*)inbuf;
    zstream.avail_in  = total_in;
    zstream.next_out  = (Bytef*)*outbuf;
    zstream.avail_out = total_in + 16;

    e = deflateInit2(&zstream, 9, Z_DEFLATED, -MAX_WBITS, 9, Z_DEFAULT_STRATEGY);
    if(e != Z_OK){
        free(*outbuf);
        *outbuf = NULL;
        return 0;
    }

    e = deflate(&zstream, Z_FINISH);
    if(e != Z_STREAM_END){
        free(*outbuf);
        *outbuf = NULL;
        return 0;
    }

    dbgprintf("File compressed: from %u bytes to %u bytes", total_in, zstream.total_out);
    (*total_out) = zstream.total_out;

    deflateEnd(&zstream);

    return 1;
}

int _mkdirs(const char* s){
  char* temp;
  char* slsh;

  temp = (char*)malloc(strlen(s)+1);
  strcpy(temp, s);

  _strbkslash(temp, 0);

  slsh = temp;
  while((slsh = strchr(slsh, '\\'))){
    *slsh = 0;
    CreateDirectoryA(temp, NULL);
    *slsh = '\\';
    slsh++;
  }
  CreateDirectoryA(temp, NULL);

  free(temp);

  return 0;
}
