/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#define __LIBMRS_INTERNAL__

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
#include <stdarg.h>
#include <time.h>

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

#include "mrs_internal.h"
#include "mrs_dbg.h"

/*******************************
   Extern functions
   and variables
*******************************/

                  /// FROM mrs_ref_table.c
          extern void _mrs_ref_table_init(struct mrs_ref_table_t* r);
                  /// FROM mrs_ref_table.c
extern unsigned char* _mrs_ref_table_append(struct mrs_ref_table_t* r,
                                            const unsigned char* s,
                                            size_t len);
                  /// FROM mrs_ref_table.c
           extern int _mrs_ref_table_free(struct mrs_ref_table_t* r,
                                          unsigned char* s);
                  /// FROM mrs_ref_table.c
          extern void _mrs_ref_table_free_all(struct mrs_ref_table_t* r);
                  /// FROM mrs_util.c
           extern int _mrs_is_initialized(const MRS* mrs);
                  /// FROM mrs_util.c
           extern int _mrs_temp_read(MRS* mrs,
                                     unsigned char* buf,
                                     off_t offset,
                                     size_t size);
                  /// FROM mrs_util.c
           extern int _mrs_temp_write(MRS* mrs,
                                      unsigned char* buf,
                                      size_t size);
                  /// FROM mrs_file.c
          extern void _mrs_file_free(struct mrs_file_t* f);
                  /// FROM utils.c
           extern int _compress_file(unsigned char* inbuf,
                                     size_t total_in,
                                     unsigned char** outbuf,
                                     size_t* total_out);
                  /// FROM mrs_util.c
           extern int _uncompress_file(unsigned char* inbuf,
                                       size_t total_in,
                                       unsigned char* outbuf,
                                       size_t uncompressed_size,
                                       size_t* out_size);
                  /// FROM mrs_util.c
           extern int _is_valid_input_filename(const char* s);
                  /// FROM mrs_util.c
           extern int _strbkslash(char* s,
                                  size_t size);
                  /// FROM mrs_add.c
           extern int _mrs_add_memory(MRS* mrs,
                                      const void* buffer,
                                      size_t buffer_size,
                                      const char* name,
                                      const time_t* timep,
                                      void* reserved,
                                      enum mrs_dupe_behavior_t on_dupe,
                                      int check_name,
                                      int check_dup,
                                      int pushit,
                                      struct mrs_file_t* f_out,
                                      int* isreplace,
                                      int* replaceindex);
                  /// FROM mrs_add.c
           extern int _mrs_add_filedes(MRS* mrs,
                                       int fd,
                                       char* filename,
                                       void* reserved,
                                       enum mrs_dupe_behavior_t on_dupe,
                                       int check_name,
                                       int check_dup,
                                       int pushit,
                                       struct mrs_file_t* f_out,
                                       int* isreplace,
                                       int* replaceindex);
                  /// FROM mrs_add.c
           extern int _mrs_add_fileptr(MRS* mrs,
                                       FILE* fp,
                                       char* filename,
                                       void* reserved,
                                       enum mrs_dupe_behavior_t on_dupe,
                                       int check_name,
                                       int check_dup,
                                       int pushit,
                                       struct mrs_file_t* f_out,
                                       int* isreplace,
                                       int* replaceindex);
                  /// FROM mrs_add.c
           extern int _mrs_add_file(MRS* mrs,
                                    const char* filename,
                                    char* final_name,
                                    void* reserved,
                                    enum mrs_dupe_behavior_t on_dupe,
                                    int pushit, struct mrs_file_t* f_out,
                                    int* isreplace,
                                    int* replaceindex);
                  /// FROM mrs_add.c
           extern int _mrs_add_folder(MRS* mrs,
                                      const char* foldername,
                                      char* final_name,
                                      void* reserved,
                                      enum mrs_dupe_behavior_t on_dupe);
                  /// FROM mrs_add.c
           extern int _mrs_add_mrs(MRS* mrs,
                                   const char* mrsname,
                                   char* final_name,
                                   void* reserved,
                                   enum mrs_dupe_behavior_t on_dupe);
                  /// FROM mrs_add.c
           extern int _mrs_add_mrs2(MRS* mrs,
                                    MRS* in,
                                    char* base_name,
                                    void* reserved,
                                    enum mrs_dupe_behavior_t on_dupe);
                  /// FROM mrs_save.c
           extern int _mrs_save_mrs_fname(const MRS* mrs,
                                          const char* output,
                                          MRS_PROGRESS_FUNC pcallback);
                  /// FROM mrs_save.c
           extern int _mrs_save_mrs(const MRS* mrs,
                                    FILE* f,
                                    MRS_PROGRESS_FUNC pcallback);
                  /// FROM mrs_save.c
           extern int _mrs_save_folder(MRS* mrs,
                                       const char* output,
                                       MRS_PROGRESS_FUNC pcallback);
                  /// FROM mrs_util.c
   extern const char* mrs_error_str[];

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

