/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _MSC_BUILD
#include <fcntl.h>
#else
#include <sys/fcntl.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#endif

#ifdef __unix__
#include <unistd.h>
#else
#include <io.h>
#endif

#include "mrs.h"
#include "mrs_error.h"
#include "dostime.h"
#include "zlib.h"
#include "zconf.h"

extern void _hex_dump(const unsigned char*, size_t);
extern int _get_fnum(const char*, unsigned*, char**);
extern int _has_invalid_character(const char*);
extern int _has_invalid_dir_or_filename(const char*);
extern int _is_valid_input_filename(const char*);
extern int _is_valid_output_filename(const char*);
extern int _strbkslash(char*, size_t);
extern int _strslash(char*, size_t);
extern int _compress_file(unsigned char*, size_t, unsigned char**, size_t*);
extern int _uncompress_file(unsigned char*, size_t, unsigned char*, size_t, size_t*);
extern int _mkdirs(const char*);

#ifdef _LIBMRS_DBG
#define dbgprintf(...) printf("%s: ", __FUNCTION__); \
                       printf(__VA_ARGS__); \
                       printf("\n")
#else
#define dbgprintf(...)
#endif

/**< Uses a temp file for temporary storage */
#define MRSMT_TEMPFILE 0
/**< Uses memory for temporary storage */
#define MRSMT_MEMORY   1

/**< STORE compression method */
#define MRSCM_STORE   0
/**< DEFLATE compression method */
#define MRSCM_DEFLATE 8

#pragma pack(2)
#define MRSM_MAGIC1 0x5030207
#define MRSM_MAGIC2 0x5030208
#define MRSM_MAGIC3 0x6054b50
/**< Base header */
struct mrs_hdr_t{
    uint32_t signature;
    uint16_t disk_num;
    uint16_t disk_start;
    uint16_t dir_count;
    uint16_t total_dir_count;
    uint32_t dir_size;
    uint32_t dir_offset;
    uint16_t comment_length;
};

#define MRSM_LOCAL_MAGIC1 0x4034b50
#define MRSM_LOCAL_MAGIC2 0x85840000
#define MRSV_LOCAL        0x14
/**< Local header */
struct mrs_local_hdr_t{
    uint32_t signature;
    uint16_t version;
    uint16_t flags;
    uint16_t compression;
    struct dostime_t filetime;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_length;
    uint16_t extra_length;
};

#define MRSM_CDIR_MAGIC1 0x2014b50
#define MRSM_CDIR_MAGIC2 0x5024b80
#define MRSV_CDIR_MADE   0x19
#define MRSV_CDIR_NEEDED 0x14
/**< Central Dir header */
struct mrs_central_dir_hdr_t{
    uint32_t signature;
    uint16_t version_made;
    uint16_t version_needed;
    uint16_t flags;
    uint16_t compression;
    struct dostime_t filetime;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_length;
    uint16_t extra_length;
    uint16_t comment_length;
    uint16_t disk_start;
    uint16_t int_attr;
    uint32_t ext_attr;
    uint32_t offset;
};

/**< Extended Local header, containing filename and extra */
struct mrs_local_hdr_ex_t{
    struct mrs_local_hdr_t h;
    char* filename;
    char* extra;
};

/**< Extended Central Dir header, containing filename, extra and comment */
struct mrs_central_dir_hdr_ex_t{
    struct mrs_central_dir_hdr_t h;
    char* filename;
    char* extra;
    char* comment;
};
#pragma pack()

struct mrs_ref_t {
    unsigned char* mem;
    size_t         len;
    unsigned       ref;
};

struct mrs_ref_table_t {
    struct mrs_ref_t* refs;
    size_t count;
};

struct mrs_afile_internal_t{
    const char* name; /**< File name */
    uint32_t crc32;   /**< Checksum */
    size_t size;      /**< Uncompressed file size */
    size_t csize;     /**< Compressed file size */
    time_t ftime;     /**< File modification time */
    unsigned char* obuf;
    unsigned char* buf;
    unsigned i;
    unsigned cnt;
    MRS_SIGNATURE_FUNC sig;
    void* ptr;
};

struct mrs_file_t{
    struct mrs_central_dir_hdr_ex_t dh;
    struct mrs_local_hdr_ex_t lh;
};

struct mrs_t{
    struct mrs_encryption_t _dec;
    struct mrs_encryption_t _enc;
    MRS_SIGNATURE_FUNC      _sig;
    uint32_t _sigs[3]; /**< 0 being BASE HEADER, 1 being LOCAL HEADER, 2 being CENTRAL DIR HEADER */
    struct mrs_hdr_t _hdr;
    struct mrs_ref_table_t _reftable;
    struct mrs_file_t* _files;
    union{
        FILE* _fbuf;
        unsigned char* _mbuf;
    };
    int _mtype;
    size_t _mbuf_size;
    void* _ptr;
};

/*******************************
    Dump helper functions
*******************************/

#ifdef _LIBMRS_DBG
void mrs_central_dir_hdr_dump(const struct mrs_central_dir_hdr_t* h){
    dbgprintf("Signature...: %08x", h->signature);
    dbgprintf("Ver made....: %#02x", h->version_made);
    dbgprintf("Ver need....: %#02x", h->version_needed);
    dbgprintf("Flags.......: %#04x", h->flags);
    dbgprintf("Compression.: %x", h->compression);
    dbgprintf("File time...: %#04x", h->filetime.time);
    dbgprintf("File date...: %#04x", h->filetime.date);
    dbgprintf("CRC32.......: %#08x", h->crc32);
    dbgprintf("C.size......: %u", h->compressed_size);
    dbgprintf("U.size......: %u", h->uncompressed_size);
    dbgprintf("Filename len: %u", h->filename_length);
    dbgprintf("Extra len...: %u", h->extra_length);
    dbgprintf("Comment len.: %u", h->comment_length);
    dbgprintf("Disk start..: %u", h->disk_start);
    dbgprintf("Int.attr....: %u", h->int_attr);
    dbgprintf("Ext.attr....: %u", h->ext_attr);
    dbgprintf("Offset......: %#08x", h->offset);
}

void mrs_central_dir_hdr_ex_dump(const struct mrs_central_dir_hdr_ex_t* h){
    mrs_central_dir_hdr_dump((struct mrs_central_dir_hdr_t*)h);
    dbgprintf("Filename....: %.*s {%p}", h->h.filename_length, h->filename, h->filename);
    dbgprintf("Extra.......: %.*s {%p}", h->h.extra_length, h->extra, h->extra);
    dbgprintf("Comment.....: %.*s {%p}", h->h.comment_length, h->comment, h->comment);
}

void mrs_local_hdr_dump(const struct mrs_local_hdr_t* h){
    dbgprintf("Signature...: %08x", h->signature);
    dbgprintf("Version.....: %#02x", h->version);
    dbgprintf("Flags.......: %#04x", h->flags);
    dbgprintf("Compression.: %#04x", h->compression);
    dbgprintf("File time...: %#04x", h->filetime.time);
    dbgprintf("File date...: %#04x", h->filetime.date);
    dbgprintf("CRC32.......: %#08x", h->crc32);
    dbgprintf("C.Size......: %u", h->compressed_size);
    dbgprintf("U.Size......: %u", h->uncompressed_size);
    dbgprintf("Filename len: %u", h->filename_length);
    dbgprintf("Extra len...: %u", h->extra_length);
}

void mrs_local_hdr_ex_dump(const struct mrs_local_hdr_ex_t* h){
    mrs_local_hdr_dump((struct mrs_local_hdr_t*)h);
    dbgprintf("Filename....: %.*s {%p}", h->h.filename_length, h->filename, h->filename);
    dbgprintf("Extra.......: %.*s {%p}", h->h.extra_length, h->extra, h->extra);
}

void mrs_file_dump(const struct mrs_file_t* f){
    mrs_central_dir_hdr_ex_dump(&f->dh);
    mrs_local_hdr_ex_dump(&f->lh);
}
#endif

/*******************************
    Internal MRS functions
*******************************/

void _mrs_ref_table_init(struct mrs_ref_table_t* r) {
    r->refs  = NULL;
    r->count = 0;
}

unsigned char* _mrs_ref_table_append(struct mrs_ref_table_t* r, const unsigned char* s, size_t len) {
    unsigned i;
    struct mrs_ref_t* cur;

    if (!r || !s)
        return NULL;

#ifdef _LIBMRS_DBG
    dbgprintf("Looking for: ");
    _hex_dump(s, len);
#endif
    
    for (i = 0; i < r->count; i++) {
        cur = &r->refs[i];
        if (cur->len == len && !memcmp(cur->mem, s, len)) {
            dbgprintf("Found! Incrementing ref");
            cur->ref++;
            return cur->mem;
        }
    }

    dbgprintf("Not found, adding a new byte stream to the ref table");

    r->refs = (struct mrs_ref_t*)realloc(r->refs, sizeof(struct mrs_ref_t) * (r->count + 1));
    cur = &r->refs[r->count];
    cur->ref = 1;
    cur->mem = (unsigned char*)malloc(len);
    memcpy(cur->mem, s, len);
    cur->len = len;
    r->count++;

    return cur->mem;
}

