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

#include "mrs.h"
#include "mrs_internal.h"
#include "mrs_dbg.h"
#include "zlib.h"
#include "zconf.h"

extern int _mrs_is_initialized(const MRS* mrs);
extern int _mrs_is_duplicate(const MRS* mrs, const char* s, char** s_out, unsigned* match_index);
extern int _is_valid_input_filename(const char* s);
extern int _compress_file(unsigned char* inbuf, size_t total_in, unsigned char** outbuf, size_t* total_out);
extern int _mrs_temp_write(MRS* mrs, unsigned char* buf, size_t size);
extern off_t _mrs_temp_tell(MRS* mrs);
extern int _mrs_temp_read(MRS* mrs, unsigned char* buf, off_t offset, size_t size);
extern int _mrs_replace_file(MRS* mrs, struct mrs_file_t* oldf, struct mrs_file_t* newf);
extern void _mrs_push_file(MRS* mrs, const struct mrs_file_t f);
extern void _mrs_file_free(MRS* mrs, struct mrs_file_t* f);
extern unsigned char* _mrs_ref_table_append(struct mrs_ref_table_t* r, const unsigned char* s, size_t len);
extern int _strslash(char* s, size_t size);
extern void _mrs_ref_table_init(struct mrs_ref_table_t* r);
extern unsigned char* _mrs_ref_table_append(struct mrs_ref_table_t* r, const unsigned char* s, size_t len);
extern int _mrs_ref_table_free(struct mrs_ref_table_t* r, unsigned char* s);
extern void _mrs_replace_index_list_dump(const struct mrs_replace_index_list_t* il);
extern void _mrs_replace_index_list_init(struct mrs_replace_index_list_t* il);
extern void _mrs_replace_index_list_add(struct mrs_replace_index_list_t* il, unsigned oldi, unsigned newi);
extern int _mrs_replace_index_list_do_replace(struct mrs_replace_index_list_t* il, MRS* mrs, const struct mrs_file_t* f, size_t fcount, unsigned index);
extern void _mrs_replace_index_list_free(struct mrs_replace_index_list_t* il);

#ifdef _LIBMRS_DBG
extern void mrs_file_dump(const struct mrs_file_t* f);
extern void mrs_local_hdr_dump(const struct mrs_local_hdr_t* h);
#else
#define mrs_file_dump(...)
#define mrs_local_hdr_dump(...)
#endif

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
    if(dup == -1 && on_dupe == MRSDB_KEEP_NEW){
        dbgprintf("There was an old file with the same name, we are replacing it!");
        _mrs_replace_file(mrs, &mrs->_files[dup_index], &f);
    }else{
        _mrs_push_file(mrs, f);
    }

    mrs_file_dump(&f);

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
    
    if(!PathFileExistsA(name)){
        dbgprintf("Folder \"%s\" does not exist", name);
        return MRSE_NOT_FOUND;
    }

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
            /*temp2 = (char*)malloc(mrsfile[i].dh.h.extra_length);
            memcpy(mrsfile[i].dh.extra, temp, mrsfile[i].dh.h.extra_length);*/
            mrsfile[i].dh.extra = _mrs_ref_table_append(&mrs->_reftable, temp, mrsfile[i].dh.h.extra_length);
        }
        temp += mrsfile[i].dh.h.extra_length;

        //// Reading Central Dir Header "comment" field (if any)
        if (mrsfile[i].dh.h.comment_length) {
            dbgprintf("We have Central Dir comment, let's copy it");
            /*mrsfile[i].dh.comment = (char*)malloc(mrsfile[i].dh.h.comment_length);
            memcpy(mrsfile[i].dh.comment, temp, mrsfile[i].dh.h.comment_length);*/
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
}

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
                    _mrs_file_free(mrs, &f[i]);
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