int mrs_add(MRS* mrs, enum mrs_add_t what, enum mrs_dupe_behavior_t on_dupe, void* reserved, ...){
    va_list a;
    void    *par1, *par2, *par3, *par4;

    dbgprintf("Let's add something!");

    if(!_mrs_is_initialized(mrs))
        return MRSE_UNITIALIZED;

    va_start(a, reserved);

    switch(what){
    case MRSA_FILE:
        dbgprintf("From file");
        par1 = va_arg(a, const char*);
        par2 = va_arg(a, char*);
        return _mrs_add_file(mrs, (const char*)par1, (char*)par2, reserved, on_dupe, 1, NULL, NULL, NULL);
    case MRSA_FOLDER:
        dbgprintf("From directory");
        par1 = va_arg(a, const char*);
        par2 = va_arg(a, char*);
        return _mrs_add_folder(mrs, (const char*)par1, (char*)par2, reserved, on_dupe);
    case MRSA_MRS:
        dbgprintf("From MRS archive");
        par1 = va_arg(a, const char*);
        par2 = va_arg(a, char*);
        return _mrs_add_mrs(mrs, (const char*)par1, (char*)par2, reserved, on_dupe);
    case MRSA_MRS2:
        dbgprintf("From MRS handle");
        par1 = va_arg(a, MRS*);
        par2 = va_arg(a, char*);
        return _mrs_add_mrs2(mrs, (MRS*)par1, (char*)par2, reserved, on_dupe);
        break;
    case MRSA_FILEPTR:
        dbgprintf("From FILE pointer");
        par1 = va_arg(a, FILE*);
        par2 = va_arg(a, char*);
        return _mrs_add_fileptr(mrs, (FILE*)par1, (char*)par2, reserved, on_dupe, 1, 1, 1, NULL, NULL, NULL);
    case MRSA_FILEDES:
        dbgprintf("From file descriptor");
        par1 = va_arg(a, int);
        par2 = va_arg(a, char*);
        return _mrs_add_filedes(mrs, (int)par1, (char*)par2, reserved, on_dupe, 1, 1, 1, NULL, NULL, NULL);
    case MRSA_MEMORY:
        dbgprintf("From memory");
        par1 = va_arg(a, const void*);
        par2 = va_arg(a, size_t);
        par3 = va_arg(a, const char*);
        par4 = time(NULL);
        //return _mrs_add_memory(mrs, (const void*)par1, (size_t)par2, (const char*)par3, (time_t)&par4, reserved, on_dupe, 1, 1);
        return _mrs_add_memory(mrs, (const void*)par1, (size_t)par2, (const char*)par3, (const time_t*)&par4, reserved, on_dupe, 1, 1, 1, NULL, NULL, NULL);
    default:
        return MRSE_INVALID_PARAM;
    }

    return MRSE_OK;
}