int _mrs_ref_table_free(struct mrs_ref_table_t* r, unsigned char* s) {
    unsigned i;
    struct mrs_ref_t* cur;

    if (!r || !s)
        return NULL;

    for (i = 0; i < r->count; i++) {
        cur = &r->refs[i];

        if (cur->mem == s){
            if (cur->ref > 1) {
                dbgprintf("More than 1 reference to %p found, decrementing it...", cur->mem);
                cur->ref--;
                return 0;
            }
            dbgprintf("No more references to %p, freeing it", cur->mem);
            free(cur->mem);
            cur->mem = NULL;
            cur->ref = 0;
            cur->len = 0;
            if (i + 1 < r->count)
                memmove(&r->refs[i], &r->refs[i + 1], (r->count - i - 1) * sizeof(struct mrs_ref_t));

            if (r->count > 1) 
                r->refs = (struct mrs_ref_t*)realloc(r->refs, sizeof(struct mrs_ref_t) * (r->count - 1));
            else {
                free(r->refs);
                r->refs = NULL;
            }

            r->count--;
            return 0;
        }
    }

    return 1;
}

void _mrs_ref_table_free_all(struct mrs_ref_table_t* r) {
    unsigned i;
    struct mrs_ref_t* cur;

    for (i = 0; i < r->count; i++) {
        cur = &r->refs[i];
        free(cur->mem);
        cur->mem = NULL;
        cur->ref = 0;
        cur->len = 0;
    }

    free(r->refs);
    r->refs  = NULL;
    r->count = 0;
}

void _mrs_file_free(MRS* mrs, struct mrs_file_t* f) {
    if (f->lh.filename != f->dh.filename) {
        dbgprintf("We got different filenames between LOCAL and CENTRAL DIR headers");
        free(f->lh.filename);
        dbgprintf("Freed LOCAL filename");
    }
    free(f->dh.filename);
    dbgprintf("Freed CENTRAL DIR filename");
    //_mrs_ref_table_free(&mrs->_reftable, f->lh.extra);
    //dbgprintf("Freed LOCAL extra");
    /*if (f->lh.extra != f->dh.extra) {
        dbgprintf("We got different extra between LOCAL and CENTRAL DIR headers");
        free(f->lh.extra);
        dbgprintf("Freed LOCAL extra");
    }*/
    free(f->dh.extra);
    dbgprintf("Freed CENTRAL DIR extra");
    free(f->dh.comment);
    dbgprintf("Freed CENTRAL DIR comment");
}

