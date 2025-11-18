/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#define __LIBMRS_INTERNAL__

#include <stdio.h>
#ifdef _MSC_BUILD
#include <fcntl.h>
#else
#include <sys/fcntl.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>
#include <shlwapi.h>
#include <stdarg.h>

#include "mrs.h"
#include "mrs_internal.h"
#include "mrs_dbg.h"
#include "zlib.h"
#include "zconf.h"

/*******************************
    Extern functions
    and variables
*******************************/

                  /// FROM mrs_util.c
           extern int _compress_file(unsigned char* inbuf, size_t total_in, unsigned char** outbuf, size_t* total_out);
                  /// FROM utils.c
           extern int _is_valid_input_filename(const char* s);
                  /// FROM mrs_util.c
          extern void mrs_central_dir_hdr(struct mrs_central_dir_hdr_t* dh, uint32_t signature, uint16_t version_made, uint16_t version_needed, uint16_t flags, uint16_t compression, struct dostime_t filetime, uint32_t crc32, uint32_t compressed_size, uint32_t uncompressed_size, uint16_t filename_length, uint16_t extra_length, uint16_t comment_length, uint16_t disk_start, uint16_t int_attr, uint32_t ext_attr, uint32_t offset);
                  /// FROM mrs_file.c
          extern void _mrs_file_free(struct mrs_file_t* f);
                  /// FROM mrs_file.c
          extern void _mrs_file_init(struct mrs_file_t* f);
                  /// FROM mrs_file.c
          extern void _mrs_files_append(struct mrs_files_t* f, const struct mrs_file_t* ff);
                  /// FROM mrs_file.c
          extern void _mrs_files_destroy(struct mrs_files_t* f, int freefiles);
                  /// FROM mrs_file.c
          extern void _mrs_files_init(struct mrs_files_t* f);
                  /// FROM mrs_util.c
           extern int _mrs_is_duplicate(const MRS* mrs, const char* s, char** s_out, unsigned* match_index);
                  /// FROM mrs_util.c
           extern int _mrs_is_initialized(const MRS* mrs);
                  /// FROM mrs_util.c
          extern void mrs_local_hdr(struct mrs_local_hdr_t* lh, uint32_t signature, uint16_t version, uint16_t flags, uint16_t compression, struct dostime_t filetime, uint32_t crc32, uint32_t compressed_size, uint32_t uncompressed_size, uint16_t filename_length, uint16_t extra_length);
                  /// FROM mrs_util.c
          extern void _mrs_push_file(MRS* mrs, const struct mrs_file_t f);
                  /// FROM mrs_ref_table.c
extern unsigned char* _mrs_ref_table_append(struct mrs_ref_table_t* r, const unsigned char* s, size_t len);
                  /// FROM mrs_ref_table.c
           extern int _mrs_ref_table_free(struct mrs_ref_table_t* r, unsigned char* s);
                  /// FROM mrs_ref_table.c
          extern void _mrs_ref_table_init(struct mrs_ref_table_t* r);
                  /// FROM mrs_util.c
           extern int _mrs_temp_read(MRS* mrs, unsigned char* buf, off_t offset, size_t size);
                  /// FROM mrs_util.c
         extern off_t _mrs_temp_tell(MRS* mrs);
                  /// FROM mrs_util.c
           extern int _mrs_temp_write(MRS* mrs, unsigned char* buf, size_t size);
                  /// FROM mrs_util.c
           extern int _mrs_replace_file(MRS* mrs, struct mrs_file_t* oldf, struct mrs_file_t* newf);
                  /// FROM mrs_replace_index.c
          extern void _mrs_replace_index_list_add(struct mrs_replace_index_list_t* il, unsigned oldi, unsigned newi);
                  /// FROM mrs_replace_index.c
           extern int _mrs_replace_index_list_do_replace(struct mrs_replace_index_list_t* il, MRS* mrs, const struct mrs_file_t* f, size_t fcount, unsigned index);
                  /// FROM mrs_replace_index.c
          extern void _mrs_replace_index_list_dump(const struct mrs_replace_index_list_t* il);
                  /// FROM mrs_replace_index.c
          extern void _mrs_replace_index_list_free(struct mrs_replace_index_list_t* il);
                  /// FROM mrs_replace_index.c
          extern void _mrs_replace_index_list_init(struct mrs_replace_index_list_t* il);
                  /// FROM utils.c
           extern int _strslash(char* s, size_t size);