/*******************************
OLD mrs_add FUNCTION
*******************************/
/*
int mrs_add(MRS* mrs, enum mrs_add_t what, void* param1, void* param2, enum mrs_dupe_behavior_t on_dupe){
    dbgprintf("Let's add something!");

    if(!_mrs_is_initialized(mrs))
        return MRSE_UNITIALIZED;
    
    if(!param1)
        return MRSE_INVALID_PARAM;
    
    switch(what){
    case MRSA_FILE:
        return _mrs_add_file(mrs, (const char*)param1, (const char*)param2, on_dupe);
    case MRSA_FOLDER:
        return _mrs_add_folder(mrs, (const char*)param1, (const char*)param2, on_dupe);
    case MRSA_MRS:
        return _mrs_add_mrs(mrs, (const char*)param1, (const char*)param2, on_dupe);
    case MRSA_MRS2:
        return _mrs_add_mrs2(mrs, (MRS*)param1, (const char*)param2, on_dupe);
    }

    return MRSE_OK;
}
*/

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

    _mrs_file_free(f);

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
        _mrs_ref_table_free(&mrs->_reftable, f->lh.extra);
        if(!buf || !buf_size){
            f->lh.extra = NULL;
            f->lh.h.extra_length = 0;
        }else{
            f->lh.extra = _mrs_ref_table_append(&mrs->_reftable, buf, buf_size);
            f->lh.h.extra_length = buf_size;
            //if(buf_size == f->dh.h.extra_length && !memcmp(buf, f->dh.extra, buf_size)){
                //f->lh.extra = f->dh.extra;
            //}else{
                //f->lh.extra = (char*)malloc(buf_size);
                //memcpy(f->lh.extra, buf, buf_size);
            //}
        }
        break;
    case MRSFI_DHEXTRA:
        _mrs_ref_table_free(&mrs->_reftable, f->dh.extra);
        if(!buf || !buf_size){
            f->dh.extra = NULL;
            f->dh.h.extra_length = 0;
        }else{
            f->dh.extra = _mrs_ref_table_append(&mrs->_reftable, buf, buf_size);
            f->dh.h.extra_length = buf_size;
            /*if (buf_size == f->lh.h.extra_length && !memcmp(buf, f->lh.extra, buf_size)) {
                f->dh.extra = f->lh.extra;
            }else{
                f->dh.extra = (char*)malloc(buf_size);
                memcpy(f->dh.extra, buf, buf_size);
            }*/
        }
        break;
    case MRSFI_DHCOMMENT:
        _mrs_ref_table_free(&mrs->_reftable, f->dh.comment);
        if(!buf || !buf_size){
            f->dh.comment = NULL;
            f->dh.h.comment_length = 0;
        }else{
            f->dh.comment = _mrs_ref_table_append(&mrs->_reftable, buf, buf_size);
            f->dh.h.comment_length = buf_size;
            /*f->dh.comment = (char*)malloc(buf_size);
            memcpy(f->dh.comment, buf, buf_size);*/
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
        return _mrs_save_mrs_fname(mrs, output, pcallback);
    case MRSS_FOLDER:
        return _mrs_save_folder(mrs, output, pcallback);
    }

    return MRSE_INVALID_PARAM;
}

int mrs_save_mrs_fp(MRS* mrs, FILE* output, MRS_PROGRESS_FUNC pcallback){
    if(!_mrs_is_initialized(mrs))
        return MRSE_UNITIALIZED;
    
    if(!output)
        return MRSE_INVALID_PARAM;

    return _mrs_save_mrs(mrs, output, pcallback);

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
            _mrs_file_free(&mrs->_files[i]);
            dbgprintf("  Freed file %u", i);
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

///NOTE: Needs to test the encryption of HEADER, LOCAL HEADER and CENTRAL DIR HEADER,
///      should also test the compressed buffer of at least one file stored in the MRS archive.
int mrs_global_verify(const char* filename, const struct mrs_encryption_t* decryption, MRS_SIGNATURE_FUNC sigcheck){
    struct mrs_hdr_t hdr;
    struct mrs_encryption_t dec;
    FILE* fp;
    
    if(decryption){
        dec.base_hdr        = decryption->base_hdr        ? decryption->base_hdr        : mrs_default_decrypt;
        dec.local_hdr       = decryption->local_hdr       ? decryption->local_hdr       : dec.base_hdr;
        dec.central_dir_hdr = decryption->central_dir_hdr ? decryption->central_dir_hdr : dec.base_hdr;
        dec.buffer          = decryption->buffer          ? decryption->buffer          : NULL;
    }else{
        dec.base_hdr        = mrs_default_decrypt;
        dec.local_hdr       = dec.base_hdr;
        dec.central_dir_hdr = dec.base_hdr;
        dec.buffer          = NULL;
    }
    
    dbgprintf("filename = %s", filename);
    
    fp = fopen(filename, "rb");
    if(!fp){
        dbgprintf(" file not found or cannot be opened");
        return MRSE_NOT_FOUND;
    }
    
    fseek(fp, -(int)sizeof(struct mrs_hdr_t), SEEK_END);
    fread(&hdr, sizeof(struct mrs_hdr_t), 1, fp);
    dec.base_hdr((unsigned char*)&hdr, sizeof(struct mrs_hdr_t));
    dbgprintf("signature = %08x", hdr.signature);
    
    if(!mrs_default_signatures(MRSSW_BASE_HDR, hdr.signature) && (!sigcheck || !sigcheck(MRSSW_BASE_HDR, hdr.signature))){
        dbgprintf("  invalid signature");
        return MRSE_INVALID_MRS;
    }
    
    fclose(fp);

    return MRSE_OK;
}

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
    
    // e = mrs_add(mrs, MRSA_FOLDER, name, NULL, MRSDB_KEEP_NEW);
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
    
    // e = mrs_add(mrs, MRSA_MRS, name, NULL, MRSDB_KEEP_NEW);
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

const char* mrs_get_error_str(unsigned e){
    return e < MRSE_END ? mrs_error_str[e] : NULL;
}