int _mrs_is_duplicate(const MRS* mrs, const char* s, char** s_out){
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
  
  if (n || match) {
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

int _mrs_is_initialized(const MRS* mrs){
  if(!mrs || !mrs->_ptr)
    return 0;
  return 1;
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

int _mrs_add_file(MRS* mrs, const char* name, char* final_name, enum mrs_dupe_behavior_t on_dupe){
    int fd;
    struct stat fs;
    struct mrs_file_t f;
    unsigned char* ubuf = NULL;
    unsigned char* cbuf = NULL;
    char* temp = NULL;
    int e;
    unsigned dup = 0, dup_index = 0;

    dbgprintf("Adding a file");
    
    if(!_mrs_is_initialized(mrs)){
      dbgprintf("mrs handle unitialized");
      return MRSE_UNITIALIZED;
    }

    if (!final_name) {
        final_name = (char*)malloc(strlen(name) + 1);
        strcpy(final_name, name);
        temp = strrchr(final_name, '/');
        if (temp)
            strcpy(final_name, temp + 1);
    }
    else {
        temp = (char*)malloc(strlen(final_name) + 1);
        strcpy(temp, final_name);
        final_name = temp;
    }

    _strslash(final_name, 0);

    if(_is_valid_input_filename(final_name)){
        dbgprintf("Invalid final filename");
        return MRSE_INVALID_FILENAME;
    }

    ///--TODO--: Check if there is a duplicate, based on final_name, and perform the specified action in on_dupe
    if(mrs->_hdr.dir_count){
        if(on_dupe == MRSDB_KEEP_NEW){
            for(e=0; e<mrs->_hdr.dir_count; e++){
                ///TODO: make stricmp portable
                if(!stricmp(final_name, mrs->_files[e].dh.filename)){
                    dup = 1;
                    dup_index = e;
                    break;
                }
            }
        }else{
            dup = _mrs_is_duplicate(mrs, final_name, &temp);
            if(!dup){
                if(on_dupe == MRSDB_KEEP_OLD){
                    dbgprintf("Duplicate file, leaving...");
                    free(final_name);
                    return MRSE_DUPLICATE;
                }else if(on_dupe == MRSDB_KEEP_BOTH){
                    dbgprintf("Freeing old final_name...");
                    free(final_name);
                    dbgprintf("Freed");
                    final_name = temp;
                    temp = NULL;
                }
            }
        }
    }

    ///TODO: Is O_BINARY portable ?
    fd = open(name, O_RDONLY|O_BINARY);
    if(fd == -1){
        free(temp);
        free(final_name);
        dbgprintf("%s: file not found", name);
        return MRSE_NOT_FOUND;
    }

    if(fstat(fd, &fs) != 0){
        free(temp);
        free(final_name);
        close(fd);
        return MRSE_CANNOT_OPEN;
    }

    dbgprintf("Got fstat");
    dbgprintf("Total size: %u", fs.st_size);

    memset(&f, 0, sizeof(struct mrs_file_t));
    f.dh.h.signature         = MRSM_CDIR_MAGIC1;                            // dir header signature
    f.lh.h.signature         = MRSM_LOCAL_MAGIC1;                           // local header signature
    f.dh.h.version_made      = MRSV_CDIR_MADE;                              // dir header version made
    f.lh.h.version           = MRSV_LOCAL;                                  // local header version
    f.dh.h.version_needed    = MRSV_CDIR_NEEDED;                            // dir header version needed
    f.dh.h.compression       = MRSCM_DEFLATE;                               // dir header compression method
    f.lh.h.uncompressed_size = f.dh.h.uncompressed_size = fs.st_size;       // dir and local header uncompressed file size
    f.lh.h.uncompressed_size = f.dh.h.uncompressed_size = fs.st_size;       // dir and local header uncompressed file size
    f.lh.h.filename_length   = f.dh.h.filename_length = strlen(final_name); // dir and local header filename length

    f.dh.h.filetime = dostime(&fs.st_mtime);    // dir header file modification time
    dbgprintf("Modification time: %02u:%02u:%02u %02u/%02u/%04u", f.dh.h.filetime.hour, f.dh.h.filetime.minute, f.dh.h.filetime.second*2, f.dh.h.filetime.day, f.dh.h.filetime.month, f.dh.h.filetime.year+1980);
    f.lh.h.filetime = f.dh.h.filetime;          // local header file modification time

    f.dh.h.crc32 = crc32(0L, Z_NULL, 0);
    if(f.dh.h.uncompressed_size){
        ubuf = (unsigned char*)malloc(f.dh.h.uncompressed_size);
        read(fd, ubuf, f.dh.h.uncompressed_size);
        dbgprintf("Dump: %.*s", f.dh.h.uncompressed_size, ubuf);
        f.dh.h.crc32 = crc32(f.dh.h.crc32, ubuf, f.dh.h.uncompressed_size); // dir header file crc32 checksum
    }
    f.lh.h.crc32 = f.dh.h.crc32;                                            // local header file crc32 checksum

    close(fd);
    
    if(f.dh.h.uncompressed_size){
        e = _compress_file(ubuf, f.dh.h.uncompressed_size, &cbuf, &f.dh.h.compressed_size); // sets dir header compressed file size
        if(!e){
            ubuf = NULL;
            cbuf = ubuf;
            f.dh.h.compressed_size = f.dh.h.uncompressed_size;  // dir header compressed file size
            f.dh.h.compression = MRSCM_STORE;                   // dir header compression method
        }
    }else{
        f.dh.h.compressed_size = f.dh.h.uncompressed_size;
        f.dh.h.compression = MRSCM_STORE;
    }

    f.lh.h.compressed_size = f.dh.h.compressed_size;    // local header compressed file size
    f.lh.h.compression     = f.dh.h.compression;        // local header compression method
    
    if(ubuf)
        free(ubuf);

    f.lh.filename = f.dh.filename = final_name; // dir and local header filename
    f.dh.h.offset = _mrs_temp_tell(mrs);        // dir header file offset
    dbgprintf("Current temp offset: %u", _mrs_temp_tell(mrs));
    _mrs_temp_write(mrs, cbuf, f.dh.h.compressed_size);
    dbgprintf("Offset is now: %u", _mrs_temp_tell(mrs));

    free(cbuf);

    ///TODO: delete the old file (base on dup_index value)
    if(dup && on_dupe == MRSDB_KEEP_NEW){
        dbgprintf("There was an old file with the same name, we are replacing it!");
        if(mrs->_files[dup_index].lh.filename != mrs->_files[dup_index].dh.filename)
            free(mrs->_files[dup_index].lh.filename);
        if(mrs->_files[dup_index].lh.extra != mrs->_files[dup_index].dh.extra)
            free(mrs->_files[dup_index].lh.extra);
        free(mrs->_files[dup_index].dh.filename);
        free(mrs->_files[dup_index].dh.extra);
        free(mrs->_files[dup_index].dh.comment);
        // copy memory from f to replace it
        memcpy(&mrs->_files[dup_index], &f, sizeof(struct mrs_file_t));
    }else{
        _mrs_push_file(mrs, f);
    }

#ifdef _LIBMRS_DBG
    mrs_file_dump(&f);
#endif

    return MRSE_OK;
}

///TODO: Make it portable
int _mrs_add_folder(MRS* mrs, const char* name, char* final_name, enum mrs_dupe_behavior_t on_dupe){
    WIN32_FIND_DATAA fda;
    HANDLE h;
    char** path_list = NULL;
    size_t path_list_size = 1;
    unsigned i = 0;
    char path[256];
    char temp[256];
    
    if(!PathFileExistsA(name))
        return MRSE_NOT_FOUND;

    if(final_name){
        if(_is_valid_input_filename(final_name))
            return MRSE_INVALID_FILENAME;
    }

    path_list = (char**)malloc(sizeof(char*));
    path_list[0] = (char*)malloc(256);
    sprintf(path_list[0], "%s\\*", name);

    while(i < path_list_size){
        h = FindFirstFileA(path_list[i], &fda);
        if(h == INVALID_HANDLE_VALUE){
            if(i == 0){
                free(path_list[0]);
                free(path_list);
                return MRSE_EMPTY_FOLDER;
            }
            continue;
        }
        do{
            if(!strcmp(fda.cFileName, ".") || !strcmp(fda.cFileName, ".."))
                continue;
            if(fda.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY){
                dbgprintf("  (DIRECTORY)");
                path_list = (char**)realloc(path_list, sizeof(char*) * (path_list_size+1));
                path_list[path_list_size] = (char*)malloc(256);
                sprintf(path_list[path_list_size], "%.*s\\%s\\*", strlen(path_list[i]) - 2, path_list[i], fda.cFileName);
                dbgprintf("   > %s", path_list[path_list_size]);
                path_list_size++;
                continue;
            }
            if(i)
                sprintf(temp, "%s%s%.*s%s", final_name ? final_name : "", final_name ? "\\" : "", strlen(path_list[i]+strlen(name)+1) - 1, path_list[i]+strlen(name)+1, fda.cFileName);
            else
                sprintf(temp, "%s%s%s", final_name ? final_name : "", final_name ? "\\" : "", fda.cFileName);
            sprintf(path, "%.*s%s", strlen(path_list[i])-1, path_list[i], fda.cFileName);
            dbgprintf("<%s>{%s}", temp, path);
            //temp being the final name
            //path being the real file path
            _mrs_add_file(mrs, path, temp, on_dupe);
        }while(FindNextFileA(h, &fda));
        i++;
    }

    for(i=0;i<path_list_size;i++)
        free(path_list[i]);
    free(path_list);

    return MRSE_OK;
}

int _mrs_add_mrs(MRS* mrs, const char* name, char* final_name, enum mrs_dupe_behavior_t on_dupe) {
    FILE* f;
    unsigned i;
    struct mrs_hdr_t hdr;
    struct mrs_encryption_t decrypt;
    unsigned char* dhbuf;
    unsigned char* temp;
    unsigned char* temp2;
    struct mrs_file_t* mrsfile;

    if(final_name){
        if(_is_valid_input_filename(final_name))
            return MRSE_INVALID_FILENAME;
    }

    dbgprintf("Ok, let's try to read a mrs file: %s", name);

    f = fopen(name, "rb");
    if (!f) {
        dbgprintf("\"%s\" not found", name);
        return MRSE_NOT_FOUND;
    }

    decrypt.base_hdr = mrs->_dec.base_hdr ? mrs->_dec.base_hdr : mrs_default_decrypt;
    decrypt.local_hdr = mrs->_dec.local_hdr ? mrs->_dec.local_hdr : decrypt.base_hdr;
    decrypt.central_dir_hdr = mrs->_dec.central_dir_hdr ? mrs->_dec.central_dir_hdr : decrypt.base_hdr;
    decrypt.buffer = mrs->_dec.buffer;

    fseek(f, -(int)sizeof(struct mrs_hdr_t), SEEK_END);
    fread(&hdr, sizeof(struct mrs_hdr_t), 1, f);

    decrypt.base_hdr((unsigned char*)&hdr, sizeof(struct mrs_hdr_t));
    dbgprintf("SIGNATURE: %08x, IS IT VALID?", hdr.signature);
   
    if (!mrs_default_signatures(MRSSW_BASE_HDR, hdr.signature) && (!mrs->_sig || !mrs->_sig(MRSSW_BASE_HDR, hdr.signature))){
        dbgprintf("  Invalid signature!");
        fclose(f);
        return MRSE_INVALID_MRS;
    }

    dbgprintf("We got %u file(s)", hdr.dir_count);
    
    dhbuf = (char*)malloc(hdr.dir_size);
    fseek(f, hdr.dir_offset, SEEK_SET);
    fread(dhbuf, hdr.dir_size, 1, f);
    decrypt.central_dir_hdr(dhbuf, hdr.dir_size);
    
    mrsfile = (struct mrs_file_t*)malloc(sizeof(struct mrs_file_t) * hdr.dir_count);
    memset(mrsfile, 0, sizeof(struct mrs_file_t) * hdr.dir_count);

    temp = dhbuf;
    for (i = 0; i < hdr.dir_count; i++) {
        // Reading dir header
        memcpy(&mrsfile[i].dh.h, temp, sizeof(struct mrs_central_dir_hdr_t));
        dbgprintf("Got dir header with signature %08x, is it valid?", mrsfile[i].dh.h.signature);
        // Checking dir header signature
        if (!mrs_default_signatures(MRSSW_CENTRAL_DIR_HDR, mrsfile[i].dh.h.signature) && (!mrs->_sig || !mrs->_sig(MRSSW_CENTRAL_DIR_HDR, mrsfile[i].dh.h.signature))) {
            dbgprintf("Invalid encryption");
            for (i = 0; i < hdr.dir_count; i++)
                _mrs_file_free(mrs, &mrsfile[i]);
            free(mrsfile);
            free(dhbuf);
            fclose(f);
            return MRSE_INVALID_ENCRYPTION;
        }

        // Reading local header
        fseek(f, mrsfile[i].dh.h.offset, SEEK_SET);
        fread(&mrsfile[i].lh.h, sizeof(struct mrs_local_hdr_t), 1, f);
        decrypt.local_hdr((unsigned char*)&mrsfile[i].lh.h, sizeof(struct mrs_local_hdr_t));
        dbgprintf("Got local header with signature %08x, is it valid?", mrsfile[i].lh.h.signature);
#ifdef _LIBMRS_DBG
        mrs_local_hdr_dump(&mrsfile[i].lh.h);
#endif
        // Checking local header signature
        if (!mrs_default_signatures(MRSSW_LOCAL_HDR, mrsfile[i].lh.h.signature) && (!mrs->_sig || !mrs->_sig(MRSSW_LOCAL_HDR, mrsfile[i].lh.h.signature))) {
            dbgprintf("Invalid local header encryption");
            for (i = 0; i < hdr.dir_count; i++)
                _mrs_file_free(mrs, &mrsfile[i]);
            free(mrsfile);
            free(dhbuf);
            fclose(f);
            return MRSE_INVALID_ENCRYPTION;
        }

        fseek(f, mrsfile[i].lh.h.filename_length, SEEK_CUR);

        //// Reading Local Header "extra" field (if any)
        if (mrsfile[i].lh.h.extra_length) {
            dbgprintf("We have Local extra, let's copy it");
            mrsfile[i].lh.extra = (char*)malloc(mrsfile[i].lh.h.extra_length);
            fread(mrsfile[i].lh.extra, mrsfile[i].lh.h.extra_length, 1, f);
            decrypt.local_hdr(mrsfile[i].lh.extra, mrsfile[i].lh.h.extra_length);
            temp2 = mrsfile[i].lh.extra;
            mrsfile[i].lh.extra = _mrs_ref_table_append(&mrs->_reftable, temp2, mrsfile[i].lh.h.extra_length);
            free(temp2);
            dbgprintf("Read local header extra:");
        }
        else
            fseek(f, mrsfile[i].lh.h.extra_length, SEEK_CUR);
        
        // We update our offset to the beginning of the file buffer
        mrsfile[i].dh.h.offset = ftell(f);

        temp += sizeof(struct mrs_central_dir_hdr_t);

        //// Reading filename
        if(final_name){
            mrsfile[i].dh.filename = (char*)malloc(strlen(final_name) + mrsfile[i].dh.h.filename_length + 2);
            memset(mrsfile[i].dh.filename, 0, strlen(final_name) + mrsfile[i].dh.h.filename_length + 2);
            sprintf(mrsfile[i].dh.filename, "%s/", final_name);
        }else{
            mrsfile[i].dh.filename = (char*)malloc(mrsfile[i].dh.h.filename_length + 1);
            memset(mrsfile[i].dh.filename, 0, mrsfile[i].dh.h.filename_length + 1);
        }
        strncat(mrsfile[i].dh.filename, temp, mrsfile[i].dh.h.filename_length);
        mrsfile[i].lh.filename = mrsfile[i].dh.filename;
        temp += mrsfile[i].dh.h.filename_length;

        //// Reading Central Dir Header "extra" field (if any)
        if (mrsfile[i].dh.h.extra_length) {
            dbgprintf("We have Central Dir extra, let's copy it");
            mrsfile[i].dh.extra = (char*)malloc(mrsfile[i].dh.h.extra_length);
            memcpy(mrsfile[i].dh.extra, temp, mrsfile[i].dh.h.extra_length);
        }
        temp += mrsfile[i].dh.h.extra_length;

        //// Reading Central Dir Header "comment" field (if any)
        if (mrsfile[i].dh.h.comment_length) {
            dbgprintf("We have Central Dir comment, let's copy it");
            mrsfile[i].dh.comment = (char*)malloc(mrsfile[i].dh.h.comment_length);
            memcpy(mrsfile[i].dh.comment, temp, mrsfile[i].dh.h.comment_length);
        }
        temp += mrsfile[i].dh.h.comment_length;
    }
    free(dhbuf);
    
    ///TODO:Now let's read each file content and append to our MRS handle
    for (i = 0; i < hdr.dir_count; i++) {
        dbgprintf("File %u is at offset %08x", i, mrsfile[i].dh.h.offset);
        temp = (char*)malloc(mrsfile[i].dh.h.compressed_size);
        fseek(f, mrsfile[i].dh.h.offset, SEEK_SET);
        fread(temp, mrsfile[i].dh.h.compressed_size, 1, f);
        // Decrypt the file buffer, if there's a decryption routine for it
        if (decrypt.buffer)
            decrypt.buffer(temp, mrsfile[i].dh.h.compressed_size);
        // We finally update our offset to the real offset in our temporary storage
        mrsfile[i].dh.h.offset = _mrs_temp_tell(mrs);
        dbgprintf("File dump:");
        _mrs_temp_write(mrs, temp, mrsfile[i].dh.h.compressed_size);
        _strslash(mrsfile[i].dh.filename, 0);
        _mrs_push_file(mrs, mrsfile[i]);
        free(temp);
    }

    free(mrsfile);

    fclose(f);

    return MRSE_OK;
}

#define MRS_SAVE_CALLBACK(...) if(pcallback) pcallback(__VA_ARGS__);
int _mrs_save_mrs(const MRS* mrs, const char* output, MRS_PROGRESS_FUNC pcallback){
    FILE* f;
    struct mrs_hdr_t hdr;
    char real_output[256];
    struct mrs_file_t* fil;
    unsigned char* temp;
    unsigned i, j;
    struct mrs_encryption_t encrypt;
    double p;

    dbgprintf("Ok let's save this as a MRS file.");

    ///TODO: Make it portable
    GetFullPathNameA(output, 256, real_output, NULL);

    if((PathFileExistsA(real_output) && PathIsDirectoryA(real_output)) || _is_valid_output_filename(real_output))
        return MRSE_INVALID_FILENAME;

    encrypt.base_hdr  = mrs->_enc.base_hdr ? mrs->_enc.base_hdr : mrs_default_encrypt;
    encrypt.local_hdr = mrs->_enc.local_hdr ? mrs->_enc.local_hdr : encrypt.base_hdr;
    encrypt.central_dir_hdr = mrs->_enc.central_dir_hdr ? mrs->_enc.central_dir_hdr : encrypt.base_hdr;
    encrypt.buffer = mrs->_enc.buffer;
        
    memcpy(&hdr, &mrs->_hdr, sizeof(struct mrs_hdr_t));
    
    dbgprintf("We got %u files", hdr.dir_count);
    f = fopen(real_output, "wb");
    if(!f)
        return MRSE_CANNOT_SAVE;

    fil = (struct mrs_file_t*)malloc(sizeof(struct mrs_file_t) * hdr.dir_count);
    memcpy(fil, mrs->_files, sizeof(struct mrs_file_t) * hdr.dir_count);

    hdr.dir_size = 0;

    p = 0;
    for(i=0; i<hdr.dir_count; i++){
        p = (double)i / (double)hdr.dir_count;
        MRS_SAVE_CALLBACK(p, i+1, mrs->_hdr.dir_count, MRSP_BEGIN, mrs->_files[i].dh.filename);

        dbgprintf("%u/%u", i+1, hdr.dir_count);
        if(mrs->_sigs[1])
            fil[i].lh.h.signature = mrs->_sigs[1];
        
        // Here we write the local header
        encrypt.local_hdr((unsigned char*)&fil[i].lh.h, sizeof(struct mrs_local_hdr_t));
        _hex_dump((unsigned char*)&fil[i].lh.h, sizeof(struct mrs_local_hdr_t));
        fil[i].dh.h.offset = ftell(f);
        fwrite((unsigned char*)&fil[i].lh.h, sizeof(struct mrs_local_hdr_t), 1, f);
		dbgprintf("Encrypt local header\n");

        // Here we write the filename
        temp = (unsigned char*)malloc(mrs->_files[i].lh.h.filename_length);
        memcpy(temp, fil[i].lh.filename, mrs->_files[i].lh.h.filename_length);
        _strbkslash(temp, mrs->_files[i].lh.h.filename_length);
        encrypt.local_hdr(temp, mrs->_files[i].lh.h.filename_length);
        fwrite(temp, mrs->_files[i].lh.h.filename_length, 1, f);
        free(temp);
		dbgprintf("File name\n");

        // Here we write extra, if any
        if(mrs->_files[i].lh.h.extra_length){
            temp = (unsigned char*)malloc(mrs->_files[i].lh.h.extra_length);
            memcpy(temp, fil[i].lh.extra, mrs->_files[i].lh.h.extra_length);
            encrypt.local_hdr(temp, mrs->_files[i].lh.h.extra_length);
            fwrite(temp, mrs->_files[i].lh.h.extra_length, 1, f);
            free(temp);
        }

        // And finally the file buffer
        if(mrs->_files[i].lh.h.uncompressed_size){
            temp = (unsigned char*)malloc(mrs->_files[i].lh.h.compressed_size);
            _mrs_temp_read(mrs, temp, mrs->_files[i].dh.h.offset, mrs->_files[i].lh.h.compressed_size);
            if(encrypt.buffer)
                encrypt.buffer(temp, mrs->_files[i].lh.h.compressed_size);
            fwrite(temp, mrs->_files[i].lh.h.compressed_size, 1, f);
            free(temp);
        }

        MRS_SAVE_CALLBACK(p, i+1, mrs->_hdr.dir_count, MRSP_END, mrs->_files[i].dh.filename);

        hdr.dir_size += sizeof(struct mrs_central_dir_hdr_t) + fil[i].dh.h.filename_length + fil[i].dh.h.extra_length + fil[i].dh.h.comment_length;
    }

    temp = (unsigned char*)malloc(hdr.dir_size);
    j = 0;

    hdr.dir_offset = ftell(f);
    for(i=0; i<hdr.dir_count; i++){
        if(mrs->_sigs[2])
            fil[i].dh.h.signature = mrs->_sigs[2];
        
        memcpy(temp + j, &fil[i].dh.h, sizeof(struct mrs_central_dir_hdr_t));
        j += sizeof(struct mrs_central_dir_hdr_t);

        memcpy(temp + j, fil[i].dh.filename, fil[i].dh.h.filename_length);
        _strbkslash(temp + j, fil[i].dh.h.filename_length);
        j += fil[i].dh.h.filename_length;

        if(fil[i].dh.h.extra_length){
            memcpy(temp + j, fil[i].dh.extra, fil[i].dh.h.extra_length);
            j += fil[i].dh.h.extra_length;
        }

        if(fil[i].dh.h.comment_length){
            memcpy(temp + j, fil[i].dh.comment, fil[i].dh.h.comment_length);
            j += fil[i].dh.h.comment_length;
        }
    }

    free(fil);

    encrypt.central_dir_hdr(temp, hdr.dir_size);
    fwrite(temp, hdr.dir_size, 1, f);
    free(temp);

    hdr.signature = mrs->_sigs[0] ? mrs->_sigs[0] : MRSM_MAGIC2;

    encrypt.base_hdr((unsigned char*)&hdr, sizeof(struct mrs_hdr_t));
    fwrite((unsigned char*)&hdr, sizeof(struct mrs_hdr_t), 1, f);

    fclose(f);

    MRS_SAVE_CALLBACK(1.f, mrs->_hdr.dir_count, mrs->_hdr.dir_count, MRSP_DONE, NULL);

    return MRSE_OK;
}

int _mrs_save_folder(MRS* mrs, const char* output, MRS_PROGRESS_FUNC pcallback){
    char real_output[256];
    unsigned char* buf;
    char fname[256];
    char temp[256];
    unsigned i;
	int e;
    size_t s;
    HANDLE f;
    FILETIME ftime;
    double p;
    
    dbgprintf("Ok, let's save this as a folder");

    GetFullPathNameA(output, 256, real_output, NULL);

    if((PathFileExistsA(real_output) && !PathIsDirectoryA(real_output)) || _is_valid_output_filename(real_output))
        return MRSE_INVALID_FILENAME;
    
    _mkdirs(real_output);

    dbgprintf("We got %u files to extract", mrs->_hdr.dir_count);

    p = 0;
    for(i=0; i<mrs->_hdr.dir_count; i++){
        if(i)
            p = (double)i / (double)mrs->_hdr.dir_count;
        strcpy(fname, mrs->_files[i].dh.filename);
        _strbkslash(fname, 0);
        MRS_SAVE_CALLBACK(p, i+1, mrs->_hdr.dir_count, MRSP_BEGIN, fname);
        if(fname[strlen(fname) - 1] == '\\'){ // It's a folder, so let's just skip it :D
            MRS_SAVE_CALLBACK(p, i+1, mrs->_hdr.dir_count, MRSP_END, fname);
            continue;
        }
        
        // If there's any subfolder, we make them
        if((buf = strrchr(fname, '\\'))){
            *buf = 0;
            sprintf(temp, "%s\\%s", real_output, fname);
            _mkdirs(temp);
            *buf = '\\';
        }

        sprintf(temp, "%s\\%s", real_output, fname);
        dbgprintf("  %s", temp);

        f = CreateFileA(temp, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if(f == INVALID_HANDLE_VALUE){
            ///TODO: throw an error to callback function
            dbgprintf("file @ %u: cant open", i);
            MRS_SAVE_CALLBACK(p, i+1, mrs->_hdr.dir_count, MRSP_ERROR, (void*)MRSE_CANNOT_OPEN);
            continue;
        }
        
        // There may be files with 0 bytes, so let's check it
        if(mrs->_files[i].dh.h.uncompressed_size == 0){
            CloseHandle(f);
            MRS_SAVE_CALLBACK(p, i+1, mrs->_hdr.dir_count, MRSP_END, fname);
            continue;
        }

        buf = (unsigned char*)malloc(mrs->_files[i].dh.h.uncompressed_size);
        e = mrs_read(mrs, i, buf, mrs->_files[i].dh.h.uncompressed_size, &s);
		if(e){
			CloseHandle(f);
			free(buf);
			MRS_SAVE_CALLBACK(p, i+1, mrs->_hdr.dir_count, MRSP_ERROR, (void*)MRSE_CANNOT_UNCOMPRESS);
			continue;
		}

        DosDateTimeToFileTime(mrs->_files[i].dh.h.filetime.date, mrs->_files[i].dh.h.filetime.time, &ftime);
        SetFileTime(f, NULL, NULL, &ftime);

        WriteFile(f, buf, s, NULL, NULL);

        CloseHandle(f);
		
		free(buf);
        
        MRS_SAVE_CALLBACK(p, i+1, mrs->_hdr.dir_count, MRSP_END, fname);
    }

    MRS_SAVE_CALLBACK(1.f, mrs->_hdr.dir_count, mrs->_hdr.dir_count, MRSP_DONE, NULL);

    return MRSE_OK;
}

/*******************************
   MRS functions
*******************************/

MRS* mrs_init(){
    MRS* mrs;

    mrs = (MRS*)malloc(sizeof(struct mrs_t));
    if(!mrs){
        dbgprintf("Could not allocate mrs handle");
        return NULL;
    }
    dbgprintf("Allocated mrs handle successfully");

    memset(mrs, 0, sizeof(struct mrs_t));
    mrs->_ptr = mrs;    
    mrs->_fbuf = tmpfile();
    _mrs_ref_table_init(&mrs->_reftable);
    if(!mrs->_fbuf){
        dbgprintf("Could not open temp file, let's use memory then");
        mrs->_mtype = MRSMT_MEMORY;
    }
    dbgprintf("mrs handle initialized, we good to go");

    return mrs;
}

int mrs_set_decryption(MRS* mrs, int where, MRS_ENCRYPTION_FUNC f){
    if(!_mrs_is_initialized(mrs)){
        dbgprintf("mrs handle apparently not initialized (NULL pointer)");
        return MRSE_UNITIALIZED;
    }

    if(where & MRSEW_BASE_HDR){
        dbgprintf("Setting base header decryption to %p", f);
        mrs->_dec.base_hdr = f;
    }

    if(where & MRSEW_LOCAL_HDR){
        dbgprintf("Setting local header decryption to %p", f);
        mrs->_dec.local_hdr = f;
    }

    if (where & MRSEW_CENTRAL_DIR_HDR){
        dbgprintf("Setting central dir header decryption to %p", f);
        mrs->_dec.central_dir_hdr = f;
    }
    
    if(where & MRSEW_BUFFER){
        dbgprintf("Setting file buffer decryption to %p", f);
        mrs->_dec.buffer = f;
    }
    
    return MRSE_OK;
}

int mrs_set_encryption(MRS* mrs, int where, MRS_ENCRYPTION_FUNC f){
    if(!_mrs_is_initialized(mrs)){
        dbgprintf("mrs handle apparently not initialized (NULL pointer)");
        return MRSE_UNITIALIZED;
    }

    if(where & MRSEW_BASE_HDR){
        dbgprintf("Setting base header encryption to %p", f);
        mrs->_enc.base_hdr = f;
    }

    if(where & MRSEW_LOCAL_HDR){
        dbgprintf("Setting local header encryption to %p", f);
        mrs->_enc.local_hdr = f;
    }

    if (where & MRSEW_CENTRAL_DIR_HDR){
        dbgprintf("Setting central dir header encryption to %p", f);
        mrs->_enc.central_dir_hdr = f;
    }

    if(where & MRSEW_BUFFER){
        dbgprintf("Setting file buffer encryption to %p", f);
        mrs->_enc.buffer = f;
    }
    
    return MRSE_OK;
}

int mrs_add(MRS* mrs, enum mrs_add_t what, const char* name, const char* final_name, enum mrs_dupe_behavior_t on_dupe){
    dbgprintf("Let's add something!");

    if(!_mrs_is_initialized(mrs))
        return MRSE_UNITIALIZED;
    
    if(!name)
        return MRSE_INVALID_PARAM;
    
    switch(what){
    case MRSA_FILE:
        return _mrs_add_file(mrs, name, final_name, on_dupe);
    case MRSA_FOLDER:
        return _mrs_add_folder(mrs, name, final_name ? final_name : NULL, on_dupe);
    case MRSA_MRS:
        return _mrs_add_mrs(mrs, name, final_name ? final_name : NULL, on_dupe);
    }

    return MRSE_OK;
}

int mrs_read(const MRS* mrs, unsigned index, unsigned char* buf, size_t buf_size, size_t* out_size){
    unsigned char* temp;
    struct mrs_file_t* f;
	int r = MRSE_OK;

    if(!_mrs_is_initialized(mrs))
        return MRSE_UNITIALIZED;

    if(index >= mrs->_hdr.dir_count){
        dbgprintf("Trying to read file @ index %u but max is %u, returning", index, mrs->_hdr.dir_count);
        return MRSE_INVALID_INDEX;
    }

    f = &mrs->_files[index];
    
    dbgprintf("Stored at %08x", f->dh.h.offset);
    
    if(f->dh.h.compression == MRSCM_STORE){
        if(out_size)
            *out_size = f->dh.h.uncompressed_size;
        if(buf_size < f->dh.h.uncompressed_size || !buf)
            return MRSE_INSUFFICIENT_MEM;
        _mrs_temp_read(mrs, buf, f->dh.h.offset, f->dh.h.uncompressed_size);
    }else{
        if(out_size)
            *out_size = f->dh.h.uncompressed_size;
        if(buf_size < f->dh.h.uncompressed_size || !buf)
            return MRSE_INSUFFICIENT_MEM;
        temp = (unsigned char*)malloc(mrs->_files[index].dh.h.compressed_size);
        _mrs_temp_read(mrs, temp, f->dh.h.offset, f->dh.h.compressed_size);
        r = _uncompress_file(temp, mrs->_files[index].dh.h.compressed_size, buf, mrs->_files[index].dh.h.uncompressed_size, out_size);
        if(r)
            r = MRSE_CANNOT_UNCOMPRESS;
        free(temp);
    }

    return r;
}

int mrs_get_file_info(const MRS* mrs, unsigned index, enum mrs_file_info_t what, void* buf, size_t buf_size, size_t* out_size){
    struct mrs_file_t* f;

    if(!_mrs_is_initialized(mrs))
        return MRSE_UNITIALIZED;

    if(index >= mrs->_hdr.dir_count){
        dbgprintf("Trying to get info from file %u but max is %u", index, mrs->_hdr.dir_count);
        return MRSE_INVALID_INDEX;
    }

    f = &mrs->_files[index];

    switch(what){
    case MRSFI_NAME:
        dbgprintf("Getting FILE NAME");
        if(out_size)
            *out_size = strlen(f->dh.filename) + 1;
        if(buf_size < strlen(f->dh.filename) || !buf)
            return MRSE_INSUFFICIENT_MEM;
        strcpy((char*)buf, f->dh.filename);
        break;
    case MRSFI_SIZE:
        if(out_size)
            *out_size = sizeof(size_t);
        if(buf_size < sizeof(size_t) || !buf)
            return MRSE_INSUFFICIENT_MEM;
        *(size_t*)buf = f->dh.h.uncompressed_size;
        break;
    case MRSFI_CSIZE:
        if(out_size)
            *out_size = sizeof(size_t);
        if(buf_size < sizeof(size_t) || !buf)
            return MRSE_INSUFFICIENT_MEM;
        *(size_t*)buf = f->dh.h.compressed_size;
        break;
    case MRSFI_TIME:
        if(out_size)
            *out_size = sizeof(time_t);
        if(buf_size < sizeof(time_t) || !buf)
            return MRSE_INSUFFICIENT_MEM;
        *(time_t*)buf = mktimedos(f->dh.h.filetime);
        break;
    case MRSFI_CRC32:
        if(out_size)
            *out_size = sizeof(uint32_t);
        if(buf_size < sizeof(uint32_t) || !buf)
            return MRSE_INSUFFICIENT_MEM;
        *(uint32_t*)buf = f->dh.h.crc32;
        break;
    case MRSFI_LHEXTRA:
        if(out_size)
            *out_size = f->lh.extra ? f->lh.h.extra_length : 0;
        if(buf_size < f->lh.h.extra_length || !buf)
            return MRSE_INSUFFICIENT_MEM;
        if(!f->lh.extra)
            return MRSE_INFO_NOT_AVAILABLE;
        memcpy(buf, f->lh.extra, f->lh.h.extra_length);
        break;
    case MRSFI_DHEXTRA:
        if(out_size)
            *out_size = f->dh.extra ? f->dh.h.extra_length : 0;
        if(buf_size < f->dh.h.extra_length || !buf)
            return MRSE_INSUFFICIENT_MEM;
        if(!f->dh.extra)
            return MRSE_INFO_NOT_AVAILABLE;
        memcpy(buf, f->dh.extra, f->dh.h.extra_length);
        break;
    case MRSFI_DHCOMMENT:
        if(out_size)
            *out_size = f->dh.comment ? f->dh.h.comment_length : 0;
        if(buf_size < f->dh.h.comment_length || !buf)
            return MRSE_INSUFFICIENT_MEM;
        if(!f->dh.comment)
            return MRSE_INFO_NOT_AVAILABLE;
        memcpy(buf, f->dh.comment, f->dh.h.comment_length);
        break;
    default:
        return MRSE_INVALID_PARAM;
    }

    return MRSE_OK;
}

size_t mrs_get_file_count(const MRS* mrs){
    if(!_mrs_is_initialized(mrs))
        return 0;
    
    return mrs->_hdr.dir_count;
}

int mrs_write(MRS* mrs, unsigned index, const unsigned char* buf, size_t buf_size){
    unsigned char* temp = NULL;
    size_t stemp;
    struct mrs_file_t* f;

    if(!_mrs_is_initialized(mrs))
        return MRSE_UNITIALIZED;

    if(index >= mrs->_hdr.dir_count)
        return MRSE_INVALID_INDEX;
    
    f = &mrs->_files[index];

    f->dh.h.crc32 = crc32(0L, Z_NULL, 0);
    f->dh.h.crc32 = crc32(f->dh.h.crc32, buf, buf_size);
    f->lh.h.crc32 = f->dh.h.crc32;

    f->lh.h.uncompressed_size = f->dh.h.uncompressed_size = buf_size;
    
    if(!_compress_file(buf, buf_size, &temp, &stemp)){
        dbgprintf("Could not compress, let's just STORE instead");
        f->lh.h.compression = f->dh.h.compression = MRSCM_STORE;
        f->lh.h.compressed_size = f->dh.h.compressed_size = buf_size;
    }else{
        f->lh.h.compression = f->dh.h.compression = MRSCM_DEFLATE;
        f->lh.h.compressed_size = f->dh.h.compressed_size = stemp;
    }

    f->dh.h.offset = _mrs_temp_tell(mrs);

    _mrs_temp_write(mrs, temp ? temp : buf, f->dh.h.compressed_size);

    dbgprintf("Ok, we good to go");

    free(temp);

    return MRSE_OK;
}

int mrs_set_signature_check(MRS* mrs, MRS_SIGNATURE_FUNC f){
    if(!_mrs_is_initialized(mrs))
        return MRSE_UNITIALIZED;
    
    mrs->_sig = f;

    return MRSE_OK;
}

int mrs_set_signature(MRS* mrs, int where, uint32_t signature){
    if(!_mrs_is_initialized(mrs))
        return MRSE_UNITIALIZED;
    
    if(where & MRSSW_BASE_HDR)
        mrs->_sigs[0] = signature;
    
    if(where & MRSSW_LOCAL_HDR)
        mrs->_sigs[1] = signature;
    
    if(where & MRSSW_CENTRAL_DIR_HDR)
        mrs->_sigs[2] = signature;

    return MRSE_OK;
}

int mrs_remove(MRS* mrs, unsigned index){
    struct mrs_file_t* f;

    if(!_mrs_is_initialized(mrs))
        return MRSE_UNITIALIZED;
    
    if(index >= mrs->_hdr.dir_count)
        return MRSE_INVALID_INDEX;
    
    f = &mrs->_files[index];

    _mrs_file_free(mrs, f);

    dbgprintf("Removing file %u", index);

    if(mrs->_hdr.dir_count > 1){
        dbgprintf("%u :: %u :: %u", index, mrs->_hdr.dir_count, mrs->_hdr.dir_count - index - 1);
        if(index+1 < mrs->_hdr.dir_count)
            memmove(f, f+1, (mrs->_hdr.dir_count - index - 1)*sizeof(struct mrs_file_t));
        memset(&mrs->_files[mrs->_hdr.dir_count - 1], 0, sizeof(struct mrs_file_t));
        mrs->_files = (struct mrs_file_t*)realloc(mrs->_files, (mrs->_hdr.dir_count - 1)*sizeof(struct mrs_file_t));
    }else{
        dbgprintf("We had only 1 file, so let's just free our files pointer");
        free(mrs->_files);
        mrs->_files = NULL;
    }

    dbgprintf("Removed file %u", index);

    mrs->_hdr.dir_count--;

    return MRSE_OK;
}

int mrs_find_file(const MRS* mrs, const char* s, unsigned* index){
    unsigned i;

    if(!_mrs_is_initialized(mrs))
        return MRSE_UNITIALIZED;
    
    for(i=0; i<mrs->_hdr.dir_count; i++){
        if(!stricmp(s, mrs->_files[i].dh.filename)){
            if(index)
                *index = i;
            return MRSE_OK;
        }
    }

    return MRSE_NOT_FOUND;
}

int mrs_set_file_info(MRS* mrs, unsigned index, enum mrs_file_info_t what, const void* buf, size_t buf_size){
    struct mrs_file_t* f;

    if(!_mrs_is_initialized(mrs))
        return MRSE_UNITIALIZED;
    
    if(index >= mrs->_hdr.dir_count)
        return MRSE_INVALID_INDEX;
    
    f = &mrs->_files[index];
    
    switch(what){
    case MRSFI_NAME:
        if(!buf || !buf_size)
            return MRSE_INVALID_FILENAME;
        if(!_is_valid_input_filename((const char*)buf))
            return MRSE_INVALID_FILENAME;
        /// TODO: CHECK FOR DUPLICATES
        /// TODO: CHECK FOR DUPLICATES
        f->dh.filename = (char*)realloc(f->dh.filename, buf_size);
        memcpy(f->dh.filename, buf, buf_size);
        f->lh.filename = f->dh.filename;
        f->lh.h.filename_length = f->dh.h.filename_length = buf_size;
        break;
    case MRSFI_TIME:
        if(buf_size < sizeof(time_t))
            return MRSE_INSUFFICIENT_MEM;
        
        f->dh.h.filetime = dostime(buf);
        f->lh.h.filetime = f->dh.h.filetime;
        break;
    case MRSFI_LHEXTRA:
        //if(f->lh.extra != f->dh.extra)
            //free(f->lh.extra);
        _mrs_ref_table_free(&mrs->_reftable, f->lh.extra);
        if(!buf || !buf_size){
            f->lh.extra = NULL;
            f->lh.h.extra_length = 0;
        }else{
            f->lh.extra = _mrs_ref_table_append(&mrs->_reftable, buf, buf_size);
            //if(buf_size == f->dh.h.extra_length && !memcmp(buf, f->dh.extra, buf_size)){
                //f->lh.extra = f->dh.extra;
            //}else{
                //f->lh.extra = (char*)malloc(buf_size);
                //memcpy(f->lh.extra, buf, buf_size);
            //}
        }
        break;
    case MRSFI_DHEXTRA:
        if(f->dh.extra != f->lh.extra)
            free(f->dh.extra);
        if(!buf || !buf_size){
            f->dh.extra = NULL;
            f->dh.h.extra_length = 0;
        }else{
            if(buf_size == f->lh.h.extra_length && !memcmp(buf, f->lh.extra, buf_size)){
                f->dh.extra = f->lh.extra;
            }else{
                f->dh.extra = (char*)malloc(buf_size);
                memcpy(f->dh.extra, buf, buf_size);
            }
        }
        break;
    case MRSFI_DHCOMMENT:
        free(f->dh.comment);
        if(!buf || !buf_size){
            f->dh.comment = NULL;
        }else{
            f->dh.comment = (char*)malloc(buf_size);
            memcpy(f->dh.comment, buf, buf_size);
        }
        break;
    default:
        return MRSE_INVALID_PARAM;
    }

    return MRSE_OK;
}

int mrs_save(MRS* mrs, enum mrs_save_t type, const char* output, MRS_PROGRESS_FUNC pcallback){
    if(!_mrs_is_initialized(mrs))
        return MRSE_UNITIALIZED;
    
    if(!output)
        return MRSE_INVALID_PARAM;

    switch(type){
    case MRSS_MRS:
        return _mrs_save_mrs(mrs, output, pcallback);
    case MRSS_FOLDER:
        return _mrs_save_folder(mrs, output, pcallback);
    }

    return MRSE_INVALID_PARAM;
}

void mrs_free(MRS* mrs){
    unsigned i;
    if(!_mrs_is_initialized(mrs)){
        dbgprintf("mrs handle apparently not initialized (NULL pointer)");
        return;
    }

    dbgprintf("Let's free it");
    
    _mrs_ref_table_free_all(&mrs->_reftable);
    if(mrs->_files){
        dbgprintf("We got files, %u of them", mrs->_hdr.dir_count);
        for(i=0; i<mrs->_hdr.dir_count; i++){
            _mrs_file_free(mrs, &mrs->_files[i]);
            dbgprintf("  Freed file %u", i);
            /*if (mrs->_files[i].lh.filename != mrs->_files[i].dh.filename) {
                dbgprintf("We got different filenames between LOCAL and CENTRAL DIR [%u] headers", i);
                free(mrs->_files[i].lh.filename);
                dbgprintf("Freed LOCAL filename[%u]", i);
            }
            free(mrs->_files[i].dh.filename);
            dbgprintf("Freed CENTRAL DIR filename[%u]", i);
            if (mrs->_files[i].lh.extra != mrs->_files[i].dh.extra) {
                dbgprintf("We got different extra between LOCAL and CENTRAL DIR [%u] headers", i);
                free(mrs->_files[i].lh.extra);
                dbgprintf("Freed LOCAL extra[%u]", i);
            }
            free(mrs->_files[i].dh.extra);
            dbgprintf("Freed CENTRAL DIR extra[%u]", i);
            free(mrs->_files[i].dh.comment);
            dbgprintf("Freed CENTRAL DIR comment[%u]", i);*/
        }
        free(mrs->_files);
        dbgprintf("Freed our files");
    }

    if(mrs->_mtype == MRSMT_TEMPFILE){
        dbgprintf("We were using a temporary file, so let's close it");
        fclose(mrs->_fbuf);
        dbgprintf("Temporary file closed");
    }else{
        dbgprintf("We were using memory for temporary storage, so let's free it");
        free(mrs->_mbuf);
        dbgprintf("Temporary memory freed");
    }

    free(mrs->_ptr);
    dbgprintf("mrs handle freed");
}

/*******************************
   Global MRS functions
*******************************/

int mrs_global_compile(const char* name, const char* out_name, struct mrs_encryption_t* encryption, struct mrs_signature_t* sig, MRS_PROGRESS_FUNC pcallback){
    char* real_output;
    char* temp;
    unsigned e;
    MRS* mrs;
    
    if(!PathFileExistsA(name) || !PathIsDirectoryA(name))
        return MRSE_CANNOT_OPEN;
    
    real_output = (char*)malloc((out_name ? strlen(out_name) : strlen(name)) + 5);
    strcpy(real_output, out_name ? out_name : name);
    if(!out_name)
        strcat(real_output, ".mrs");
    
    e = GetFullPathNameA(real_output, 0, NULL, NULL);
    temp = (char*)malloc(e+1);
    memset(temp, 0, e+1);
    GetFullPathNameA(real_output, e+1, temp, NULL);
    free(real_output);
    real_output = temp;
    
    dbgprintf("real_output \"%s\"", real_output);
    if(PathFileExistsA(real_output) && PathIsDirectoryA(real_output)){
        free(real_output);
        return MRSE_INVALID_FILENAME;
    }
    
    dbgprintf("Ok, let's compile \"%s\" into \"%s\"", name, real_output);
    
    mrs = mrs_init();
    if(!mrs){
        free(real_output);
        return MRSE_INSUFFICIENT_MEM;
    }
    
    if(encryption){
        mrs_set_encryption(mrs, MRSEW_BASE_HDR, encryption->base_hdr);
        mrs_set_encryption(mrs, MRSEW_LOCAL_HDR, encryption->local_hdr);
        mrs_set_encryption(mrs, MRSEW_CENTRAL_DIR_HDR, encryption->central_dir_hdr);
        mrs_set_encryption(mrs, MRSEW_BUFFER, encryption->buffer);
    }

    if(sig){
        mrs_set_signature(mrs, MRSSW_BASE_HDR, sig->base_hdr);
        mrs_set_signature(mrs, MRSSW_LOCAL_HDR, sig->local_hdr);
        mrs_set_signature(mrs, MRSSW_CENTRAL_DIR_HDR, sig->central_dir_hdr);
    }
    
    e = mrs_add(mrs, MRSA_FOLDER, name, NULL, MRSDB_KEEP_NEW);
    if(e){
        mrs_free(mrs);
        free(real_output);
        return e;
    }
    
    e = mrs_save(mrs, MRSS_MRS, real_output, pcallback);
    
    mrs_free(mrs);
    
    free(real_output);
    
    return e;
}

int mrs_global_decompile(const char* name, const char* out_name, struct mrs_encryption_t* decryption, MRS_SIGNATURE_FUNC sig_check, MRS_PROGRESS_FUNC pcallback){
    char* real_output;
    char* temp;
    unsigned e;
    MRS* mrs;

    if(!PathFileExistsA(name) || PathIsDirectoryA(name))
        return MRSE_CANNOT_OPEN;

    real_output = (char*)malloc((out_name ? strlen(out_name) : strlen(name)) + 1);
    strcpy(real_output, out_name ? out_name : name);
    if(!out_name){
        temp = strrchr(real_output, '.');
        if(temp)
            *temp = 0;
    }

    e = GetFullPathNameA(real_output, 0, NULL, NULL);
    temp = (char*)malloc(e+1);
    memset(temp, 0, e+1);
    GetFullPathNameA(real_output, e+1, temp, NULL);
    free(real_output);
    real_output = temp;

    if(PathFileExistsA(real_output) && !PathIsDirectoryA(real_output)){
        free(real_output);
        return MRSE_INVALID_FILENAME;
    }

    dbgprintf("Let's decompile \"%s\" as \"%s\"", name, real_output);

    mrs = mrs_init();
    if(!mrs){
        free(real_output);
        return MRSE_INSUFFICIENT_MEM;
    }
    
    if(decryption){
        mrs_set_decryption(mrs, MRSEW_BASE_HDR, decryption->base_hdr);
        mrs_set_decryption(mrs, MRSEW_LOCAL_HDR, decryption->local_hdr);
        mrs_set_decryption(mrs, MRSEW_CENTRAL_DIR_HDR, decryption->central_dir_hdr);
        mrs_set_decryption(mrs, MRSEW_BUFFER, decryption->buffer);
    }

    if(sig_check)
        mrs_set_signature_check(mrs, sig_check);
    
    e = mrs_add(mrs, MRSA_MRS, name, NULL, MRSDB_KEEP_NEW);
    if(e){
        mrs_free(mrs);
        free(real_output);
        return e;
    }

    e = mrs_save(mrs, MRSS_FOLDER, real_output, pcallback);

    mrs_free(mrs);

    free(real_output);

    return e;
}

int mrs_global_list(const char* name, struct mrs_encryption_t* decryption, MRS_SIGNATURE_FUNC sig_check, MRSFILE* f){
    FILE* fp;
    struct mrs_hdr_t hdr;
    struct mrs_encryption_t decrypt;
    unsigned char* dhbuf;
    struct mrs_central_dir_hdr_t* dh;
    
    if(!f)
        return MRSE_INVALID_PARAM;
    
    if(decryption){
        decrypt.base_hdr        = decryption->base_hdr ? decryption->base_hdr : mrs_default_decrypt;
        decrypt.central_dir_hdr = decryption->central_dir_hdr ? decryption->central_dir_hdr : decrypt.base_hdr;
    }else{
        decrypt.base_hdr        = mrs_default_decrypt;
        decrypt.central_dir_hdr = mrs_default_decrypt;
    }
  
    dbgprintf("Listing files of archive \"%s\"", name);
    fp = fopen(name, "rb");
    if(!fp)
        return MRSE_NOT_FOUND;
    
    fseek(fp, -(int)sizeof(struct mrs_hdr_t), SEEK_END);
    fread(&hdr, sizeof(struct mrs_hdr_t), 1, fp);
    decrypt.base_hdr((unsigned char*)&hdr, sizeof(struct mrs_hdr_t));
    dbgprintf("signature = %08x", hdr.signature);
	
    if(!mrs_default_signatures(MRSSW_BASE_HDR, hdr.signature) && (!sig_check || !sig_check(MRSSW_BASE_HDR, hdr.signature))){
        dbgprintf("Invalid signature");
        fclose(fp);
        return MRSE_INVALID_MRS;
    }
    
    if(!hdr.dir_count){
        dbgprintf("Empty mrs file");
        return MRSE_EMPTY;
    }
    
    dhbuf = (unsigned char*)malloc(hdr.dir_size);
    fseek(fp, hdr.dir_offset, SEEK_SET);
    fread(dhbuf, hdr.dir_size, 1, fp);
    decrypt.central_dir_hdr(dhbuf, hdr.dir_size);
    fclose(fp);
    
    dh = (struct mrs_central_dir_hdr_t*)dhbuf;
    dbgprintf("Dir header signature = %08x", dh->signature);
    if(!mrs_default_signatures(MRSSW_CENTRAL_DIR_HDR, dh->signature) && (!sig_check || !sig_check(MRSSW_CENTRAL_DIR_HDR, dh->signature))){
        free(dhbuf);
        return MRSE_INVALID_ENCRYPTION;
    }
    
    *f = (MRSFILE)malloc(sizeof(struct mrs_afile_internal_t));
    (*f)->name = (const char*)malloc(dh->filename_length+1);
    memset((*f)->name, 0, dh->filename_length+1);
    strncpy((*f)->name, dhbuf+sizeof(struct mrs_central_dir_hdr_t), dh->filename_length);
    _strbkslash((*f)->name, 0);
    (*f)->crc32 = dh->crc32;
    (*f)->size  = dh->uncompressed_size;
    (*f)->csize = dh->compressed_size;
    (*f)->ftime = mktimedos(dh->filetime);
    ((struct mrs_afile_internal_t*)*f)->obuf = dhbuf;
    ((struct mrs_afile_internal_t*)*f)->buf = dhbuf;
    ((struct mrs_afile_internal_t*)*f)->i = 0;
    ((struct mrs_afile_internal_t*)*f)->cnt = hdr.dir_count;
    ((struct mrs_afile_internal_t*)*f)->sig = sig_check;
    ((struct mrs_afile_internal_t*)*f)->ptr = *f;
    
    dbgprintf("name  ::: %s", (*f)->name);
    dbgprintf("crc32 ::: %08x", (*f)->crc32);
    dbgprintf("size  ::: %u", (*f)->size);
    dbgprintf("csize ::: %u", (*f)->csize);
    dbgprintf("ftime ::: %u", (unsigned)(*f)->ftime);
    dbgprintf("index ::: %u", ((struct mrs_afile_internal_t*)*f)->i);
  
    return MRSE_OK;
}

int mrs_global_list_next(MRSFILE f){
    unsigned index;
    unsigned char* dhbuf;
    struct mrs_central_dir_hdr_t* dh;
    MRS_SIGNATURE_FUNC sig_check;
    
    if(!f)
        return MRSE_INVALID_PARAM;
    
    index = ((struct mrs_afile_internal_t*)f)->i;
    
    index++;
    if(index >= ((struct mrs_afile_internal_t*)f)->cnt){
        dbgprintf("No more files to list...");
        return MRSE_NO_MORE_FILES;
    }
    
    sig_check = ((struct mrs_afile_internal_t*)f)->sig;
    
    dhbuf = ((struct mrs_afile_internal_t*)f)->buf;
    dh = (struct mrs_central_dir_hdr_t*)dhbuf;
    dhbuf += sizeof(struct mrs_central_dir_hdr_t) + dh->filename_length + dh->extra_length + dh->comment_length;
    dh = (struct mrs_central_dir_hdr_t*)dhbuf;
    
    dbgprintf("signature %08x", dh->signature);
    if(!mrs_default_signatures(MRSSW_CENTRAL_DIR_HDR, dh->signature) && (sig_check && !sig_check(MRSSW_CENTRAL_DIR_HDR, dh->signature))){
        dbgprintf("Invalid encryption");
        return MRSE_INVALID_ENCRYPTION;
    }
    
    f->name = (const char*)realloc(f->name, dh->filename_length+1);
    memset(f->name, 0, dh->filename_length+1);
    strncpy(f->name, dhbuf+sizeof(struct mrs_central_dir_hdr_t), dh->filename_length);
    _strbkslash(f->name, 0);
    f->crc32 = dh->crc32;
    f->size  = dh->uncompressed_size;
    f->csize = dh->compressed_size;
    f->ftime = mktimedos(dh->filetime);
    
    ((struct mrs_afile_internal_t*)f)->buf = dhbuf;
    ((struct mrs_afile_internal_t*)f)->i++;
    
    return MRSE_OK;
}

int mrs_global_list_free(MRSFILE f){
    if(!f)
        return MRSE_INVALID_PARAM;
    
    dbgprintf("freeing...");
    
    free(f->name);
    free(((struct mrs_afile_internal_t*)f)->obuf);
    free(((struct mrs_afile_internal_t*)f)->ptr);
    
    dbgprintf("freed");
    
    return MRSE_OK;
}

/*******************************
    Other functions
*******************************/

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

const char* mrs_get_error_str(unsigned e){
    return e < MRSE_END ? mrs_error_str[e] : NULL;
}