#ifdef _LIBMRS_DBG
                  /// FROM mrs_dbg.c
          extern void mrs_file_dump(const struct mrs_file_t* f);
                  /// FROM mrs_dbg.c
          extern void mrs_local_hdr_dump(const struct mrs_local_hdr_t* h);
#else
#define mrs_file_dump(...)
#define mrs_local_hdr_dump(...)
#endif

int _mrs_add_memory(MRS* mrs, const void* buffer, size_t buffer_size,
                    const char* name, const time_t* timep, void* reserved,
                    enum mrs_dupe_behavior_t on_dupe, int check_name, int check_dup,
                    int pushit, struct mrs_file_t* f_out, int *isreplace, int* replaceindex){
    struct mrs_file_t f;
    char*             final_name;
    char*             temp;
    unsigned char*    ubuf      = NULL;
    unsigned char*    cbuf      = NULL;
    unsigned          dup       = 0;
    unsigned          dup_index = 0;
    int               e;
    time_t            timepp = timep ? *timep : time(NULL);

    if(!name){
        dbgprintf("name was not given, leaving...");
        return MRSE_INVALID_PARAM;
    }

    final_name = strdup(name);

    if(check_name){
        _strslash(final_name, 0);
        
        if(_is_valid_input_filename(final_name)){
            dbgprintf("Invalid final filename");
            free(final_name);
            return MRSE_INVALID_FILENAME;
        }
    }

    if(isreplace)
        *isreplace = 0;
    
    if(check_dup){
        if(mrs->_hdr.dir_count){
            dup = _mrs_is_duplicate(mrs, final_name, &temp, &dup_index);
            if(!dup){
                dbgprintf("Found duplicate");
                switch(on_dupe){
                case MRSDB_KEEP_NEW:
                    dbgprintf(" Let's keep the new one");
                    free(temp);
                    temp = NULL;
                    dup = -1;
                    if (isreplace)
                        *isreplace = 1;
                    break;
                case MRSDB_KEEP_OLD:
                    dbgprintf(" Let's keep the old one");
                    free(temp);
                    free(final_name);
                    return MRSE_DUPLICATE;
                case MRSDB_KEEP_BOTH:
                    dbgprintf(" Let's keep both files");
                    free(final_name);
                    final_name = temp;
                    temp = NULL;
                    break;
                }
            }
        }
    }

    _mrs_file_init(&f);

    mrs_central_dir_hdr(&f.dh.h,                //// CENTRAL DIR HEADER
                            MRSM_CDIR_MAGIC1,   // signature
                            MRSV_CDIR_MADE,     // version made
                            MRSV_CDIR_NEEDED,   // version needed
                            0,                  // flags
                            MRSCM_DEFLATE,      // compression method
                            dostime(&timep),    // filetime
                            0,                  // crc32
                            0,                  // compressed size
                            buffer_size,        // uncompressed size
                            0,                  // filename length
                            0,                  // extra length
                            0,                  // comment length
                            0,                  // disk start
                            0,                  // int attr
                            0,                  // ext attr
                            0);                 // offset

    mrs_local_hdr(&f.lh.h,                      //// LOCAL HEADER
                    MRSM_LOCAL_MAGIC1,          // signature
                    MRSV_LOCAL,                 // version
                    0,                          // flags
                    MRSCM_DEFLATE,              // compression method
                    dostime(&timep),            // filetime
                    0,                          // crc32
                    0,                          // compressed size
                    buffer_size,                // uncompressed size
                    0,                          // filename length
                    0);                         // extra length

    ubuf = (unsigned char*)malloc(buffer_size);
    memcpy(ubuf, buffer, buffer_size);

    f.dh.h.crc32 = crc32(0, Z_NULL, 0);
    if(f.dh.h.uncompressed_size)
        f.dh.h.crc32 = crc32(f.dh.h.crc32, ubuf, f.dh.h.uncompressed_size);
    f.lh.h.crc32 = f.dh.h.crc32;

    if(f.dh.h.uncompressed_size){
        e = _compress_file(buffer, buffer_size, &cbuf, &f.dh.h.compressed_size);
        if(!e){
            cbuf = ubuf;
            ubuf = NULL;
            f.dh.h.compressed_size = f.dh.h.uncompressed_size;
            f.dh.h.compression = MRSCM_STORE;
        }
    }else{
        f.dh.h.compressed_size = f.dh.h.uncompressed_size;
        f.dh.h.compression = MRSCM_STORE;
    }
    
    f.lh.h.compressed_size = f.dh.h.compressed_size;    // local header compressed file size
    f.lh.h.compression     = f.dh.h.compression;        // local header compression method

    if(ubuf)
        free(ubuf);

    f.lh.filename = f.dh.filename = final_name;
    f.dh.h.offset = _mrs_temp_tell(mrs);

    _mrs_temp_write(mrs, cbuf, f.dh.h.compressed_size);

    free(cbuf);

    if(check_dup){
        if(dup == -1 && on_dupe == MRSDB_KEEP_NEW){
            dbgprintf("There was an old file with the same name, we are replacing it!");
            if (pushit)
                _mrs_replace_file(mrs, &mrs->_files[dup_index], &f);
            else {
                if(replaceindex)
                    *replaceindex = dup_index;
                if (f_out)
                    memcpy(f_out, &f, sizeof(struct mrs_file_t));
                else
                    _mrs_file_free(&f);
            }
        }else{
            if (pushit) {
                _mrs_push_file(mrs, f);
                dbgprintf("OK, file appended to the MRS handle!");
            }
            else {
                dbgprintf("File will not be appended to the MRS handle!");
                if (f_out)
                    memcpy(f_out, &f, sizeof(struct mrs_file_t));
                else
                    _mrs_file_free(&f);
            }
        }
    }

    return MRSE_OK;
}

