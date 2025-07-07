/***************************************************************
    libmrs
    Easily manage GunZ: The Duel's .MRS archives
    by Wes (@jwesy0), 2025
***************************************************************/

#define __LIBMRS_INTERNAL__

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <Shlwapi.h>

#include "mrs.h"
#include "mrs_internal.h"
#include "mrs_dbg.h"

extern int _is_valid_output_filename(const char* s);
extern int _strbkslash(char* s, size_t size);
extern int _mkdirs(const char* s);
#ifdef _LIBMRS_DBG
extern void _hex_dump(const unsigned char* buf, size_t size);
extern void mrs_central_dir_hdr_dump(const struct mrs_central_dir_hdr_t* h);
#else
#define _hex_dump(...)
#define mrs_central_dir_hdr_dump(...)
#endif

int _mrs_save_mrs(const MRS* mrs, FILE* f, MRS_PROGRESS_FUNC pcallback);

#define MRS_SAVE_CALLBACK(...) if(pcallback) pcallback(__VA_ARGS__);
int _mrs_save_mrs_fname(const MRS* mrs, const char* output, MRS_PROGRESS_FUNC pcallback){
    char real_output[256];
    FILE* f;
    int e;
    
    GetFullPathNameA(output, 256, real_output, NULL);

    if((PathFileExistsA(real_output) && PathIsDirectoryA(real_output)) || _is_valid_output_filename(real_output))
        return MRSE_INVALID_FILENAME;
    
    f = fopen(real_output, "wb");
    if(!f)
        return MRSE_CANNOT_SAVE;
    
    e = _mrs_save_mrs(mrs, f, pcallback);

    fclose(f);
    
    return e;
}

int _mrs_save_mrs(const MRS* mrs, FILE* f, MRS_PROGRESS_FUNC pcallback){
    // FILE* f;
    struct mrs_hdr_t hdr;
    // char real_output[256];
    struct mrs_file_t* fil;
    unsigned char* temp;
    unsigned i, j;
    struct mrs_encryption_t encrypt;
    double p;

    dbgprintf("Ok let's save this as a MRS file.");

    ///TODO: Make it portable
    // GetFullPathNameA(output, 256, real_output, NULL);

    // if((PathFileExistsA(real_output) && PathIsDirectoryA(real_output)) || _is_valid_output_filename(real_output))
    //     return MRSE_INVALID_FILENAME;

    encrypt.base_hdr  = mrs->_enc.base_hdr ? mrs->_enc.base_hdr : mrs_default_encrypt;
    encrypt.local_hdr = mrs->_enc.local_hdr ? mrs->_enc.local_hdr : encrypt.base_hdr;
    encrypt.central_dir_hdr = mrs->_enc.central_dir_hdr ? mrs->_enc.central_dir_hdr : encrypt.base_hdr;
    encrypt.buffer = mrs->_enc.buffer;
        
    memcpy(&hdr, &mrs->_hdr, sizeof(struct mrs_hdr_t));
    
    dbgprintf("We got %u files", hdr.dir_count);
    // f = fopen(real_output, "wb");
    // if(!f)
    //     return MRSE_CANNOT_SAVE;

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
        dbgprintf("%s dump", fil[i].dh.filename);
        mrs_central_dir_hdr_dump(&fil[i].dh);

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