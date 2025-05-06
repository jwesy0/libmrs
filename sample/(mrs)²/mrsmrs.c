/***************************************************************
    (mrs)²
    "mrs.exe" clone written using libmrs.
    by Wes (@jwesy0), 2025
***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mrs.h"

#define PROGRAM_NAME "mrsmrs.exe"

typedef int (*mrsmrs_func)(const char*);

struct mrsmrs_opt_t{
    char*       opt;
    mrsmrs_func f;
};

int mrsmrs_compile(const char* fname){
    MRS* mrs;
    char f[256];
    int e;

    mrs = mrs_init();

    mrs_add(mrs, MRSA_FOLDER, fname, NULL, 0);

    sprintf(f, "%s.mrs", fname);

    e = mrs_save(mrs, MRSS_MRS, f, NULL);
    switch(e){
    case MRSE_INVALID_FILENAME:
        fprintf(stderr, "Invalid output filename: \"%s\"\n", f);
        mrs_free(mrs);
        return 23;
    case MRSE_CANNOT_SAVE:
        fprintf(stderr, "Can't open output file\n");
        mrs_free(mrs);
        return 24;
    }

    mrs_free(mrs);
    
    return 0;
}

void mrsmrs_decompile_callback(double p, unsigned i, unsigned total, enum mrs_progress_t a, const void* param){
    switch(a){
    case MRSP_BEGIN:
        printf("[%s]", (char*)param);
        break;
    case MRSP_END:
        printf("\n");
        break;
    case MRSP_ERROR:
        printf(" --> Fail\n");
        break;
    }
}

int mrsmrs_decompile(const char* fname){
    int e;
    
    e = mrs_global_decompile(fname, NULL, NULL, NULL, mrsmrs_decompile_callback);
    
    switch(e){
    case MRSE_CANNOT_OPEN:
        fprintf(stderr, "Can't open input file\n");
        return 22;
    case MRSE_INVALID_FILENAME:
        fprintf(stderr, "Invalid output filename for: \"%s\"\n", fname);
        return 23;
    case MRSE_INVALID_MRS:
        fprintf(stderr, "Invalid mrs file or invalid encryption\n");
        return 21;
    case MRSE_INVALID_ENCRYPTION:
        fprintf(stderr, "Invalid encryption\n");
        return 21;
    case MRSE_CANNOT_SAVE:
        fprintf(stderr, "Can't open output file\n");
        return 24;
    }
    
    return 0;
}

int mrsmrs_list(const char* fname){
    MRSFILE l;
    int e;
    
    e = mrs_global_list(fname, NULL, NULL, &l);
    if(e == MRSE_NOT_FOUND){
        fprintf(stderr, "Can't open input file\n");
        return 22;
    }else if(e == MRSE_INVALID_MRS || e == MRSE_INVALID_ENCRYPTION){
        fprintf(stderr, "Invalid mrs file or invalid encryption\n");
        return 21;
    }else if(e == MRSE_EMPTY){
        fprintf(stderr, "Empty mrs file\n");
        return 0;
    }
    
    do{
        printf("%u %s\n", l->size, l->name);
    }while((e = mrs_global_list_next(l)) == MRSE_OK);
    
    if(e == MRSE_INVALID_ENCRYPTION){
        fprintf(stderr, "Error: Invalid encryption, cannot continue\n");
        return 21;
    }
    
    mrs_global_list_free(l);
    
    return 0;
}

int mrsmrs_z(const char* ignore){
    return 57;
}

void mrsmrs_help(){
    printf("\n");
    printf("  .mrs files packer/unpacker v1.0 (c) by jwes, 2025\n");
    printf("                   based on \"mrs.exe\" by wad, 2005\n\n");
    printf("  Usage: " PROGRAM_NAME " d|c filename\n");
    printf("                d - decompress archive to directory\n");
    printf("                c - compress directory to archive\n");
    printf("  Examples:\n");
    printf("    " PROGRAM_NAME " d system.mrs\n");
    printf("       (unpack system.mrs to directory ./system/ )\n");
    printf("    " PROGRAM_NAME " c system\n");
    printf("       (make system.mrs and pack files ./system/*.* to it)\n");
    printf("  Warning:\n");
    printf("    Don't leave unpacked folder and/or backup files in the GunZ directory!\n\n");
}

int main(int argc, char* argv[]){
    unsigned i;
    struct mrsmrs_opt_t opts[] = {
        {"a", mrsmrs_compile},   // Alias for 'c', compile
        {"c", mrsmrs_compile},   // Compile
        {"d", mrsmrs_decompile}, // Decompile
        {"e", mrsmrs_decompile}, // Alias for 'd', decompile
        {"l", mrsmrs_list},      // List files in a mrs archive, included in original "mrs.exe" but not documented on help (why?)
        {"x", mrsmrs_decompile}, // Alias for 'd', decompile
        {"z", mrsmrs_z}          // Included in original "mrs.exe", but apparently it doesn't do anything? Only returns '57'
    };
    size_t opts_count = sizeof(opts) / sizeof(struct mrsmrs_opt_t);
    
    if(argc <= 2){
        mrsmrs_help();
        return 1;
    }
    
    for(i=0; i<opts_count; i++){
        if(!stricmp(argv[1], opts[i].opt))
            return opts[i].f(argv[2]);
    }
    
    printf("Unknown option\n");
    
    return 15;
}