/// TODO: Check if the file descriptor is READABLE
int _mrs_add_filedes(MRS* mrs, int fd, char* filename, void* reserved, enum mrs_dupe_behavior_t on_dupe, int check_name, int check_dup, int pushit, struct mrs_file_t* f_out, int *isreplace, int *replaceindex){
    char*          final_name;
    unsigned       dup    = 0;
    unsigned char* buffer = NULL;
    struct stat    fs;
    int            e;

    if(fd == -1){
        dbgprintf("Invalid file descriptor");
        return MRSE_INVALID_PARAM;
    }

    if(!filename){
        dbgprintf("Filename not given");
        return MRSE_INVALID_PARAM;
    }

    final_name = (char*)malloc(strlen(filename) + 1);
    strcpy(final_name, filename);

    if(check_name){
        _strslash(final_name, 0);
        
        if(_is_valid_input_filename(final_name)){
            dbgprintf("Invalid final filename");
            free(final_name);
            return MRSE_INVALID_FILENAME;
        }
    }

    if(check_dup){
        if(mrs->_hdr.dir_count){
            dup = _mrs_is_duplicate(mrs, final_name, NULL, NULL);
            if(!dup){
                dbgprintf("Found duplicate");
                if(on_dupe == MRSDB_KEEP_OLD){
                    dbgprintf(" Let's keep the old one");
                    free(final_name);
                    return MRSE_DUPLICATE;
                }
            }
        }
    }

    if(fstat(fd, &fs) != 0){
        free(final_name);
        return MRSE_CANNOT_OPEN;
    }

    buffer = (unsigned char*)malloc(fs.st_size);
    read(fd, buffer, fs.st_size);

    e = _mrs_add_memory(mrs, buffer, fs.st_size, final_name, &fs.st_mtime, reserved, on_dupe, 0, 1, pushit, f_out, isreplace, replaceindex);

    free(buffer);
    free(final_name);

    return e;
}

/// TODO: Check if the file pointer is READABLE
int _mrs_add_fileptr(MRS* mrs, FILE* fp, char* filename, void* reserved, enum mrs_dupe_behavior_t on_dupe, int check_name, int check_dup, int pushit, struct mrs_file_t* f_out, int* isreplace, int* replaceindex) {
    int      e;
    char* final_name;
    unsigned dup;
    unsigned char* buffer;
    off_t offset;
    size_t len;

    if (!fp) {
        dbgprintf("Invalid file pointer");
        return MRSE_INVALID_PARAM;
    }

    if (!filename) {
        dbgprintf("Filename not given");
        return MRSE_INVALID_PARAM;
    }

    final_name = (char*)malloc(strlen(filename) + 1);
    strcpy(final_name, filename);

    if (check_name) {
        _strslash(final_name, 0);

        if (_is_valid_input_filename(final_name)) {
            dbgprintf("Invalid final filename");
            free(final_name);
            return MRSE_INVALID_FILENAME;
        }
    }

    if (check_dup) {
        if (mrs->_hdr.dir_count) {
            dup = _mrs_is_duplicate(mrs, final_name, NULL, NULL);
            if (!dup) {
                dbgprintf("Found duplicate!");
                if (on_dupe == MRSDB_KEEP_OLD) {
                    dbgprintf("Let's keep the old one");
                    free(final_name);
                    return MRSE_DUPLICATE;
                }
            }
        }
    }
    
    offset = ftell(fp);
    fseek(fp, 0, SEEK_END);
    len = ftell(fp) - offset;
    fseek(fp, offset, SEEK_SET);
    buffer = (unsigned char*)malloc(len);
    fread(buffer, len, 1, fp);
    fseek(fp, offset, SEEK_SET);

    e = _mrs_add_memory(mrs, buffer, len, final_name, NULL, reserved, on_dupe, 0, 1, pushit, f_out, isreplace, replaceindex);

    free(buffer);
    free(final_name);
    
    return e;
}

