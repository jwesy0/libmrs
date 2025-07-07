/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#define __LIBMRS_INTERNAL__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#include "mrs.h"
#include "mrs_internal.h"
#include "mrs_dbg.h"

extern int _get_fnum(const char* s, unsigned* n, char** offset);

void _mrs_file_free(MRS* mrs, struct mrs_file_t* f) {
    if (f->lh.filename != f->dh.filename) {
        dbgprintf("We got different filenames between LOCAL and CENTRAL DIR headers");
        free(f->lh.filename);
        dbgprintf("Freed LOCAL filename");
    }
    free(f->dh.filename);
    dbgprintf("Freed CENTRAL DIR filename");
    f->lh.filename = NULL;
    f->dh.filename = NULL;
    f->lh.extra    = NULL;
    f->dh.extra    = NULL;
    f->dh.comment  = NULL;
}

int _mrs_is_initialized(const MRS* mrs){
  if(!mrs || !mrs->_ptr)
    return 0;
  return 1;
}

int _mrs_is_duplicate(const MRS* mrs, const char* s, char** s_out, unsigned* match_index){
  char* s_temp;
  char  temp2[256];
  char* temp;
  char  my_ext[64];
  unsigned my_num = 0;
  unsigned num = 0;
  unsigned e, i;
  unsigned match = 0;
  unsigned* n   = NULL;
  size_t n_size = 0;
  
  s_temp = (char*)malloc(strlen(s)+1);
  strcpy(s_temp, s);
  
  temp = strrchr(s_temp, '.');
  if(temp){
    strcpy(my_ext, temp);
    *temp = 0;
  }
  
  _get_fnum(s_temp, &my_num, &temp);
  if(temp)
    *temp = 0;
  
  dbgprintf("\"%s\"", s_temp);
  dbgprintf("My extension: %s", my_ext);
  dbgprintf("My num: %u", my_num);
  
  for(i=0; i<mrs->_hdr.dir_count; i++){
    strcpy(temp2, mrs->_files[i].dh.filename);
    dbgprintf("%s", temp2);
    if(!stricmp(temp2, s) && !match){
      match = 1;
      if(match_index)
        *match_index = i;
      dbgprintf(" EXACT MATCH");
    }
    temp = strrchr(temp2, '.');
    dbgprintf(" Extension: %s == %s?", temp, my_ext);
    if((my_ext && !temp) || (!my_ext && temp) || (my_ext && temp && stricmp(my_ext, temp))) // Not same extension, skip it then...
      continue;
    dbgprintf("            OK");
    
    *temp = 0;
    e = _get_fnum(temp2, &num, &temp);
    if(!e){
      *temp = 0;
      dbgprintf(" Num: %u", num);
    }
    
    dbgprintf("%s == %s?", temp2, s_temp);
    if(!stricmp(s_temp, temp2)){
      dbgprintf("%u", e);
      if(e)
        continue;
      
      dbgprintf("adding %u to the n list...", num);
      
      n = (unsigned*)realloc(n, sizeof(unsigned)*(n_size+1));
      e = 0;
      if(n_size){
        for(e=0; e<n_size; e++){
          if(num < n[e])
            break;
        }
        memmove(n+e+1, n+e, sizeof(unsigned)*(n_size-e));
      }
      n[e] = num;
      n_size++;
    }
  }
  
  if (/*n || */match) {
    if (match)
        my_num = 2;
    if (n) {
        for (i = 0; i < n_size; i++) {
            if (my_num < n[i])
                break;
            if (my_num == n[i])
                my_num++;
        }
        free(n);
    }
    dbgprintf("My final num is: %u", my_num);
    dbgprintf("%s (%u)%s", s_temp, my_num, my_ext ? my_ext : "");
    if (s_out) {
        sprintf(temp2, "%s (%u)%s", s_temp, my_num, my_ext ? my_ext : "");
        *s_out = (char*)malloc(strlen(temp2) + 1);
        strcpy(*s_out, temp2);
    }
  }
  else {
      free(s_temp);
      return 1;
  }
  
  free(s_temp);
  dbgprintf("Freed");
  
  return 0;
}

void _mrs_push_file(MRS* mrs, const struct mrs_file_t f){
    unsigned i = mrs->_hdr.dir_count;
    mrs->_files = (struct mrs_file_t*)realloc(mrs->_files, (mrs->_hdr.dir_count + 1)*sizeof(struct mrs_file_t));

    memcpy(&mrs->_files[i], &f, sizeof(struct mrs_file_t));

    mrs->_hdr.dir_count++;
    mrs->_hdr.total_dir_count = mrs->_hdr.dir_count;
}

off_t _mrs_temp_tell(MRS* mrs){
    return (mrs->_mtype == MRSMT_TEMPFILE ? ftell(mrs->_fbuf) : mrs->_mbuf_size);
}

int _mrs_temp_write(MRS* mrs, unsigned char* buf, size_t size){
    if(mrs->_mtype == MRSMT_TEMPFILE){
        fseek(mrs->_fbuf, 0, SEEK_END);
        dbgprintf("Writing %u bytes to temporary file", size);
        fwrite(buf, size, 1, mrs->_fbuf);
    }else{
        dbgprintf("Writing %u bytes to memory", size);
        mrs->_mbuf = (unsigned char*)realloc(mrs->_mbuf, mrs->_mbuf_size + size);
        memcpy(mrs->_mbuf + mrs->_mbuf_size, buf, size);
        mrs->_mbuf_size += size;
    }
    return 1;
}

int _mrs_temp_read(MRS* mrs, unsigned char* buf, off_t offset, size_t size){
    off_t _off;
    if(mrs->_mtype == MRSMT_TEMPFILE){
        _off = ftell(mrs->_fbuf);
        if(offset >= _off)
            return 0;
        fseek(mrs->_fbuf, offset, SEEK_SET);
        fread(buf, size, 1, mrs->_fbuf);
        fseek(mrs->_fbuf, 0, SEEK_END);
    }else{
        if(offset >= mrs->_mbuf_size)
            return 0;
        memcpy(buf, mrs->_mbuf + offset, size);
    }
    return 1;
}

int _mrs_replace_file(MRS* mrs, struct mrs_file_t* oldf, struct mrs_file_t* newf){
    if(!mrs)
        return MRSE_UNITIALIZED;
    if(!oldf || !newf)
        return MRSE_INVALID_PARAM;
    
    _mrs_file_free(mrs, oldf);
    memcpy(oldf, newf, sizeof(struct mrs_file_t));

    return MRSE_OK;
}

const char* mrs_error_str[] = {
    "Ok",
    "Unitialized MRS handle.",
    "Invalid parameter.",
    "File not found.",
    "Cannot open file.",
    "Invalid output file name.",
    "Duplicate file found.",
    "Out of bound index.",
    "Insufficient memory.",
    "Empty folder.",
    "Invalid MRS file or invalid encryption.",
    "Invalid MRS encryption.",
    "Given info not available or set.",
    "Cannot save file.",
    "Empty MRS file.",
    "No more files.",
    "Cannot uncompress file."
};