int _mrs_add_file(MRS* mrs, const char* filename, char* final_name, void* reserved, enum mrs_dupe_behavior_t on_dupe, int pushit, struct mrs_file_t* f_out, int* isreplace, int* replaceindex){
    int   fd;
    char* temp = NULL;
    int   dup;
    int   e;

    if(!final_name){
        final_name = (char*)malloc(strlen(filename) + 1);
        memset(final_name, 0, strlen(filename) + 1);
        strcpy(final_name, filename);
        _strslash(final_name, 0);
        temp = strrchr(filename, '/');
        if(temp)
            strcpy(final_name, temp + 1);
    }else{
        temp = (char*)malloc(strlen(final_name) + 1);
        strcpy(temp, final_name);
        final_name = temp;
    }
    
    temp = NULL;
    _strslash(final_name, 0);

    if(_is_valid_input_filename(final_name)){
        dbgprintf("Invalid final filename");
        free(final_name);
        return MRSE_INVALID_FILENAME;
    }

    if(mrs->_hdr.dir_count){
        dup = _mrs_is_duplicate(mrs, final_name, NULL, NULL);
        if(!dup){
            dbgprintf("Found duplicate");
            if(on_dupe == MRSDB_KEEP_OLD){
                dbgprintf(" Let's keep the old one");
                free(temp);
                free(final_name);
                return MRSE_DUPLICATE;
            }
        }
    }

    fd = open(filename, O_RDONLY | O_BINARY);
    if(fd == -1){
        free(temp);
        free(filename);
        dbgprintf("%s: File not found", filename);
        return MRSE_NOT_FOUND;
    }

    dbgprintf("Ok, ready to read file descriptor.");
    e = _mrs_add_filedes(mrs, fd, final_name, reserved, on_dupe, 0, 1, pushit, f_out, isreplace, replaceindex);

    free(final_name);

    close(fd);

    return MRSE_OK;
}

int _mrs_add_folder(MRS* mrs, const char* foldername, char* base_name, void* reserved, enum mrs_dupe_behavior_t on_dupe) {
    WIN32_FIND_DATAA fda;
    HANDLE           h;
    char**           path_list;
    size_t           len;
    size_t           path_list_count;
    unsigned         i;
    char*            path;
    char*            temp;
    struct mrs_files_t files;
    struct mrs_file_t  f;
    int isreplace, ridx, e;
    struct mrs_replace_index_list_t ridxl;

    if (!PathFileExistsA(foldername)) {
        dbgprintf("Folder \"%s\" does not exist", foldername);
        return MRSE_NOT_FOUND;
    }

    if (base_name) {
        if (_is_valid_input_filename(base_name))
            return MRSE_INVALID_FILENAME;
    }

    path_list = (char**)malloc(sizeof(char*));
    len = _scprintf("%s\\*", foldername);
    path_list[0] = (char*)malloc(len + 1);
    sprintf(path_list[0], "%s\\*", foldername);
    path_list_count = 1;

    dbgprintf("First path_list: \"%s\"", path_list[0]);

    _mrs_files_init(&files);
    _mrs_replace_index_list_init(&ridxl);
    i = 0;
    while (i < path_list_count) {
        h = FindFirstFileA(path_list[i], &fda);
        if (h == INVALID_HANDLE_VALUE) {
            if (i == 0) {
                free(path_list[0]);
                free(path_list);
                return MRSE_EMPTY_FOLDER;
            }
            continue;
        }
        do {
            if (!strcmp(fda.cFileName, ".") || !strcmp(fda.cFileName, ".."))
                continue;
            if (fda.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                dbgprintf("<Found directory>");
                path_list = (char**)realloc(path_list, sizeof(char*) * (path_list_count + 1));
                len = _scprintf("%.*s\\%s\\*", strlen(path_list[i]) - 2, path_list[i], fda.cFileName);
                temp = (char*)malloc(len + 1);
                sprintf(temp, "%.*s\\%s\\*", strlen(path_list[i]) - 1, path_list[i], fda.cFileName);
                path_list[path_list_count] = temp;
                dbgprintf("  <%s>", path_list[path_list_count]);
                path_list_count++;
                temp = NULL;
                continue;
            }
            if (i) {
                len = _scprintf("%s%s%.*s%s", base_name ? base_name : "", base_name ? "\\" : "", strlen(path_list[i] + strlen(foldername) + 1) - 1, path_list[i] + strlen(foldername) + 1, fda.cFileName);
                temp = (char*)malloc(len + 1);
                sprintf(temp, "%s%s%.*s%s", base_name ? base_name : "", base_name ? "\\" : "", strlen(path_list[i] + strlen(foldername) + 1) - 1, path_list[i] + strlen(foldername) + 1, fda.cFileName);
            }
            else {
                len = _scprintf("%s%s%s", base_name ? base_name : "", base_name ? "\\" : "", fda.cFileName);
                temp = (char*)malloc(len + 1);
                sprintf(temp, "%s%s%s", base_name ? base_name : "", base_name ? "\\" : "", fda.cFileName);
            }
            len = _scprintf("%.*s%s", strlen(path_list[i]) - 1, path_list[i], fda.cFileName);
            path = (char*)malloc(len + 1);
            sprintf(path, "%.*s%s", strlen(path_list[i]) - 1, path_list[i], fda.cFileName);
            dbgprintf(" TEMP=<%s>", temp);
            dbgprintf(" PATH=<%s>", path);
            isreplace = 0;
            e = _mrs_add_file(mrs, path, temp, reserved, on_dupe, 0, &f, on_dupe == MRSDB_KEEP_NEW ? &isreplace : NULL, on_dupe == MRSDB_KEEP_NEW ? &ridx : NULL);
            free(temp);
            free(path);
            temp = path = NULL;
            if (e) {
                dbgprintf("   Error -> %u", e);
                for (i = 0; i < path_list_count; i++)
                    free(path_list[i]);
                free(path_list);
                _mrs_replace_index_list_free(&ridxl);
                _mrs_files_destroy(&files, 1);
                return e;
            }
            else {
                if (on_dupe == MRSDB_KEEP_NEW && isreplace)
                    _mrs_replace_index_list_add(&ridxl, ridx, ridxl.cnt);
                _mrs_files_append(&files, &f);
            }
        } while (FindNextFileA(h, &fda));
        i++;
    }
    
    for (i = 0;i < path_list_count;i++)
        free(path_list[i]);
    free(path_list);

    dbgprintf("We got %u files", files.count);
    dbgprintf("%u files need to be replaced", ridxl.cnt);

    for (i = 0; i < files.count; i++) {
        dbgprintf("[%s]", files.files[i].dh.filename);
        if (on_dupe == MRSDB_KEEP_NEW && ridxl.cnt) {
            if (!_mrs_replace_index_list_do_replace(&ridxl, mrs, files.files, files.count, i))
                continue;
        }
        _mrs_push_file(mrs, files.files[i]);
    }

    _mrs_replace_index_list_free(&ridxl);
    _mrs_files_destroy(&files, 0);

    return MRSE_OK;
}

int _mrs_add_mrs(MRS* mrs, const char* mrsname, char* base_name, void* reserved, enum mrs_dupe_behavior_t on_dupe) {
    FILE* fp;
    unsigned i;
    struct mrs_hdr_t hdr;
    struct mrs_encryption_t decrypt;
    struct mrs_files_t ff;
    struct mrs_file_t f;
    unsigned char* temp;
    unsigned char* temp2;
    unsigned char* dhbuf;
    unsigned dup;
    unsigned dup_index;
    struct mrs_replace_index_list_t ridxl;

    if (base_name) {
        if (_is_valid_input_filename(base_name)) {
            dbgprintf("Invalid filename");
            return MRSE_INVALID_FILENAME;
        }
    }

    dbgprintf("Let's read a mrs file: \"%s\"", mrsname);

    fp = fopen(mrsname, "rb");
    if (!fp) {
        dbgprintf("\"%s\" not found", mrsname);
        return MRSE_NOT_FOUND;
    }

    decrypt.base_hdr = mrs->_dec.base_hdr ? mrs->_dec.base_hdr : mrs_default_decrypt;
    decrypt.local_hdr = mrs->_dec.local_hdr ? mrs->_dec.local_hdr : decrypt.base_hdr;
    decrypt.central_dir_hdr = mrs->_dec.central_dir_hdr ? mrs->_dec.central_dir_hdr : decrypt.base_hdr;
    decrypt.buffer = mrs->_dec.buffer;

    fseek(fp, -(int)sizeof(struct mrs_hdr_t), SEEK_END);
    fread(&hdr, sizeof(struct mrs_hdr_t), 1, fp);

    decrypt.base_hdr((unsigned char*)&hdr, sizeof(struct mrs_hdr_t));
    dbgprintf("SIG = %08x", hdr.signature);

    if (!mrs_default_signatures(MRSSW_BASE_HDR, hdr.signature) && (!mrs->_sig || !mrs->_sig(MRSSW_BASE_HDR, hdr.signature))) {
        dbgprintf("  Invalid signature!");
        fclose(fp);
        return MRSE_INVALID_MRS;
    }

    dbgprintf("We got %u file(s)", hdr.dir_count);
    _mrs_files_init(&ff);
    _mrs_replace_index_list_init(&ridxl);
    
    dhbuf = (char*)malloc(hdr.dir_size);
    fseek(fp, hdr.dir_offset, SEEK_SET);
    fread(dhbuf, hdr.dir_size, 1, fp);
    decrypt.central_dir_hdr(dhbuf, hdr.dir_size);

    temp = dhbuf;
    for (i = 0; i < hdr.dir_count; i++) {
        memset(&f, 0, sizeof(struct mrs_file_t));
        memcpy(&f.dh.h, temp, sizeof(struct mrs_central_dir_hdr_t));
        dbgprintf("Central header signature = %08x", f.dh.h.signature);

        if (!mrs_default_signatures(MRSSW_CENTRAL_DIR_HDR, f.dh.h.signature) && (!mrs->_sig || !mrs->_sig(MRSSW_CENTRAL_DIR_HDR, f.dh.h.signature))) {
            dbgprintf("Invalid encryption");
            _mrs_replace_index_list_free(&ridxl);
            _mrs_files_destroy(&ff, 1);
            _mrs_file_free(&f);
            free(dhbuf);
            fclose(fp);
            return MRSE_INVALID_ENCRYPTION;
        }

        fseek(fp, f.dh.h.offset, SEEK_SET);
        fread(&f.lh.h, sizeof(struct mrs_local_hdr_t), 1, fp);
        decrypt.local_hdr((unsigned char*)&f.lh.h, sizeof(struct mrs_local_hdr_t));
        dbgprintf("Local header sig = %08x", f.lh.h.signature);
        mrs_local_hdr_dump(&f.lh.h);

        if (!mrs_default_signatures(MRSSW_LOCAL_HDR, f.lh.h.signature) && (!mrs->_sig || !mrs->_sig(MRSSW_LOCAL_HDR, f.lh.h.signature))) {
            dbgprintf("Invalid local header encryption");
            _mrs_replace_index_list_free(&ridxl);
            _mrs_files_destroy(&ff, 1);
            _mrs_file_free(&f);
            free(dhbuf);
            fclose(fp);
            return MRSE_INVALID_ENCRYPTION;
        }

        fseek(fp, f.lh.h.filename_length, SEEK_CUR);

        if (f.lh.h.extra_length) {
            dbgprintf("We have Local extra, let's copy it");
            f.lh.extra = (char*)malloc(f.lh.h.extra_length);
            fread(f.lh.extra, f.lh.h.extra_length, 1, fp);
            decrypt.local_hdr(f.lh.extra, f.lh.h.extra_length);
            temp2 = f.lh.extra;
            f.lh.extra = _mrs_ref_table_append(&mrs->_reftable, temp2, f.lh.h.extra_length);
            free(temp2);
            dbgprintf("Read local header extra: got address %p", f.lh.extra);
        }
        else
            fseek(fp, f.lh.h.extra_length, SEEK_CUR);

        // We update our offset to the beginning of the file buffer
        f.dh.h.offset = ftell(fp);

        temp += sizeof(struct mrs_central_dir_hdr_t);

        //// Reading filename
        if (base_name) {
            f.dh.filename = (char*)malloc(strlen(base_name) + f.dh.h.filename_length + 2);
            memset(f.dh.filename, 0, strlen(base_name) + f.dh.h.filename_length + 2);
            sprintf(f.dh.filename, "%s/", base_name);
        }
        else {
            f.dh.filename = (char*)malloc(f.dh.h.filename_length + 1);
            memset(f.dh.filename, 0, f.dh.h.filename_length + 1);
        }
        strncat(f.dh.filename, temp, f.dh.h.filename_length);

        // Check duplicate
        if (mrs->_hdr.dir_count) {
            temp2 = NULL;
            dup = _mrs_is_duplicate(mrs, f.dh.filename, &temp2, &dup_index);
            if (!dup) {
                switch (on_dupe) {
                case MRSDB_KEEP_NEW:
                    _mrs_replace_index_list_add(&ridxl, dup_index, i);
                    break;
                case MRSDB_KEEP_BOTH:
                    free(f.dh.filename);
                    f.dh.filename = temp2;
                    break;
                case MRSDB_KEEP_OLD:
                    dbgprintf("Duplicate found!");
                    _mrs_replace_index_list_free(&ridxl);
                    _mrs_files_destroy(&ff, 1);
                    _mrs_file_free(&f);
                    free(temp2);
                    free(dhbuf);
                    fclose(fp);
                    return MRSE_DUPLICATE;
                }
            }
        }

        f.lh.filename = f.dh.filename;
        temp += f.dh.h.filename_length;

        //// Reading Central Dir Header "extra" field (if any)
        if (f.dh.h.extra_length) {
            dbgprintf("We have Central Dir extra, let's copy it");
            f.dh.extra = _mrs_ref_table_append(&mrs->_reftable, temp, f.dh.h.extra_length);
        }
        temp += f.dh.h.extra_length;

        //// Reading Central Dir Header "comment" field (if any)
        if (f.dh.h.comment_length) {
            dbgprintf("We have Central Dir comment, let's copy it");
            f.dh.comment = _mrs_ref_table_append(&mrs->_reftable, temp, f.dh.h.comment_length);
        }
        temp += f.dh.h.comment_length;

        ///TODO: Check if compressed file buffer is valid

        _mrs_files_append(&ff, &f);
    }

    free(dhbuf);

    for (i = 0; i < ff.count; i++) {
        dbgprintf("File %u is at offset %08x", i, ff.files[i].dh.h.offset);
        temp = (char*)malloc(ff.files[i].dh.h.compressed_size);
        fseek(fp, ff.files[i].dh.h.offset, SEEK_SET);
        fread(temp, ff.files[i].dh.h.compressed_size, 1, fp);
        // Decrypt the file buffer, if there's a decryption routine for it
        if (decrypt.buffer)
            decrypt.buffer(temp, ff.files[i].dh.h.compressed_size);
        // We finally update our offset to the real offset in our temporary storage
        ff.files[i].dh.h.offset = _mrs_temp_tell(mrs);
        _mrs_temp_write(mrs, temp, ff.files[i].dh.h.compressed_size);
        _strslash(ff.files[i].dh.filename, 0);
        ff.files[i].dh.h.filename_length = strlen(ff.files[i].dh.filename);
        ff.files[i].lh.h.filename_length = ff.files[i].dh.h.filename_length;

        free(temp);
        if (on_dupe == MRSDB_KEEP_NEW && ridxl.cnt) {
            if (!_mrs_replace_index_list_do_replace(&ridxl, mrs, ff.files, ff.count, i))
                continue;
        }
        _mrs_push_file(mrs, ff.files[i]);
    }

    fclose(fp);
    _mrs_replace_index_list_free(&ridxl);
    _mrs_files_destroy(&ff, 0);

    return MRSE_OK;
}

/***************
OLD _mrs_add_mrs FUNCTION

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
                _mrs_file_free(&mrsfile[i]);
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
                _mrs_file_free(&mrsfile[i]);
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
            dbgprintf("Read local header extra: got address %p", mrsfile[i].lh.extra);
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
            mrsfile[i].dh.extra = _mrs_ref_table_append(&mrs->_reftable, temp, mrsfile[i].dh.h.extra_length);
        }
        temp += mrsfile[i].dh.h.extra_length;

        //// Reading Central Dir Header "comment" field (if any)
        if (mrsfile[i].dh.h.comment_length) {
            dbgprintf("We have Central Dir comment, let's copy it");
            mrsfile[i].dh.comment = _mrs_ref_table_append(&mrs->_reftable, temp, mrsfile[i].dh.h.comment_length);
        }
        temp += mrsfile[i].dh.h.comment_length;
    }
    free(dhbuf);
    
    ///TODO: Check for duplicates!!!
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
        mrsfile[i].dh.h.filename_length = strlen(mrsfile[i].dh.filename);
        mrsfile[i].lh.h.filename_length = mrsfile[i].dh.h.filename_length;
        _mrs_push_file(mrs, mrsfile[i]);
        free(temp);
    }

    free(mrsfile);

    fclose(f);

    return MRSE_OK;
}*/

int _mrs_add_mrs2(MRS* mrs, MRS* in, char* base_name, enum mrs_dupe_behavior_t on_dupe){
    size_t cnt;
    size_t i;
    struct mrs_file_t* f;
    int dup;
    unsigned dup_index;
    char* temp;
    struct mrs_replace_index_list_t il;

    _mrs_replace_index_list_init(&il);

    dbgprintf("Let's add files from a MRS handle");

    if(!in)
        return MRSE_INVALID_PARAM;

    cnt = mrs_get_file_count(in);
    dbgprintf("File count: %u", cnt);
    
    if(!cnt)
        return MRSE_EMPTY;

    f = (struct mrs_file_t*)malloc(sizeof(struct mrs_file_t) * cnt);
    memset(f, 0, sizeof(struct mrs_file_t) * cnt);

    for(i=0; i<cnt; i++){
        dbgprintf("Adding file %u...", i);
        dbgprintf("  Filename:  %s", in->_files[i].dh.filename);
        dbgprintf("  File size: %u", in->_files[i].dh.h.compressed_size);
        memcpy(&f[i], &in->_files[i], sizeof(struct mrs_file_t));
        if(in->_files[i].dh.extra)
            f[i].dh.extra = _mrs_ref_table_append(&mrs->_reftable, in->_files[i].dh.extra, in->_files[i].dh.h.extra_length);
        if(in->_files[i].dh.comment)
            f[i].dh.comment = _mrs_ref_table_append(&mrs->_reftable, in->_files[i].dh.comment, in->_files[i].dh.h.comment_length);
        if(in->_files[i].lh.extra)
            f[i].lh.extra = _mrs_ref_table_append(&mrs->_reftable, in->_files[i].lh.extra, in->_files[i].lh.h.extra_length);
        if(base_name){
            f[i].dh.filename = (char*)malloc(strlen(base_name)+strlen(in->_files[i].dh.filename)+2);
            sprintf(f[i].dh.filename, "%s/%s", base_name, in->_files[i].dh.filename);
        }else
            f[i].dh.filename = strdup(in->_files[i].dh.filename);
        f[i].dh.h.filename_length = strlen(f[i].dh.filename);
        f[i].lh.filename = f[i].dh.filename;
        f[i].lh.h.filename_length = f[i].dh.h.filename_length;
        dbgprintf("  Final name: %s", f[i].dh.filename);
    }

    /// Find duplicates
    for(i=0; i<cnt; i++){
        dup = _mrs_is_duplicate(mrs, f[i].dh.filename, NULL, &dup_index);
        if(!dup){
            dbgprintf("\"%s\" is a duplicate, let's take action", f[i].dh.filename);
            switch(on_dupe){
            case MRSDB_KEEP_NEW:
                _mrs_replace_index_list_add(&il, dup_index, i);
                break;
            case MRSDB_KEEP_OLD:
                for(i=0; i<cnt; i++){
                    _mrs_file_free(&f[i]);
                    ///TODO: Free extras and comments from MRS ref table
                }
                _mrs_replace_index_list_free(&il);
                free(f);
                return MRSE_DUPLICATE;
            case MRSDB_KEEP_BOTH:
                _mrs_is_duplicate(mrs, f[i].dh.filename, &temp, NULL);
                free(f[i].dh.filename);
                f[i].dh.filename = temp;
                break;
            }
        }
    }

    for(i=0; i<cnt; i++){
        temp = (char*)malloc(f[i].dh.h.compressed_size);
        _mrs_temp_read(in, temp, f[i].dh.h.offset, f[i].dh.h.compressed_size);
        f[i].dh.h.offset = _mrs_temp_tell(mrs);
        _mrs_temp_write(mrs, temp, f[i].dh.h.compressed_size);
        free(temp);
        if(on_dupe == MRSDB_KEEP_NEW && il.cnt){
            dbgprintf("Searching replace indices...");
            _mrs_replace_index_list_dump(&il);
            if(!_mrs_replace_index_list_do_replace(&il, mrs, f, cnt, i))
                continue;
        }
        _mrs_push_file(mrs, f[i]);
        // _mrs_temp_read(in, temp, f[i].dh.h.offset, f[i].dh.h.compressed_size);
    }
    
    _mrs_replace_index_list_free(&il);
    free(f);

    return MRSE_OK;
}